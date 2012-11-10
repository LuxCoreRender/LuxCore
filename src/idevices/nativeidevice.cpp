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

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/context.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// Native thread IntersectionDevice
//------------------------------------------------------------------------------

size_t NativeThreadIntersectionDevice::RayBufferSize = 512;

NativeThreadIntersectionDevice::NativeThreadIntersectionDevice(const Context *context,
	const size_t threadIndex, const size_t devIndex) :
	HardwareIntersectionDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex)
{
	char buf[64];
	sprintf(buf, "NativeIntersectThread-%03d", (int)threadIndex);
	deviceName = std::string(buf);
	reportedPermissionError = false;
	externalRayBufferQueue = NULL;
	intersectionThread = NULL;
}

NativeThreadIntersectionDevice::~NativeThreadIntersectionDevice()
{
	if (started)
		Stop();
	delete intersectionThread;
}

void NativeThreadIntersectionDevice::SetExternalRayBufferQueue(RayBufferQueue *queue)
{
	assert (!started);

	externalRayBufferQueue = queue;
}

void NativeThreadIntersectionDevice::SetDataSet(const DataSet *newDataSet)
{
	IntersectionDevice::SetDataSet(newDataSet);
}

void NativeThreadIntersectionDevice::Start()
{
	IntersectionDevice::Start();

	// Create the thread for the rendering
	intersectionThread = new boost::thread(boost::bind(NativeThreadIntersectionDevice::IntersectionThread, this));

	// Set intersectionThread priority
	bool res = SetThreadRRPriority(intersectionThread);
	if (res && !reportedPermissionError) {
		LR_LOG(deviceContext, "[NativeThread device::" << deviceName << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)");
		reportedPermissionError = true;
	}
}

void NativeThreadIntersectionDevice::Interrupt()
{
	assert (started);

	intersectionThread->interrupt();
}

void NativeThreadIntersectionDevice::Stop()
{
	IntersectionDevice::Stop();

	intersectionThread->interrupt();
	intersectionThread->join();
	delete intersectionThread;
	intersectionThread = NULL;

	if (!externalRayBufferQueue)
		rayBufferQueue.Clear();
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer()
{
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *NativeThreadIntersectionDevice::NewRayBuffer(const size_t size)
{
	return new RayBuffer(RoundUpPow2<size_t>(size));
}

void NativeThreadIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer)
{
	assert (started);
	assert (!externalRayBufferQueue);

	rayBufferQueue.PushToDo(rayBuffer, 0);
}

RayBuffer *NativeThreadIntersectionDevice::PopRayBuffer()
{
	assert (started);
	assert (!externalRayBufferQueue);

	return rayBufferQueue.PopDone(0);
}

void NativeThreadIntersectionDevice::IntersectionThread(NativeThreadIntersectionDevice *renderDevice)
{
	LR_LOG(renderDevice->deviceContext, "[NativeThread device::" << renderDevice->deviceName << "] Rendering thread started");

	try {
		RayBufferQueue *queue = renderDevice->externalRayBufferQueue ?
			renderDevice->externalRayBufferQueue : &(renderDevice->rayBufferQueue);

		const double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			const double t1 = WallClockTime();
			RayBuffer *rayBuffer = queue->PopToDo();
			renderDevice->statsDeviceIdleTime += WallClockTime() - t1;
			// Trace rays
			const Ray *rb = rayBuffer->GetRayBuffer();
			RayHit *hb = rayBuffer->GetHitBuffer();
			const size_t rayCount = rayBuffer->GetRayCount();
			for (unsigned int i = 0; i < rayCount; ++i) {
				hb[i].SetMiss();
				renderDevice->dataSet->Intersect(&rb[i], &hb[i]);
			}
			renderDevice->statsTotalRayCount += rayCount;
			queue->PushDone(rayBuffer);

			renderDevice->statsDeviceTotalTime = WallClockTime() - startTime;
		}

		LR_LOG(renderDevice->deviceContext, "[NativeThread device::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		LR_LOG(renderDevice->deviceContext, "[NativeThread device::" << renderDevice->deviceName << "] Rendering thread halted");
	}
}
