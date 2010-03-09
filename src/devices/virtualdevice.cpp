/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#include "luxrays/core/device.h"
#include "luxrays/core/virtualdevice.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// Virtual Many To One Hardware Device
//------------------------------------------------------------------------------

size_t VirtualM2OHardwareIntersectionDevice::RayBufferSize = OpenCLIntersectionDevice::RayBufferSize;

VirtualM2OHardwareIntersectionDevice::VirtualM2OHardwareIntersectionDevice(const size_t count,
		HardwareIntersectionDevice *device) {
	virtualDeviceCount = count;
	realDevice = device;
	realDevice->SetExternalRayBufferQueue(&rayBufferQueue);

	virtualDeviceInstances = new VirtualM2ODevHInstance *[virtualDeviceCount];
	for (size_t i = 0; i < virtualDeviceCount; ++i)
		virtualDeviceInstances[i] = new VirtualM2ODevHInstance(this, i);
}

VirtualM2OHardwareIntersectionDevice::~VirtualM2OHardwareIntersectionDevice() {
	for (size_t i = 0; i < virtualDeviceCount; ++i)
		delete virtualDeviceInstances[i];
	delete virtualDeviceInstances;
}

IntersectionDevice *VirtualM2OHardwareIntersectionDevice::GetVirtualDevice(size_t index) {
	return virtualDeviceInstances[index];
}

//------------------------------------------------------------------------------
// VirtualM2ODevHInstance class
//------------------------------------------------------------------------------

VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::VirtualM2ODevHInstance(
	VirtualM2OHardwareIntersectionDevice *device, const size_t index) :
			IntersectionDevice(device->realDevice->GetContext(), DEVICE_TYPE_VIRTUAL, index) {
	char buf[256];
	sprintf(buf, "VirtualM2OHDevice-%03d-%s", (int)index, device->realDevice->GetName().c_str());
	deviceName = std::string(buf);

	instanceIndex = index;
	virtualDevice = device;
}

VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::~VirtualM2ODevHInstance() {
	if (started)
		Stop();
}

void VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::SetDataSet(const DataSet *newDataSet) {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);
	IntersectionDevice::SetDataSet(newDataSet);

	// Set the real device data set if it is a new one
	if (virtualDevice->realDevice->GetDataSet() != newDataSet)
		virtualDevice->realDevice->SetDataSet(newDataSet);
}

void VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::Start() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	IntersectionDevice::Start();
	pendingRayBuffers = 0;

	// Start the real device if required
	if (!virtualDevice->realDevice->IsRunning()) {
		LR_LOG(deviceContext, "[VirtualM2ODevice::" << deviceName << "] Starting real device");
		virtualDevice->realDevice->Start();
	}
}

void VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::Interrupt() {
}

void VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::Stop() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	// Need to wait for all my pending RayBuffer
	while (pendingRayBuffers > 0)
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
		LR_LOG(deviceContext, "[VirtualM2ODevice::" << deviceName << "] Stopping real device");
		virtualDevice->realDevice->Stop();
	}

	IntersectionDevice::Stop();
}

RayBuffer *VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::PushRayBuffer(RayBuffer *rayBuffer) {
	virtualDevice->rayBufferQueue.PushToDo(rayBuffer, instanceIndex);
	++pendingRayBuffers;
}

RayBuffer *VirtualM2OHardwareIntersectionDevice::VirtualM2ODevHInstance::PopRayBuffer() {
	RayBuffer *rayBuffer = virtualDevice->rayBufferQueue.PopDone(instanceIndex);
	--pendingRayBuffers;

	statsTotalRayCount += rayBuffer->GetRayCount();

	return rayBuffer;
}

//------------------------------------------------------------------------------
// Virtual Many To Many Hardware Device
//------------------------------------------------------------------------------

size_t VirtualM2MHardwareIntersectionDevice::RayBufferSize = OpenCLIntersectionDevice::RayBufferSize;

VirtualM2MHardwareIntersectionDevice::VirtualM2MHardwareIntersectionDevice(const size_t count,
		const std::vector<HardwareIntersectionDevice *> &devices) : rayBufferQueue(count) {
	assert (count > 0);
	assert (devices.size() > 0);

	// Set the queue for all hardware device
	realDevices = devices;
	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->SetExternalRayBufferQueue(&rayBufferQueue);

	virtualDeviceCount = count;
	virtualDeviceInstances = new VirtualM2MDevHInstance *[virtualDeviceCount];
	for (size_t i = 0; i < virtualDeviceCount; ++i)
		virtualDeviceInstances[i] = new VirtualM2MDevHInstance(this, i);
}

VirtualM2MHardwareIntersectionDevice::~VirtualM2MHardwareIntersectionDevice() {
	for (size_t i = 0; i < virtualDeviceCount; ++i)
		delete virtualDeviceInstances[i];
	delete virtualDeviceInstances;
}

IntersectionDevice *VirtualM2MHardwareIntersectionDevice::GetVirtualDevice(size_t index) {
	return virtualDeviceInstances[index];
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
		if (virtualDevice->realDevices[i]->GetDataSet() != newDataSet)
			virtualDevice->realDevices[i]->SetDataSet(newDataSet);
	}
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::Start() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	IntersectionDevice::Start();
	pendingRayBuffers = 0;

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
	while (pendingRayBuffers > 0)
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
			LR_LOG(deviceContext, "[VirtualM2ODevice::" << deviceName << "] Stopping real device: " << i);
			virtualDevice->realDevices[i]->Stop();
		}
	}

	IntersectionDevice::Stop();
}

RayBuffer *VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::PushRayBuffer(RayBuffer *rayBuffer) {
	virtualDevice->rayBufferQueue.PushToDo(rayBuffer, instanceIndex);
	++pendingRayBuffers;
}

RayBuffer *VirtualM2MHardwareIntersectionDevice::VirtualM2MDevHInstance::PopRayBuffer() {
	RayBuffer *rayBuffer = virtualDevice->rayBufferQueue.PopDone(instanceIndex);
	--pendingRayBuffers;

	statsTotalRayCount += rayBuffer->GetRayCount();

	return rayBuffer;
}
