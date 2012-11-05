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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/ocl/utils.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL IntersectionDevice
//------------------------------------------------------------------------------

size_t OpenCLIntersectionDevice::RayBufferSize = OPENCL_RAYBUFFER_SIZE;

OpenCLIntersectionDevice::OpenCLIntersectionDevice(
		const Context *context,
		OpenCLDeviceDescription *desc,
		const size_t index,
		const unsigned int forceWGSize) :
		HardwareIntersectionDevice(context, desc->type, index),
		oclQueue(new cl::CommandQueue(desc->GetOCLContext(),
		desc->GetOCLDevice())), kernel(NULL)
{
	forceWorkGroupSize = forceWGSize;
	stackSize = 24;
	deviceDesc = desc;
	deviceName = (desc->GetName() +"Intersect").c_str();
	reportedPermissionError = false;
	disableImageStorage = false;
	hybridRenderingSupport = true;
	intersectionThread = NULL;

	externalRayBufferQueue = NULL;
}

OpenCLIntersectionDevice::~OpenCLIntersectionDevice() {
	if (started)
		Stop();

	FreeDataSetBuffers();

	delete oclQueue;
}

void OpenCLIntersectionDevice::SetExternalRayBufferQueue(RayBufferQueue *queue) {
	assert (!started);

	externalRayBufferQueue = queue;
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer() {
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer(const size_t size) {
	return new RayBuffer(RoundUpPow2<size_t>(size));
}

void OpenCLIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer) {
	assert (started);
	assert (!externalRayBufferQueue);

	rayBufferQueue.PushToDo(rayBuffer, 0);
}

RayBuffer *OpenCLIntersectionDevice::PopRayBuffer() {
	assert (started);
	assert (!externalRayBufferQueue);

	return rayBufferQueue.PopDone(0);
}

void OpenCLIntersectionDevice::FreeDataSetBuffers() {
	// Check if I have to free something from previous DataSet
	if (dataSet) {
		deviceDesc->FreeMemory(raysBuff->getInfo<CL_MEM_SIZE>());
		delete raysBuff;
		raysBuff = NULL;
		deviceDesc->FreeMemory(hitsBuff->getInfo<CL_MEM_SIZE>());
		delete hitsBuff;
		hitsBuff = NULL;
	}
	delete kernel;
	kernel = NULL;
}

void OpenCLIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	FreeDataSetBuffers();

	IntersectionDevice::SetDataSet(newDataSet);

	if (!newDataSet)
		return;

	cl::Context &oclContext = GetOpenCLContext();

	// Allocate OpenCL buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Ray buffer size: " <<
		(sizeof(Ray) * RayBufferSize / 1024) << "Kbytes");
	raysBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY,
		sizeof(Ray) * RayBufferSize);
	deviceDesc->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Ray hits buffer size: " <<
		(sizeof(RayHit) * RayBufferSize / 1024) << "Kbytes");
	hitsBuff = new cl::Buffer(oclContext, CL_MEM_WRITE_ONLY,
		sizeof(RayHit) * RayBufferSize);
	deviceDesc->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	kernel = dataSet->GetAccelerator()->NewOpenCLKernel(this,
		stackSize, disableImageStorage);
}

void OpenCLIntersectionDevice::UpdateDataSet() {
	if (kernel)
		kernel->UpdateDataSet(dataSet);
}

void OpenCLIntersectionDevice::Start() {
	IntersectionDevice::Start();

	if (hybridRenderingSupport) {
		// Create the thread for the rendering
		intersectionThread = new boost::thread(boost::bind(OpenCLIntersectionDevice::IntersectionThread, this));

		// Set intersectionThread priority
		bool res = SetThreadRRPriority(intersectionThread);
		if (res && !reportedPermissionError) {
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)");
			reportedPermissionError = true;
		}
	}
}

void OpenCLIntersectionDevice::Interrupt() {
	assert (started);

	if (hybridRenderingSupport)
		intersectionThread->interrupt();
}

void OpenCLIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	if (hybridRenderingSupport) {
		intersectionThread->interrupt();
		intersectionThread->join();
		delete intersectionThread;
		intersectionThread = NULL;

		if (!externalRayBufferQueue)
			rayBufferQueue.Clear();
	}
}

void OpenCLIntersectionDevice::TraceRayBuffer(RayBuffer *rayBuffer,
	VECTOR_CLASS<cl::Event> &readEvent, VECTOR_CLASS<cl::Event> &traceEvent,
	cl::Event *event) {
	// Upload the rays to the GPU
	oclQueue->enqueueWriteBuffer(*raysBuff, CL_FALSE, 0,
		sizeof(Ray) * rayBuffer->GetRayCount(),
		rayBuffer->GetRayBuffer(), NULL, &(readEvent[0]));

	EnqueueTraceRayBuffer(*raysBuff, *hitsBuff, rayBuffer->GetSize(),
		&readEvent, &(traceEvent[0]));

	// Download the results
	oclQueue->enqueueReadBuffer(*hitsBuff, CL_FALSE, 0,
		sizeof(RayHit) * rayBuffer->GetRayCount(),
		rayBuffer->GetHitBuffer(), &traceEvent, event);
}

void OpenCLIntersectionDevice::EnqueueTraceRayBuffer(cl::Buffer &rBuff,
	cl::Buffer &hBuff, const unsigned int rayCount,
	const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
	if (kernel) {
		kernel->EnqueueRayBuffer(rBuff, hBuff, rayCount, events, event);
		statsTotalRayCount += rayCount;
	}
}

void OpenCLIntersectionDevice::IntersectionThread(OpenCLIntersectionDevice *renderDevice) {
	LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread started");

	try {
		RayBufferQueue *queue = renderDevice->externalRayBufferQueue ?
			renderDevice->externalRayBufferQueue : &(renderDevice->rayBufferQueue);

		RayBuffer *rayBuffer0, *rayBuffer1, *rayBuffer2;
		const double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			const double t1 = WallClockTime();
			queue->Pop3xToDo(&rayBuffer0, &rayBuffer1, &rayBuffer2);
			renderDevice->statsDeviceIdleTime += WallClockTime() - t1;
			const unsigned int count = (rayBuffer0 ? 1 : 0) + (rayBuffer1 ? 1 : 0) + (rayBuffer2 ? 1 : 0);

			switch(count) {
				case 1: {
					// Only one ray buffer to trace available
					VECTOR_CLASS<cl::Event> readEvent0(1);
					VECTOR_CLASS<cl::Event> traceEvent0(1);
					cl::Event event0;
					renderDevice->TraceRayBuffer(rayBuffer0,
						readEvent0, traceEvent0,
						&event0);

					event0.wait();
					queue->PushDone(rayBuffer0);

					renderDevice->statsDeviceTotalTime = WallClockTime() - startTime;
					break;
				}
				case 2: {
					// At least 2 ray buffers to trace

					// Trace 0 ray buffer
					VECTOR_CLASS<cl::Event> readEvent0(1);
					VECTOR_CLASS<cl::Event> traceEvent0(1);
					cl::Event event0;
					renderDevice->TraceRayBuffer(rayBuffer0,
						readEvent0, traceEvent0,
						&event0);

					// Trace 1 ray buffer
					VECTOR_CLASS<cl::Event> readEvent1(1);
					VECTOR_CLASS<cl::Event> traceEvent1(1);
					cl::Event event1;
					renderDevice->TraceRayBuffer(rayBuffer1,
						readEvent1, traceEvent1,
						&event1);

					// Pop 0 ray buffer
					event0.wait();
					queue->PushDone(rayBuffer0);

					// Pop 1 ray buffer
					event1.wait();
					queue->PushDone(rayBuffer1);

					renderDevice->statsDeviceTotalTime = WallClockTime() - startTime;
					break;
				}
				case 3: {
					// At least 3 ray buffers to trace

					// Trace 0 ray buffer
					VECTOR_CLASS<cl::Event> readEvent0(1);
					VECTOR_CLASS<cl::Event> traceEvent0(1);
					cl::Event event0;
					renderDevice->TraceRayBuffer(rayBuffer0,
						readEvent0, traceEvent0,
						&event0);

					// Trace 1 ray buffer
					VECTOR_CLASS<cl::Event> readEvent1(1);
					VECTOR_CLASS<cl::Event> traceEvent1(1);
					cl::Event event1;
					renderDevice->TraceRayBuffer(rayBuffer1,
						readEvent1, traceEvent1,
						&event1);

					// Trace 2 ray buffer
					VECTOR_CLASS<cl::Event> readEvent2(1);
					VECTOR_CLASS<cl::Event> traceEvent2(1);
					cl::Event event2;
					renderDevice->TraceRayBuffer(rayBuffer2,
						readEvent2, traceEvent2,
						&event2);

					// Pop 0 ray buffer
					event0.wait();
					queue->PushDone(rayBuffer0);

					// Pop 1 ray buffer
					event1.wait();
					queue->PushDone(rayBuffer1);

					// Pop 2 ray buffer
					event2.wait();
					queue->PushDone(rayBuffer2);

					renderDevice->statsDeviceTotalTime = WallClockTime() - startTime;
					break;
				}
				default:
					assert (false);
			}
		}

		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread halted");
	} catch (cl::Error err) {
		LR_LOG(renderDevice->deviceContext, "[OpenCL device::" << renderDevice->deviceName << "] Rendering thread ERROR: " << err.what() << "(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}
}

#endif
