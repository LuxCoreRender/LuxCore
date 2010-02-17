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

#if defined (__linux__)
#include <pthread.h>
#endif

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

	intersectionThread = NULL;
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

	// Create the thread for the rendering
	intersectionThread = new boost::thread(boost::bind(NativeThreadIntersectionDevice::IntersectionThread, this));

	// Set thread priority
#if defined (__linux__) // || defined (__APPLE__) Note: SCHED_BATCH is not supported on MacOS
	{
		const pthread_t tid = (pthread_t) intersectionThread->native_handle();

		int policy = SCHED_BATCH;
		int sysMinPriority = sched_get_priority_min(policy);

		struct sched_param param;
		param.sched_priority = sysMinPriority;

		int ret = pthread_setschedparam(tid, policy, &param);
		if (ret)
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set thread priority, error: " << ret);
	}
#elif defined (WIN32)
	{
		const HANDLE tid = (HANDLE) renderThread->native_handle();
		if (!SetThreadPriority(tid, THREAD_PRIORITY_LOWEST))
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set thread priority, error: " << ret);
	}
#endif
}

void NativeThreadIntersectionDevice::Interrupt() {
	assert (started);
	intersectionThread->interrupt();
}

void NativeThreadIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	intersectionThread->interrupt();
	intersectionThread->join();
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer() {
	return new RayBuffer(RayBufferSize);
}

void NativeThreadIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	todoRayBufferQueue.Push(rayBuffer);
}

RayBuffer *NativeThreadIntersectionDevice::PopRayBuffer() {
	return doneRayBufferQueue.Pop();
}

void NativeThreadIntersectionDevice::IntersectionThread(NativeThreadIntersectionDevice *renderDevice) {
	LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread started");

	try {
		double lastStatPrint = WallClockTime();

		while (!boost::this_thread::interruption_requested()) {
			const double t1 = WallClockTime();
			RayBuffer *rayBuffer = renderDevice->todoRayBufferQueue.Pop();
			const double t2 = WallClockTime();

			// Trace rays
			const Ray *rb = rayBuffer->GetRayBuffer();
			RayHit *hb = rayBuffer->GetHitBuffer();
			const DataSet *dataSet = renderDevice->dataSet;
			for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i)
				dataSet->Intersect(&rb[i], &hb[i]);

			const double t3 = WallClockTime();
			const double rayCount = rayBuffer->GetRayCount();
			renderDevice->doneRayBufferQueue.Push(rayBuffer);
			const double t4 = WallClockTime();

			renderDevice->statsTotalRayTime += t3 - t2;
			renderDevice->statsTotalRayCount += rayCount;
			renderDevice->statsDeviceIdleTime += t2 - t1;
			renderDevice->statsDeviceTotalTime += t4 - t1;

			if (t4 - lastStatPrint > 5.0) {
				// Print some statistic
				char buf[128];
				sprintf(buf, "[Rays/sec %dK][Load %.1f%%]",
					int(renderDevice->statsTotalRayCount / (1000.0 * renderDevice->statsTotalRayTime)),
					100.0 * renderDevice->statsTotalRayTime / renderDevice->statsDeviceTotalTime);
				LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "]" << buf);
				lastStatPrint = t4;
			}
		}

		LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (cl::Error err) {
		LR_LOG(renderDevice->deviceContext, "[NativeThread::" << renderDevice->deviceName << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")");
	}
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
