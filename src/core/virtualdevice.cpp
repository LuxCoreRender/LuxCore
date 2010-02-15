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

namespace luxrays {
	
VirtualM2OIntersectionDevice::VirtualM2OIntersectionDevice(const size_t count, IntersectionDevice *device) {
	virtualDeviceCount = count;
	realDevice = device;

	virtualDeviceInstances = new VirtualM2ODevInstance *[virtualDeviceCount];
	for (size_t i = 0; i < virtualDeviceCount; ++i)
		virtualDeviceInstances[i] = new VirtualM2ODevInstance(this, i);

	// Start the RayBuffer routing thread
	routerThread = new boost::thread(boost::bind(VirtualM2OIntersectionDevice::RayBufferRouter, this));
}

VirtualM2OIntersectionDevice::~VirtualM2OIntersectionDevice() {
	// Stop the router thread
	routerThread->interrupt();
	routerThread->join();

	for (size_t i = 0; i < virtualDeviceCount; ++i)
		delete virtualDeviceInstances[i];
	delete virtualDeviceInstances;
}

IntersectionDevice *VirtualM2OIntersectionDevice::GetVirtualDevice(size_t index) {
	return virtualDeviceInstances[index];
}

void VirtualM2OIntersectionDevice::RayBufferRouter(VirtualM2OIntersectionDevice *virtualDevice) {
	try {
		while (!boost::this_thread::interruption_requested()) {
			RayBuffer *rayBuffer = virtualDevice->realDevice->PopRayBuffer();

			size_t instanceIndex = rayBuffer->GetUserData();
			virtualDevice->virtualDeviceInstances[instanceIndex]->PushRayBufferDone(rayBuffer);
		}
	} catch (boost::thread_interrupted) {
		// Time to exit
	}
}

//------------------------------------------------------------------------------
// VirtualM2OIntersectionDevice class
//------------------------------------------------------------------------------

VirtualM2OIntersectionDevice::VirtualM2ODevInstance::VirtualM2ODevInstance(
	VirtualM2OIntersectionDevice *device, const size_t index) :
			IntersectionDevice(device->realDevice->GetContext(), device->realDevice->GetType()) {
	char buf[64];
	sprintf(buf, "VirtualDevice-%03d-%s", (int)index, device->realDevice->GetName().c_str());
	deviceName = std::string(buf);

	instanceIndex = index;
	virtualDevice = device;
}

VirtualM2OIntersectionDevice::VirtualM2ODevInstance::~VirtualM2ODevInstance() {
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::PushRayBufferDone(RayBuffer *rayBuffer) {
	doneRayBufferQueue.Push(rayBuffer);
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::SetDataSet(const DataSet *newDataSet) {
	throw std::runtime_error("Internal error: called VirtualM2OIntersectionDevice::VirtualM2ODevInstance::SetDataSet()");
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::Start() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	IntersectionDevice::Start();

	// Start the real device if required
	if (!virtualDevice->realDevice->IsRunning()) {
		LR_LOG(deviceContext, "[Virtual device::" << deviceName << "] Starting real device");
		virtualDevice->realDevice->Start();
	}
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::Stop() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	IntersectionDevice::Stop();

	// Start the real device if required

	// Need to wait for all my pending RayBuffer
	size_t count = rayBufferUserData.size();
	for (size_t i = 0; i < count; ++i)
		IntersectionDevice::PopRayBuffer();

	// Check if I'm the last one running
	bool lastOne = true;
	for (size_t i = 0; i < virtualDevice->virtualDeviceCount; ++i) {
		if ((i != instanceIndex) && (virtualDevice->virtualDeviceInstances[i]->IsRunning())) {
			lastOne = false;
			break;
		}

	}

	if (lastOne) {
		LR_LOG(deviceContext, "[Virtual device::" << deviceName << "] Stopping real device");
		virtualDevice->realDevice->Stop();
	}
}

RayBuffer *VirtualM2OIntersectionDevice::VirtualM2ODevInstance::NewRayBuffer() {
	return virtualDevice->realDevice->NewRayBuffer();
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::PushRayBuffer(RayBuffer *rayBuffer) {
	// Replace the RayBuffer UserData with my index
	rayBufferUserData.push_back(rayBuffer->GetUserData());
	rayBuffer->SetUserData(instanceIndex);

	virtualDevice->realDevice->PushRayBuffer(rayBuffer);
}

RayBuffer *VirtualM2OIntersectionDevice::VirtualM2ODevInstance::PopRayBuffer() {
	RayBuffer *rayBuffer = IntersectionDevice::PopRayBuffer();

	// Restore the RayBuffer UserData
	rayBuffer->SetUserData(rayBufferUserData.front());
	rayBufferUserData.pop_front();

	return rayBuffer;
}

}
