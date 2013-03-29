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
	OpenCLIntersectionDevice *dev, cl::CommandQueue *q) : device(dev), oclQueue(q) {
	cl::Context &oclContext = device->deviceDesc->GetOCLContext();
	
	// Create the associated kernel
	kernel = device->dataSet->GetAccelerator()->NewOpenCLKernel(device,
			device->stackSize, device->disableImageStorage);

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
	delete kernel;
}

void OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::UpdateDataSet() {
	kernel->UpdateDataSet(device->dataSet);
}

void OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::PushRayBuffer(RayBuffer *rayBuffer) {
	// Enqueue the upload of the rays to the device
	const size_t rayCount = rayBuffer->GetRayCount();
	oclQueue->enqueueWriteBuffer(*rayBuff, CL_FALSE, 0,
			sizeof(Ray) * rayCount, rayBuffer->GetRayBuffer());

	// Enqueue the intersection kernel
	kernel->EnqueueRayBuffer(*rayBuff, *hitBuff, rayCount, NULL, NULL);

	// Enqueue the download of the results
	oclQueue->enqueueReadBuffer(*hitBuff, CL_FALSE, 0,
			sizeof(RayHit) * rayBuffer->GetRayCount(),
			rayBuffer->GetHitBuffer(), NULL, event);

	pendingRayBuffer = rayBuffer;
}

RayBuffer *OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueueElem::PopRayBuffer() {
	event->wait();
	
	return pendingRayBuffer;
}

//------------------------------------------------------------------------------
// OpenCL Intersection Queue
//------------------------------------------------------------------------------

OpenCLIntersectionDevice::OpenCLDeviceQueue::OpenCLDeviceQueue(OpenCLIntersectionDevice *dev) :
	device(dev) {
	cl::Context &oclContext = device->deviceDesc->GetOCLContext();

	// Create the OpenCL queue
	oclQueue = new cl::CommandQueue(oclContext, device->deviceDesc->GetOCLDevice());

	// Allocated all associated buffers
	for (u_int i = 0; i < device->deviceBufferCount; ++i)
		freeElem.push_back(new OpenCLDeviceQueueElem(device, oclQueue));

	statsTotalDataParallelRayCount = 0.0;
}

OpenCLIntersectionDevice::OpenCLDeviceQueue::~OpenCLDeviceQueue() {
	oclQueue->finish();

	BOOST_FOREACH(OpenCLDeviceQueueElem *elem, freeElem)
		delete elem;
	BOOST_FOREACH(OpenCLDeviceQueueElem *elem, busyElem)
		delete elem;

	delete oclQueue;
}

void OpenCLIntersectionDevice::OpenCLDeviceQueue::UpdateDataSet() {
	BOOST_FOREACH(OpenCLDeviceQueueElem *elem, freeElem)
		elem->UpdateDataSet();
}

void OpenCLIntersectionDevice::OpenCLDeviceQueue::PushRayBuffer(RayBuffer *rayBuffer) {
	if (freeElem.size() == 0)
		throw std::runtime_error("Out of free buffers in OpenCLIntersectionDevice::OpenCLDeviceQueue::PushRayBuffer()");

	OpenCLDeviceQueueElem *elem = freeElem.back();
	freeElem.pop_back();

	elem->PushRayBuffer(rayBuffer);

	busyElem.push_front(elem);

	statsTotalDataParallelRayCount += rayBuffer->GetRayCount();
	AtomicInc(&device->pendingRayBuffers);
}

RayBuffer *OpenCLIntersectionDevice::OpenCLDeviceQueue::PopRayBuffer() {
	OpenCLDeviceQueueElem *elem = busyElem.back();
	busyElem.pop_back();

	RayBuffer *rayBuffer = elem->PopRayBuffer();
	AtomicDec(&device->pendingRayBuffers);

	freeElem.push_front(elem);

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
	deviceBufferCount = 3;
	pendingRayBuffers = 0;
	reportedPermissionError = false;
	disableImageStorage = false;
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

void OpenCLIntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	IntersectionDevice::SetDataSet(newDataSet);
}

void OpenCLIntersectionDevice::UpdateDataSet() {
	BOOST_FOREACH(OpenCLDeviceQueue *queue, oclQueues)
		queue->UpdateDataSet();
}

void OpenCLIntersectionDevice::Start() {
	IntersectionDevice::Start();

	oclQueues.resize(0);
	if (dataParallelSupport) {
		for (u_int i = 0; i < queueCount; ++i) {
			// Create the OpenCL queue
			oclQueues.push_back(new OpenCLDeviceQueue(this));
		}
	}
}

void OpenCLIntersectionDevice::Interrupt() {
	assert (started);
}

void OpenCLIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	if (dataParallelSupport) {
		BOOST_FOREACH(OpenCLDeviceQueue *queue, oclQueues)
			delete queue;

		oclQueues.resize(0);
	}
	pendingRayBuffers = 0;
}

#endif
