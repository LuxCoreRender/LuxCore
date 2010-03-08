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
// Virtual Many To One Device
//------------------------------------------------------------------------------

size_t VirtualM2OIntersectionDevice::RayBufferSize = OpenCLIntersectionDevice::RayBufferSize;

VirtualM2OIntersectionDevice::VirtualM2OIntersectionDevice(const size_t count,
		IntersectionDevice *device) {
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
	} catch (cl::Error err) {
		LR_LOG(virtualDevice->realDevice->GetContext(), "[VirtualM2ODevice::" << virtualDevice->realDevice->GetName() <<
				"] RayBufferRouter thread ERROR: " << err.what() << "(" << err.err() << ")");
	}
}

//------------------------------------------------------------------------------
// VirtualM2OIntersectionDevice class
//------------------------------------------------------------------------------

VirtualM2OIntersectionDevice::VirtualM2ODevInstance::VirtualM2ODevInstance(
	VirtualM2OIntersectionDevice *device, const size_t index) :
			IntersectionDevice(device->realDevice->GetContext(), DEVICE_TYPE_VIRTUAL, index) {
	char buf[256];
	sprintf(buf, "VirtualM2ODevice-%03d-%s", (int)index, device->realDevice->GetName().c_str());
	deviceName = std::string(buf);

	instanceIndex = index;
	virtualDevice = device;
}

VirtualM2OIntersectionDevice::VirtualM2ODevInstance::~VirtualM2ODevInstance() {
	if (started)
		Stop();
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::SetDataSet(const DataSet *newDataSet) {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);
	IntersectionDevice::SetDataSet(newDataSet);

	// Set the real device data set if it is a new one
	if (virtualDevice->realDevice->GetDataSet() != newDataSet)
		virtualDevice->realDevice->SetDataSet(newDataSet);
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::Start() {
	boost::mutex::scoped_lock lock(virtualDevice->virtualDeviceMutex);

	IntersectionDevice::Start();
	pendingRayBuffers = 0;

	// Start the real device if required
	if (!virtualDevice->realDevice->IsRunning()) {
		LR_LOG(deviceContext, "[VirtualM2ODevice::" << deviceName << "] Starting real device");
		virtualDevice->realDevice->Start();
	}

}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::Interrupt() {
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::Stop() {
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

RayBuffer *VirtualM2OIntersectionDevice::VirtualM2ODevInstance::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::PushRayBuffer(RayBuffer *rayBuffer) {
	rayBuffer->PushUserData(instanceIndex);

	virtualDevice->realDevice->PushRayBuffer(rayBuffer);
	++pendingRayBuffers;
}

RayBuffer *VirtualM2OIntersectionDevice::VirtualM2ODevInstance::PopRayBuffer() {
	RayBuffer *rayBuffer = doneRayBufferQueue.Pop();
	rayBuffer->PopUserData();
	--pendingRayBuffers;

	statsTotalRayCount += rayBuffer->GetRayCount();

	return rayBuffer;
}

void VirtualM2OIntersectionDevice::VirtualM2ODevInstance::PushRayBufferDone(RayBuffer *rayBuffer) {
	doneRayBufferQueue.Push(rayBuffer);
}

//------------------------------------------------------------------------------
// Virtual One to Many device
//------------------------------------------------------------------------------

size_t VirtualO2MIntersectionDevice::RayBufferSize = OpenCLIntersectionDevice::RayBufferSize;

VirtualO2MIntersectionDevice::VirtualO2MIntersectionDevice(std::vector<IntersectionDevice *> devices,
		const size_t index) : IntersectionDevice(devices[0]->GetContext(), DEVICE_TYPE_VIRTUAL, index) {
	assert (devices.size() > 0);

	char buf[256];
	sprintf(buf, "VirtualO2MDevice-%03d", (int)deviceIndex);
	deviceName = std::string(buf);

	realDevices = devices;
	pusherThread = NULL;
}

VirtualO2MIntersectionDevice::~VirtualO2MIntersectionDevice() {
	if (started)
		Stop();
}

void VirtualO2MIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);

	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->SetDataSet(newDataSet);
}

void VirtualO2MIntersectionDevice::Start() {
	IntersectionDevice::Start();

	// Start all real devices
	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->Start();

	// Start the RayBuffer routing threads
	pusherThread = new boost::thread(boost::bind(VirtualO2MIntersectionDevice::PusherRouter, this));
	popperThread.resize(realDevices.size(), NULL);
	for (size_t i = 0; i < realDevices.size(); ++i)
		popperThread[i] = new boost::thread(boost::bind(VirtualO2MIntersectionDevice::PopperRouter, this, i));
}

void VirtualO2MIntersectionDevice::Interrupt() {
	if (pusherThread)
		pusherThread->interrupt();
	if (popperThread.size() > 0) {
		for (size_t i = 0; i < realDevices.size(); ++i)
			popperThread[i]->interrupt();
	}

	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->Interrupt();
}

void VirtualO2MIntersectionDevice::Stop() {
	if (pusherThread) {
		pusherThread->interrupt();
		pusherThread->join();
		delete pusherThread;
		pusherThread = NULL;
	}

	if (popperThread.size() > 0) {
		for (size_t i = 0; i < realDevices.size(); ++i) {
			popperThread[i]->interrupt();
			popperThread[i]->join();
			delete popperThread[i];
		}

		popperThread.clear();
	}

	if (started) {
		for (size_t i = 0; i < realDevices.size(); ++i)
			realDevices[i]->Stop();
	}

	IntersectionDevice::Stop();
}

RayBuffer *VirtualO2MIntersectionDevice::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void VirtualO2MIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	todoRayBufferQueue.Push(rayBuffer);
}

RayBuffer *VirtualO2MIntersectionDevice::PopRayBuffer() {
	return doneRayBufferQueue.Pop();
}

void VirtualO2MIntersectionDevice::PusherRouter(VirtualO2MIntersectionDevice *virtualDevice) {
	try {
		size_t lastIndex = 0;
		const size_t devCount = virtualDevice->realDevices.size();

		while (!boost::this_thread::interruption_requested()) {
			RayBuffer *rayBuffer = virtualDevice->todoRayBufferQueue.Pop();

			// Look for the device with less work to do
			size_t index = 0;
// Windows crap ...
#undef max
			size_t minQueueSize = std::numeric_limits<size_t>::max();
			for (size_t i = 0; i < devCount; ++i) {
				const size_t indexCheck = (lastIndex + i) % devCount;
				const size_t queueSize = virtualDevice->realDevices[indexCheck]->GetQueueSize();
				if (queueSize < minQueueSize) {
					index = indexCheck;
					minQueueSize = queueSize;
				}
			}

			virtualDevice->realDevices[index]->PushRayBuffer(rayBuffer);
			lastIndex = index;
		}
	} catch (boost::thread_interrupted) {
		// Time to exit
	} catch (cl::Error err) {
		LR_LOG(virtualDevice->GetContext(), "[VirtualO2MDevice::" << virtualDevice->deviceIndex <<
				"] PusherRouter thread ERROR: " << err.what() << "(" << err.err() << ")");
	}
}

void VirtualO2MIntersectionDevice::PopperRouter(VirtualO2MIntersectionDevice *virtualDevice, size_t deviceIndex) {
	try {
		IntersectionDevice *device = virtualDevice->realDevices[deviceIndex];

		while (!boost::this_thread::interruption_requested()) {
			RayBuffer *rayBuffer = device->PopRayBuffer();

			virtualDevice->doneRayBufferQueue.Push(rayBuffer);
		}
	} catch (boost::thread_interrupted) {
		// Time to exit
	} catch (cl::Error err) {
		LR_LOG(virtualDevice->GetContext(), "[VirtualO2MDevice::" << virtualDevice->deviceIndex << "::" << deviceIndex << "] PopperRouter thread ERROR: " << err.what() << "(" << err.err() << ")");
	}
}
