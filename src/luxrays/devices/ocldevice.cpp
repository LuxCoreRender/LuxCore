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

#if defined (__linux__)
#include <GL/glx.h>
#endif

#if defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#endif

#include "luxrays/devices/ocldevice.h"

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL Device Description
//------------------------------------------------------------------------------

std::string OpenCLDeviceDescription::GetDeviceType(const cl_uint type) {
	switch (type) {
		case CL_DEVICE_TYPE_ALL:
			return "TYPE_ALL";
		case CL_DEVICE_TYPE_DEFAULT:
			return "TYPE_DEFAULT";
		case CL_DEVICE_TYPE_CPU:
			return "TYPE_CPU";
		case CL_DEVICE_TYPE_GPU:
			return "TYPE_GPU";
		default:
			return "TYPE_UNKNOWN";
	}
}

DeviceType OpenCLDeviceDescription::GetOCLDeviceType(const cl_device_type type)
{
	switch (type) {
		case CL_DEVICE_TYPE_ALL:
			return DEVICE_TYPE_OPENCL_ALL;
		case CL_DEVICE_TYPE_DEFAULT:
			return DEVICE_TYPE_OPENCL_DEFAULT;
		case CL_DEVICE_TYPE_CPU:
			return DEVICE_TYPE_OPENCL_CPU;
		case CL_DEVICE_TYPE_GPU:
			return DEVICE_TYPE_OPENCL_GPU;
		default:
			return DEVICE_TYPE_OPENCL_UNKNOWN;
	}
}

void OpenCLDeviceDescription::AddDeviceDescs(const cl::Platform &oclPlatform,
	const DeviceType filter, std::vector<DeviceDescription *> &descriptions)
{
	// Get the list of devices available on the platform
	VECTOR_CLASS<cl::Device> oclDevices;
	oclPlatform.getDevices(CL_DEVICE_TYPE_ALL, &oclDevices);

	// Build the descriptions
	for (size_t i = 0; i < oclDevices.size(); ++i) {
		DeviceType dev_type = GetOCLDeviceType(oclDevices[i].getInfo<CL_DEVICE_TYPE>());
		if (filter & dev_type)
		{
			/*if (dev_type == DEVICE_TYPE_OPENCL_CPU)
			{
				cl_device_partition_property_ext props[4] = { CL_DEVICE_PARTITION_BY_COUNTS_EXT, 1, 0, 0 };
				std::vector<cl::Device> subDevices;
				oclDevices[i].createSubDevices(props, &subDevices);
				descriptions.push_back(new OpenCLDeviceDescription(subDevices[0], i));
			} else*/
				descriptions.push_back(new OpenCLDeviceDescription(oclDevices[i], i));
		}
	}
}

cl::Context &OpenCLDeviceDescription::GetOCLContext() {
	if (!oclContext) {
		// Allocate a context with the selected device
		VECTOR_CLASS<cl::Device> devices;
		devices.push_back(oclDevice);
		cl::Platform platform = oclDevice.getInfo<CL_DEVICE_PLATFORM>();

		if (enableOpenGLInterop) {
#if defined (__APPLE__)
			CGLContextObj kCGLContext = CGLGetCurrentContext();
			CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
			cl_context_properties cps[] = {
				CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)kCGLShareGroup,
				0
			};
#else
#ifdef WIN32
			cl_context_properties cps[] = {
				CL_GL_CONTEXT_KHR, (intptr_t)wglGetCurrentContext(),
				CL_WGL_HDC_KHR, (intptr_t)wglGetCurrentDC(),
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
				0
			};
#else
			cl_context_properties cps[] = {
				CL_GL_CONTEXT_KHR, (intptr_t)glXGetCurrentContext(),
				CL_GLX_DISPLAY_KHR, (intptr_t)glXGetCurrentDisplay(),
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
				0
			};
#endif
#endif
			oclContext = new cl::Context(devices, cps);
		} else {
			cl_context_properties cps[] = {
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(), 0
			};

			oclContext = new cl::Context(devices, cps);
		}
	}

	return *oclContext;
}

//------------------------------------------------------------------------------
// OpenCL IntersectionDevice
//------------------------------------------------------------------------------

OpenCLIntersectionDevice::OpenCLIntersectionDevice(
		const Context *context,
		OpenCLDeviceDescription *desc,
		const size_t devIndex) :
		Device(context, desc->type, devIndex),
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
// Memory management for hardware (aka GPU) only applications
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

		*buff = nullptr;

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
	AllocBuffer(src ? (CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR) : CL_MEM_READ_WRITE, buff, src, size, desc);
}

void OpenCLIntersectionDevice::AllocBufferRW(cl::Buffer **buff, void *src, const size_t size, const std::string &desc) {
	AllocBuffer(src ? (CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR) : CL_MEM_READ_WRITE, buff, src, size, desc);
}

void OpenCLIntersectionDevice::FreeBuffer(cl::Buffer **buff) {
	if (*buff) {

		FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
		delete *buff;
		*buff = nullptr;
	}
}

void OpenCLIntersectionDevice::AllocBufferRO(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc) {
	if (!(*buff))
		*buff = new OpenCLDeviceBuffer();

	OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<OpenCLDeviceBuffer *>(*buff);
	assert (oclDeviceBuff);
	
	AllocBuffer(src ? (CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR) : CL_MEM_READ_WRITE, &(oclDeviceBuff->oclBuff), src, size, desc);
}

void OpenCLIntersectionDevice::AllocBufferRW(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc) {
	if (!(*buff))
		*buff = new OpenCLDeviceBuffer();

	OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<OpenCLDeviceBuffer *>(*buff);
	assert (oclDeviceBuff);

	AllocBuffer(src ? (CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR) : CL_MEM_READ_WRITE, &(oclDeviceBuff->oclBuff), src, size, desc);
}

void OpenCLIntersectionDevice::FreeBuffer(HardwareDeviceBuffer **buff) {
	if (*buff && !(*buff)->IsNull()) {
		OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<OpenCLDeviceBuffer *>(*buff);
		assert (oclDeviceBuff);

		FreeMemory(oclDeviceBuff->oclBuff->getInfo<CL_MEM_SIZE>());

		delete *buff;
		*buff = nullptr;
	}
}

}

#endif
