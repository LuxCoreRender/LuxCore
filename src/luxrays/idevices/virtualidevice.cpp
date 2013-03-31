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
// VirtualIntersectionDevice class
//------------------------------------------------------------------------------

size_t VirtualIntersectionDevice::RayBufferSize = RAYBUFFER_SIZE;

VirtualIntersectionDevice::VirtualIntersectionDevice(
		const std::vector<IntersectionDevice *> &devices, const size_t index) :
		IntersectionDevice(devices[0]->GetContext(), DEVICE_TYPE_VIRTUAL, index) {
	char buf[256];
	sprintf(buf, "VirtualDevice-%03d", (int)index);
	deviceName = std::string(buf);

	realDevices = devices;
	traceRayRealDeviceIndex = 0;

	for (size_t i = 0; i < realDevices.size(); ++i) {
		realDevices[i]->SetQueueCount(queueCount);
		realDevices[i]->SetBufferCount(bufferCount);
	}
}

VirtualIntersectionDevice::~VirtualIntersectionDevice() {
	if (started)
		Stop();

	for (size_t i = 0; i < realDevices.size(); ++i)
		delete realDevices[i];
}

void VirtualIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);

	// Set the real devices data set
	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->SetDataSet(newDataSet);
}

void VirtualIntersectionDevice::Start() {
	IntersectionDevice::Start();
	pendingRayBufferDeviceIndex.resize(queueCount);
	for (size_t i = 0; i < queueCount; ++i)
		pendingRayBufferDeviceIndex[i].clear();

	// Start the real devices if required
	for (size_t i = 0; i < realDevices.size(); ++i) {
		LR_LOG(deviceContext, "[VirtualIntersectionDevice::" << deviceName << "] Starting real device: " << i);
		realDevices[i]->Start();
	}
}

void VirtualIntersectionDevice::Interrupt() {
}

void VirtualIntersectionDevice::Stop() {
	// Need to wait for all my pending RayBuffer
	for (size_t i = 0; i < queueCount; ++i) {
		while (pendingRayBufferDeviceIndex[i].size() > 0) {
			u_int deviceIndex = pendingRayBufferDeviceIndex[i].back();
			pendingRayBufferDeviceIndex[i].pop_back();

			realDevices[deviceIndex]->PopRayBuffer(i);
		}
	}

	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->Stop();

	IntersectionDevice::Stop();
}

void VirtualIntersectionDevice::SetQueueCount(const u_int count) {
	IntersectionDevice::SetQueueCount(count);

	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->SetQueueCount(queueCount);

	pendingRayBufferDeviceIndex.resize(queueCount);
}

void VirtualIntersectionDevice::SetBufferCount(const u_int count) {
	IntersectionDevice::SetBufferCount(count);

	for (size_t i = 0; i < realDevices.size(); ++i)
		realDevices[i]->SetBufferCount(bufferCount);
}

RayBuffer *VirtualIntersectionDevice::NewRayBuffer() {
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *VirtualIntersectionDevice::NewRayBuffer(const size_t size) {
	return new RayBuffer(size);
}

void VirtualIntersectionDevice::PushRayBuffer(
	RayBuffer *rayBuffer, const u_int queueIndex) {
	// Look for the hardware device with less pending work
	u_int deviceIndex = 0;
	size_t minCount = realDevices[0]->GetQueueSize();
	for (u_int i = 1; i < realDevices.size(); ++i) {
		const size_t count = realDevices[i]->GetQueueSize();
		if (count < minCount) {
			deviceIndex = i;
			minCount = count;
		}
	}

	realDevices[deviceIndex]->PushRayBuffer(rayBuffer, queueIndex);
	pendingRayBufferDeviceIndex[queueIndex].push_front(deviceIndex);
}

RayBuffer *VirtualIntersectionDevice::PopRayBuffer(const u_int queueIndex) {
	const u_int deviceIndex = pendingRayBufferDeviceIndex[queueIndex].back();
	pendingRayBufferDeviceIndex[queueIndex].pop_back();

	RayBuffer *rayBuffer = realDevices[deviceIndex]->PopRayBuffer(queueIndex);

	statsTotalDataParallelRayCount += rayBuffer->GetRayCount();

	return rayBuffer;
}
