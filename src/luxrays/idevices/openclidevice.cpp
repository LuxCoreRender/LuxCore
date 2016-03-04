/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/foreach.hpp>

#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/utils/atomic.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL Intersection Queue Element
//------------------------------------------------------------------------------

OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::OpenCLDeviceQueueElem(
	OpenCLIntersectionDevice *dev, cl::CommandQueue *q, const u_int index) :
	device(dev), oclQueue(q), kernelIndex(index) {
	cl::Context &oclContext = device->deviceDesc->GetOCLContext();

	// Allocate OpenCL buffers
	rayBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY,
		sizeof(Ray) * RayBufferSize);
	device->AllocMemory(rayBuff->getInfo<CL_MEM_SIZE>());

	hitBuff = new cl::Buffer(oclContext, CL_MEM_WRITE_ONLY,
		sizeof(RayHit) * RayBufferSize);
	device->AllocMemory(hitBuff->getInfo<CL_MEM_SIZE>());

	event = new cl::Event();

	pendingRayBuffer = NULL;
}

OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::~OpenCLDeviceQueueElem() {
	delete event;
	device->FreeMemory(rayBuff->getInfo<CL_MEM_SIZE>());
	delete rayBuff;
	device->FreeMemory(hitBuff->getInfo<CL_MEM_SIZE>());
	delete hitBuff;
}

void OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::PushRayBuffer(RayBuffer *rayBuffer) {
	// A safety check
	if (pendingRayBuffer)
		throw std::runtime_error("Double push in OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::PushRayBuffer()");

	// Enqueue the upload of the rays to the device
	const size_t rayCount = rayBuffer->GetRayCount();
	oclQueue->enqueueWriteBuffer(*rayBuff, CL_FALSE, 0,
			sizeof(Ray) * rayCount, rayBuffer->GetRayBuffer());

	// Enqueue the intersection kernel
	device->kernels->EnqueueRayBuffer(*oclQueue, kernelIndex, *rayBuff, *hitBuff, rayCount, NULL, NULL);

	// Enqueue the download of the results
	oclQueue->enqueueReadBuffer(*hitBuff, CL_FALSE, 0,
			sizeof(RayHit) * rayBuffer->GetRayCount(),
			rayBuffer->GetHitBuffer(), NULL, event);

	pendingRayBuffer = rayBuffer;
}

RayBuffer *OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::PopRayBuffer() {
	// A safety check
	if (!pendingRayBuffer)
		throw std::runtime_error("Pop without a push in OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::PopRayBuffer()");

	event->wait();	
	RayBuffer *result = pendingRayBuffer;
	pendingRayBuffer = NULL;

	return result;
}

//------------------------------------------------------------------------------
// OpenCL Intersection Queue
//------------------------------------------------------------------------------

OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueue(
	OpenCLIntersectionDevice *dev, const u_int kernelIndexOffset) : device(dev) {
	cl::Context &oclContext = device->deviceDesc->GetOCLContext();

	// Create the OpenCL queue
	oclQueue = new cl::CommandQueue(oclContext, device->deviceDesc->GetOCLDevice());

	// Allocated all associated buffers if using data parallel mode
	if (device->dataParallelSupport) {
		for (u_int i = 0; i < device->bufferCount; ++i)
			freeElem.push_back(new OpenCLDeviceQueueElem(device, oclQueue, kernelIndexOffset + i));
	} else {
		// Only need one buffer
		freeElem.push_back(new OpenCLDeviceQueueElem(device, oclQueue, kernelIndexOffset));
	}

	pendingRayBuffers = 0;
	lastTimeEmptyQueue = WallClockTime();

	statsTotalDataParallelRayCount = 0.0;
	statsDeviceIdleTime = 0.0;
}

OpenCLIntersectionDevice::OpenCLDeviceQueue::~OpenCLDeviceQueue() {
	oclQueue->finish();

	BOOST_FOREACH(OpenCLDeviceQueueElem *elem, freeElem)
		delete elem;
	BOOST_FOREACH(OpenCLDeviceQueueElem *elem, busyElem)
		delete elem;

	delete oclQueue;
}

void OpenCLIntersectionDevice::OpenCLDeviceQueue::PushRayBuffer(RayBuffer *rayBuffer) {
	if (freeElem.size() == 0)
		throw std::runtime_error("Out of free buffers in OpenCLIntersectionDevice::OpenCLDeviceQueue::PushRayBuffer()");

	OpenCLDeviceQueueElem *elem = freeElem.back();
	freeElem.pop_back();

	elem->PushRayBuffer(rayBuffer);

	busyElem.push_front(elem);

	if (pendingRayBuffers == 0)
		statsDeviceIdleTime += WallClockTime() - lastTimeEmptyQueue;

	++pendingRayBuffers;
}

RayBuffer *OpenCLIntersectionDevice::OpenCLDeviceQueue::PopRayBuffer() {
	if (busyElem.size() == 0)
		throw std::runtime_error("Double pop in OpenCLIntersectionDevice::OpenCLDeviceQueue::PopRayBuffer()");

	OpenCLDeviceQueueElem *elem = busyElem.back();
	busyElem.pop_back();

	RayBuffer *rayBuffer = elem->PopRayBuffer();
	--pendingRayBuffers;
	statsTotalDataParallelRayCount += rayBuffer->GetRayCount();

	freeElem.push_front(elem);

	if (pendingRayBuffers == 0)
		lastTimeEmptyQueue = WallClockTime();

	return rayBuffer;
}

//------------------------------------------------------------------------------
// OpenCL IntersectionDevice
//------------------------------------------------------------------------------

size_t OpenCLIntersectionDevice::RayBufferSize = RAYBUFFER_SIZE;

OpenCLIntersectionDevice::OpenCLIntersectionDevice(
		const Context *context,
		OpenCLDeviceDescription *desc,
		const size_t index) :
		HardwareIntersectionDevice(context, desc->type, index) {
	stackSize = 24;
	deviceDesc = desc;
	deviceName = (desc->GetName() + "Intersect").c_str();
	reportedPermissionError = false;

	// Check if OpenCL 1.1 is available
	if (!desc->IsOpenCL_1_1()) {
		// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
		// print a warning instead of throwing an exception
		LR_LOG(deviceContext, "WARNING: OpenCL version 1.1 or better is required. Device " + deviceName + " may not work.");
	}
}

OpenCLIntersectionDevice::~OpenCLIntersectionDevice() {
	if (started)
		Stop();
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer() {
	return NewRayBuffer(RayBufferSize);
}

RayBuffer *OpenCLIntersectionDevice::NewRayBuffer(const size_t size) {
	return new RayBuffer(RoundUpPow2<size_t>(size));
}

size_t OpenCLIntersectionDevice::GetQueueSize() {
	if (started) {
		size_t count = 0;
		BOOST_FOREACH(OpenCLDeviceQueue *oclQueue, oclQueues)
			count += oclQueue->pendingRayBuffers;

		return count;
	} else
		return 0;
}

void OpenCLIntersectionDevice::PushRayBuffer(RayBuffer *rayBuffer, const u_int queueIndex) {
	assert (started);
	assert (dataParallelSupport);

	oclQueues[queueIndex]->PushRayBuffer(rayBuffer);
}

RayBuffer *OpenCLIntersectionDevice::PopRayBuffer(const u_int queueIndex) {
	assert (started);
	assert (dataParallelSupport);

	return oclQueues[queueIndex]->PopRayBuffer();
}

void OpenCLIntersectionDevice::SetDataSet(DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);

	if (dataSet) {
		const AcceleratorType accelType = dataSet->GetAcceleratorType();
		if (accelType != ACCEL_AUTO) {
			accel = dataSet->GetAccelerator(accelType);
		} else {
			if (dataSet->RequiresInstanceSupport() || dataSet->RequiresMotionBlurSupport())
				accel = dataSet->GetAccelerator(ACCEL_MBVH);
			else
				accel = dataSet->GetAccelerator(ACCEL_BVH);
		}
	}
}

void OpenCLIntersectionDevice::Update() {
	kernels->Update(dataSet);
}

void OpenCLIntersectionDevice::Start() {
	IntersectionDevice::Start();

	oclQueues.clear();
	if (dataParallelSupport) {
		// Compile all required kernels
		kernels = accel->NewOpenCLKernels(this, queueCount * bufferCount, stackSize);

		for (u_int i = 0; i < queueCount; ++i) {
			// Create the OpenCL queue
			oclQueues.push_back(new OpenCLDeviceQueue(this, i * bufferCount));
		}
	} else {
		// Compile all required kernels
		kernels = accel->NewOpenCLKernels(this, 1, stackSize);

		// I need to create at least one queue (for GPU rendering)
		oclQueues.push_back(new OpenCLDeviceQueue(this, 0));
	}
}

void OpenCLIntersectionDevice::Interrupt() {
	assert (started);
}

void OpenCLIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	BOOST_FOREACH(OpenCLDeviceQueue *queue, oclQueues)
		delete queue;
	oclQueues.clear();

	delete kernels;
	kernels = NULL;
}

//------------------------------------------------------------------------------
// Memory allocation for GPU only applications
//------------------------------------------------------------------------------

void OpenCLIntersectionDevice::AllocBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
		void *src, const size_t size, const std::string &desc) {
	// Check if the buffer is too big
	if (GetDeviceDesc()->GetMaxMemoryAllocSize() < size) {
		std::stringstream ss;
		ss << "The " << desc << " buffer is too big for " << GetName() <<
				" device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				GetDeviceDesc()->GetMaxMemoryAllocSize() <<
				"): try to reduce related parameters";
		throw std::runtime_error(ss.str());
	}

	// Handle the case of an empty buffer
	if (!size) {
		if (*buff) {
			// Free the buffer
			FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
			delete *buff;
		}

		*buff = NULL;

		return;
	}

	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer; just update the content

			//LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc << " buffer updated for size: " << (size / 1024) << "Kbytes");
			if (src) {
				cl::CommandQueue &oclQueue = GetOpenCLQueue();
				oclQueue.enqueueWriteBuffer(**buff, CL_FALSE, 0, size, src);
			}

			return;
		} else {
			// Free the buffer
			FreeBuffer(buff);
		}
	}

	cl::Context &oclContext = GetOpenCLContext();

	if (desc != "")
		LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc << " buffer size: " <<
				(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));
	*buff = new cl::Buffer(oclContext,
			clFlags,
			size, src);
	AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void OpenCLIntersectionDevice::AllocBufferRO(cl::Buffer **buff, void *src, const size_t size, const std::string &desc) {
	AllocBuffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, buff, src, size, desc);
}

void OpenCLIntersectionDevice::AllocBufferRO(cl::Buffer **buff, const size_t size, const std::string &desc) {
	AllocBuffer(CL_MEM_READ_ONLY, buff, NULL, size, desc);
}

void OpenCLIntersectionDevice::AllocBufferRW(cl::Buffer **buff, void *src, const size_t size, const std::string &desc) {
	AllocBuffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buff, src, size, desc);
}

void OpenCLIntersectionDevice::AllocBufferRW(cl::Buffer **buff, const size_t size, const std::string &desc) {
	AllocBuffer(CL_MEM_READ_WRITE, buff, NULL, size, desc);
}

void OpenCLIntersectionDevice::FreeBuffer(cl::Buffer **buff) {
	if (*buff) {

		FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
		delete *buff;
		*buff = NULL;
	}
}

//------------------------------------------------------------------------------
// Statistics
//------------------------------------------------------------------------------

double OpenCLIntersectionDevice::GetLoad() const {
	UpdateCounters();

	return HardwareIntersectionDevice::GetLoad();
}

void OpenCLIntersectionDevice::UpdateCounters() const {
	double totalCount = 0.0;
	double totalIdle = 0.0;
	BOOST_FOREACH(OpenCLDeviceQueue *oclQueue, oclQueues) {
		totalCount += oclQueue->statsTotalDataParallelRayCount;
		totalIdle += oclQueue->statsDeviceIdleTime;
	}

	statsDeviceIdleTime = totalIdle / oclQueues.size();
	statsDeviceTotalTime = WallClockTime() - statsStartTime;
	statsTotalDataParallelRayCount = totalCount;
}

double OpenCLIntersectionDevice::GetTotalRaysCount() const {
	UpdateCounters();

	return HardwareIntersectionDevice::GetTotalRaysCount();
}

double OpenCLIntersectionDevice::GetTotalPerformance() const {
	UpdateCounters();

	return HardwareIntersectionDevice::GetTotalPerformance();
}

double OpenCLIntersectionDevice::GetDataParallelPerformance() const {
	UpdateCounters();

	return HardwareIntersectionDevice::GetDataParallelPerformance();
}

void OpenCLIntersectionDevice::ResetPerformaceStats() {
	HardwareIntersectionDevice::ResetPerformaceStats();

	BOOST_FOREACH(OpenCLDeviceQueue *oclQueue, oclQueues)
			oclQueue->statsTotalDataParallelRayCount = 0.0;
}

#endif
