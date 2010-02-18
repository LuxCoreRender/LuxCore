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
#include "luxrays/core/context.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Native thread IntersectionDevice
//------------------------------------------------------------------------------

size_t NativeThreadIntersectionDevice::RayBufferSize = 512;

NativeThreadIntersectionDevice::NativeThreadIntersectionDevice(const Context *context,
		const size_t threadIndex, const unsigned int devIndex) :
			IntersectionDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex) {
	char buf[64];
	sprintf(buf, "NativeThread-%03d", (int)threadIndex);
	deviceName = std::string(buf);
}

NativeThreadIntersectionDevice::~NativeThreadIntersectionDevice() {
	if (started)
		Stop();
}

void NativeThreadIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);
}

void NativeThreadIntersectionDevice::Start() {
	IntersectionDevice::Start();
}

void NativeThreadIntersectionDevice::Interrupt() {
	assert (started);
}

void NativeThreadIntersectionDevice::Stop() {
	IntersectionDevice::Stop();
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void NativeThreadIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	const double t1 = WallClockTime();

	// Trace rays
	const Ray *rb = rayBuffer->GetRayBuffer();
	RayHit *hb = rayBuffer->GetHitBuffer();
	const double rayCount = rayBuffer->GetRayCount();
	for (unsigned int i = 0; i < rayCount; ++i)
		dataSet->Intersect(&rb[i], &hb[i]);

	const double t2 = WallClockTime();
	doneRayBufferQueue.Push(rayBuffer);

	const double dt = t2 - t1;
	statsTotalRayTime += dt;
	statsTotalRayCount += rayCount;
	statsDeviceTotalTime += dt;
}

RayBuffer *NativeThreadIntersectionDevice::PopRayBuffer() {
	return doneRayBufferQueue.Pop();
}

void NativeThreadIntersectionDevice::AddDevices(std::vector<DeviceDescription *> &descriptions) {
	unsigned int count = boost::thread::hardware_concurrency();

	// Build the descriptions
	char buf[64];
	for (size_t i = 0; i < count; ++i) {
		sprintf(buf, "NativeThread-%03d", (int)i);
		std::string deviceName = std::string(buf);

		descriptions.push_back(new NativeThreadDeviceDescription(deviceName, i));
	}
}

}
