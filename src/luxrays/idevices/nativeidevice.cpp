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

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/atomic.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// Native thread IntersectionDevice
//------------------------------------------------------------------------------

size_t NativeThreadIntersectionDevice::RayBufferSize = 512;

NativeThreadIntersectionDevice::NativeThreadIntersectionDevice(
	const Context *context, const size_t devIndex) :
	HardwareIntersectionDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex) {
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

void NativeThreadIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);
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
			bool res = SetThreadRRPriority(intersectionThread);
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
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer(const size_t size) {
	return new RayBuffer(RoundUpPow2<size_t>(size));
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
				renderDevice->dataSet->Intersect(&rb[i], &hb[i]);
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
