/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <cstdio>
#include <boost/foreach.hpp>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/atomic.h"

using namespace luxrays;

#define RAYBUFFER_DEFAULT_NATIVE_SIZE 512

//------------------------------------------------------------------------------
// Native thread IntersectionDevice
//------------------------------------------------------------------------------

NativeThreadIntersectionDevice::NativeThreadIntersectionDevice(
	const Context *context, const size_t devIndex) :
	HardwareIntersectionDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex) {
    maxRayBufferSize = RAYBUFFER_DEFAULT_NATIVE_SIZE;

	deviceName = std::string("NativeIntersect");
	reportedPermissionError = false;
	rayBufferQueue = NULL;
	threadCount = boost::thread::hardware_concurrency();
}

NativeThreadIntersectionDevice::~NativeThreadIntersectionDevice() {
	if (started)
		Stop();

	BOOST_FOREACH(boost::thread *intersectionThread, intersectionThreads)
		delete intersectionThread;
	delete rayBufferQueue;
}

void NativeThreadIntersectionDevice::SetDataSet(DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);

	if (dataSet) {
		const AcceleratorType accelType = dataSet->GetAcceleratorType();
		if (accelType != ACCEL_AUTO)
			accel = dataSet->GetAccelerator(accelType);
		else
			accel = dataSet->GetAccelerator(ACCEL_EMBREE);
	}
}

void NativeThreadIntersectionDevice::Start() {
	IntersectionDevice::Start();

	threadDeviceIdleTime.clear();
	threadTotalDataParallelRayCount.clear();
	threadDeviceTotalTime.clear();
	if (dataParallelSupport) {
		// Create all the required queues
		rayBufferQueue = new RayBufferQueueM2M(queueCount);

		// Create all threads for the rendering
		for (u_int i = 0; i < threadCount; ++i) {
			boost::thread *intersectionThread = new boost::thread(boost::bind(NativeThreadIntersectionDevice::IntersectionThread, this, i));

			// Set intersectionThread priority
			const bool res = SetThreadRRPriority(intersectionThread);
			if (res && !reportedPermissionError) {
				LR_LOG(deviceContext, "[NativeThread device::" << deviceName << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)");
				reportedPermissionError = true;
			}

			intersectionThreads.push_back(intersectionThread);
			threadDeviceIdleTime.push_back(0.0);
			threadTotalDataParallelRayCount.push_back(0.0);
			threadDeviceTotalTime.push_back(0.0);
		}
	}
}

void NativeThreadIntersectionDevice::Interrupt() {
	assert (started);

	if (dataParallelSupport) {
		BOOST_FOREACH(boost::thread *intersectionThread, intersectionThreads)
			intersectionThread->interrupt();
	}
}

void NativeThreadIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	if (dataParallelSupport) {
		BOOST_FOREACH(boost::thread *intersectionThread, intersectionThreads) {
			intersectionThread->interrupt();
			intersectionThread->join();
			delete intersectionThread;
		}
		intersectionThreads.clear();
		delete rayBufferQueue;
		rayBufferQueue = NULL;
	}
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer() {
	return NewRayBuffer(RAYBUFFER_DEFAULT_NATIVE_SIZE);
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer(const size_t size) {
    if (size > maxRayBufferSize)
        maxRayBufferSize = size;
	return new RayBuffer(size);
}

void NativeThreadIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer, const u_int queueIndex) {
	assert (started);
	assert (dataParallelSupport);

	rayBufferQueue->PushToDo(rayBuffer, queueIndex);
}

RayBuffer *NativeThreadIntersectionDevice::PopRayBuffer(const u_int queueIndex) {
	assert (started);
	assert (dataParallelSupport);

	return rayBufferQueue->PopDone(queueIndex);
}

void NativeThreadIntersectionDevice::IntersectionThread(NativeThreadIntersectionDevice *renderDevice, const u_int threadIndex) {
	//LR_LOG(renderDevice->deviceContext, "[NativeThread device::" << renderDevice->deviceName << "::" << threadIndex <<"] Rendering thread started");

	try {
		RayBufferQueue *queue = renderDevice->rayBufferQueue;

		const double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			const double t1 = WallClockTime();
			RayBuffer *rayBuffer = queue->PopToDo();
			renderDevice->threadDeviceIdleTime[threadIndex] += WallClockTime() - t1;

			// Trace rays
			const Ray *rb = rayBuffer->GetRayBuffer();
			RayHit *hb = rayBuffer->GetHitBuffer();
			const size_t rayCount = rayBuffer->GetRayCount();
			for (unsigned int i = 0; i < rayCount; ++i) {
				hb[i].SetMiss();
				renderDevice->accel->Intersect(&rb[i], &hb[i]);
			}
			renderDevice->threadTotalDataParallelRayCount[threadIndex] += rayCount;
			queue->PushDone(rayBuffer);

			renderDevice->threadDeviceTotalTime[threadIndex] = WallClockTime() - startTime;
		}

		//LR_LOG(renderDevice->deviceContext, "[NativeThread device::" << renderDevice->deviceName << "::" << threadIndex <<"] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		//LR_LOG(renderDevice->deviceContext, "[NativeThread device::" << renderDevice->deviceName << "::" << threadIndex <<"] Rendering thread halted");
	}
}

//------------------------------------------------------------------------------
// Statistics
//------------------------------------------------------------------------------

double NativeThreadIntersectionDevice::GetLoad() const {
	double totalIdle = 0.0;
	BOOST_FOREACH(double &idleTime, threadDeviceIdleTime)
		totalIdle += idleTime;
	statsDeviceIdleTime = totalIdle;

	double total = 0.0;
	BOOST_FOREACH(double &totalTime, threadDeviceTotalTime)
		total += totalTime;
	statsDeviceTotalTime = total;

	return IntersectionDevice::GetLoad();
}

void NativeThreadIntersectionDevice::UpdateTotalDataParallelRayCount() const {
	double total = 0.0;
	BOOST_FOREACH(double &rayCount, threadTotalDataParallelRayCount)
		total += rayCount;
	statsTotalDataParallelRayCount = total;
}

double NativeThreadIntersectionDevice::GetTotalRaysCount() const {
	UpdateTotalDataParallelRayCount();

	return HardwareIntersectionDevice::GetTotalRaysCount();
}

double NativeThreadIntersectionDevice::GetTotalPerformance() const {
	UpdateTotalDataParallelRayCount();

	return HardwareIntersectionDevice::GetTotalPerformance();
}

double NativeThreadIntersectionDevice::GetDataParallelPerformance() const {
	UpdateTotalDataParallelRayCount();

	return HardwareIntersectionDevice::GetDataParallelPerformance();
}

void NativeThreadIntersectionDevice::ResetPerformaceStats() {
	HardwareIntersectionDevice::ResetPerformaceStats();

	BOOST_FOREACH(double &idelTime, threadDeviceIdleTime)
			idelTime = 0.0;
	BOOST_FOREACH(double &rayCount, threadTotalDataParallelRayCount)
			rayCount = 0.0;
	BOOST_FOREACH(double &totalTime, threadDeviceTotalTime)
			totalTime = 0.0;
}
