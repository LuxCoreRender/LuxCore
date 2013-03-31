/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <cstdio>
#include <boost/foreach.hpp>

#include "luxrays/core/virtualdevice.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// Virtual Many To Many Hardware Device
//------------------------------------------------------------------------------

size_t VirtualM2MHardwareIntersectionDevice::RayBufferSize = RAYBUFFER_SIZE;

VirtualM2MHardwareIntersectionDevice::VirtualM2MHardwareIntersectionDevice(const size_t count,
		const std::vector<HardwareIntersectionDevice *> &devices) {
	assert (devices.size() > 0);

	realDevices = devices;
	virtualDeviceCount = count;
	
	BOOST_FOREACH(HardwareIntersectionDevice *device, realDevices) {
		device->SetQueueCount(virtualDeviceCount);
		device->SetBufferCount(1);
	}

	for (size_t i = 0; i < virtualDeviceCount; ++i)
		virtualDeviceInstances.push_back(new VirtualM2MDevHInstance(this, i));
}

VirtualM2MHardwareIntersectionDevice::~VirtualM2MHardwareIntersectionDevice() {
	std::vector<VirtualM2MDevHInstance *> devs = virtualDeviceInstances;

	// The virtual devices must always be removed in reverse order
	for (size_t i = 0; i < devs.size(); ++i)
		RemoveVirtualDevice(devs[devs.size() - i - 1]);

	for (size_t i = 0; i < realDevices.size(); ++i)
		delete realDevices[i];
}

IntersectionDevice *VirtualM2MHardwareIntersectionDevice::GetVirtualDevice(size_t index) {
	return virtualDeviceInstances[index];
}

IntersectionDevice *VirtualM2MHardwareIntersectionDevice::AddVirtualDevice() {
	VirtualM2MDevHInstance *dev;

	{
		boost::mutex::scoped_lock lock(virtualDeviceMutex);

		dev = new VirtualM2MDevHInstance(this, virtualDeviceInstances.size());
		virtualDeviceInstances.push_back(dev);
		++virtualDeviceCount;
	}

	// I assume all real devices have the same DataSet and state
	const Context *ctx = realDevices[0]->deviceContext;

	if (ctx->GetCurrentDataSet())
		dev->SetDataSet(ctx->GetCurrentDataSet());

	if (ctx->IsRunning())
		dev->Start();

	return dev;
}

void VirtualM2MHardwareIntersectionDevice::RemoveVirtualDevice(IntersectionDevice *d) {
	VirtualM2MDevHInstance *dev = (VirtualM2MDevHInstance *)d;
	
	{
		boost::mutex::scoped_lock lock(virtualDeviceMutex);

		virtualDeviceInstances.erase(virtualDeviceInstances.begin() + dev->instanceIndex);
		--virtualDeviceCount;
	}

	delete dev;
}

//------------------------------------------------------------------------------
// VirtualM2MDevHInstance class
//------------------------------------------------------------------------------

VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::VirtualM2MDevHInstance(
	VirtualM2MHardwareIntersectionDevice *device, const size_t index) :
			IntersectionDevice(device->realDevices[0]->GetContext(), DEVICE_TYPE_VIRTUAL, index) {
	char buf[256];
	sprintf(buf, "VirtualM2MHDevice-%03d", (int)index);
	deviceName = std::string(buf);

	instanceIndex = index;
	virtualDevice = device;

	traceRayRealDeviceIndex = 0;
}

VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::~VirtualM2MDevHInstance() {
	if (started)
		Stop();
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::SetDataSet(const DataSet *newDataSet) {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);
	IntersectionDevice::SetDataSet(newDataSet);

	// Set the real devices data set if it is a new one
	for (size_t i = 0; i < virtualDevice->realDevices.size(); ++i) {
		const DataSet *oldDataSet = virtualDevice->realDevices[i]->GetDataSet();

		if ((oldDataSet == NULL) || !oldDataSet->IsEqual(newDataSet))
			virtualDevice->realDevices[i]->SetDataSet(newDataSet);
	}
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::Start() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	IntersectionDevice::Start();
	pendingRayBufferDeviceIndex.resize(0);

	// Start the real devices if required
	for (size_t i = 0; i < virtualDevice->realDevices.size(); ++i) {
		if (!virtualDevice->realDevices[i]->IsRunning()) {
			LR_LOG(deviceContext, "[VirtualM2MHDevice::" << deviceName << "] Starting real device: " << i);
			virtualDevice->realDevices[i]->Start();
		}
	}

}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::Interrupt() {
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::Stop() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	// Need to wait for all my pending RayBuffer
	while (pendingRayBufferDeviceIndex.size() > 0)
		PopRayBuffer();

	// Check if I'm the last one running
	bool lastOne = true;
	for (size_t i = 0; i < virtualDevice->virtualDeviceCount; ++i) {
		if ((i != instanceIndex) && (virtualDevice->virtualDeviceInstances[i]->IsRunning())) {
			lastOne = false;
			break;
		}
	}

	if (lastOne) {
		for (size_t i = 0; i < virtualDevice->realDevices.size(); ++i) {
			LR_LOG(deviceContext, "[VirtualM2MDevice::" << deviceName << "] Stopping real device: " << i);
			virtualDevice->realDevices[i]->Stop();
		}
	}

	IntersectionDevice::Stop();
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::SetQueueCount(const u_int count) {
	if (count != 1)
		throw std::runtime_error("Error in VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::SetQueueCount(): virtual device supports only 1 queue");
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::SetBufferCount(const u_int count) {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	BOOST_FOREACH(HardwareIntersectionDevice *dev, virtualDevice->realDevices)
		dev->SetBufferCount(count);
}

RayBuffer *VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::NewRayBuffer() {
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::NewRayBuffer(const size_t size) {
	return new RayBuffer(size);
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::PushRayBuffer(
	RayBuffer *rayBuffer, const u_int queueIndex) {
	if (queueIndex != 0)
		throw std::runtime_error("Error in VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::PushRayBuffer(): "
				"virtual devices can be used only with a single queue");

	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	// Look for the hardware device with less pending work
	u_int deviceIndex = 0;
	size_t minCount = virtualDevice->realDevices[0]->GetQueueSize();
	for (u_int i = 1; i < virtualDevice->realDevices.size(); ++i) {
		const size_t count = virtualDevice->realDevices[i]->GetQueueSize();
		if (count < minCount) {
			deviceIndex = i;
			minCount = count;
		}
	}

	virtualDevice->realDevices[deviceIndex]->PushRayBuffer(rayBuffer, instanceIndex);
	pendingRayBufferDeviceIndex.push_front(deviceIndex);
}

RayBuffer *VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::PopRayBuffer(const u_int queueIndex) {
	if (queueIndex != 0)
		throw std::runtime_error("Error in VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::PopRayBuffer(): "
				" virtual devices can be used only with a single queue");

	const u_int deviceIndex = pendingRayBufferDeviceIndex.back();
	pendingRayBufferDeviceIndex.pop_back();

	RayBuffer *rayBuffer = virtualDevice->realDevices[deviceIndex]->PopRayBuffer(instanceIndex);

	statsTotalDataParallelRayCount += rayBuffer->GetRayCount();

	return rayBuffer;
}
