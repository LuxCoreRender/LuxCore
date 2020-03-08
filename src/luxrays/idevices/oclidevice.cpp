/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#include "luxrays/idevices/oclidevice.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/utils/atomic.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL IntersectionDevice
//------------------------------------------------------------------------------

OpenCLIntersectionDevice::OpenCLIntersectionDevice(
		const Context *context,
		OpenCLDeviceDescription *desc,
		const size_t index) :
		HardwareIntersectionDevice(context, desc->type, index),
		deviceDesc(desc), oclQueue(nullptr), kernel(nullptr) {
	deviceName = (desc->GetName() + " Intersect").c_str();

	// Check if OpenCL 1.1 is available
	if (!desc->IsOpenCL_1_1()) {
		// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
		// print a warning instead of throwing an exception
		LR_LOG(deviceContext, "WARNING: OpenCL version 1.1 or better is required. Device " + deviceName + " may not work.");
	}
}

OpenCLIntersectionDevice::~OpenCLIntersectionDevice() {
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
	kernel->Update(dataSet);
}

void OpenCLIntersectionDevice::Start() {
	IntersectionDevice::Start();

	// Create the OpenCL queue
	cl::Context &oclContext = deviceDesc->GetOCLContext();
	oclQueue = new cl::CommandQueue(oclContext, deviceDesc->GetOCLDevice());

	// Compile required kernel
	kernel = accel->NewOpenCLKernel(this);
}

void OpenCLIntersectionDevice::Stop() {
	IntersectionDevice::Stop();

	delete kernel;
	kernel = nullptr;

	delete oclQueue;
}

//------------------------------------------------------------------------------
// Memory allocation for GPU only applications
//------------------------------------------------------------------------------

void OpenCLIntersectionDevice::AllocBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
		void *src, const size_t size, const std::string &desc) {
	// Check if the buffer is too big
	if (GetDeviceDesc()->GetMaxMemoryAllocSize() < size) {
		// This is now only a WARNING and not an ERROR because NVIDIA reported
		// CL_DEVICE_MAX_MEM_ALLOC_SIZE is lower than the real limit.
		LR_LOG(deviceContext, "WARNING: the " << desc << " buffer is too big for " << GetName() <<
				" device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				GetDeviceDesc()->GetMaxMemoryAllocSize() << ")");
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
		LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc <<
				" buffer size: " << ToMemString(size));

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

#endif
