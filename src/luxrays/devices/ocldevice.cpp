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
#include "luxrays/kernels/kernels.h"

using namespace std;

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL Device Description
//------------------------------------------------------------------------------

string OpenCLDeviceDescription::GetDeviceType(const cl_uint type) {
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
	const DeviceType filter, vector<DeviceDescription *> &descriptions)
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
				vector<cl::Device> subDevices;
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
// OpenCLDevice
//------------------------------------------------------------------------------

OpenCLDevice::OpenCLDevice(
		const Context *context,
		OpenCLDeviceDescription *desc,
		const size_t devIndex) :
		Device(context, devIndex),
		deviceDesc(desc), oclQueue(nullptr) {
	deviceName = (desc->GetName() + " OpenCLIntersect").c_str();

	// Check if OpenCL 1.1 is available
	if (!desc->IsOpenCL_1_1()) {
		// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
		// print a warning instead of throwing an exception
		LR_LOG(deviceContext, "WARNING: OpenCL version 1.1 or better is required. Device " + deviceName + " may not work.");
	}
	
	kernelCache = new oclKernelPersistentCache("LUXRAYS_" LUXRAYS_VERSION_MAJOR "." LUXRAYS_VERSION_MINOR);
}

OpenCLDevice::~OpenCLDevice() {
	delete kernelCache;
}

void OpenCLDevice::Start() {
	HardwareDevice::Start();

	// Create the OpenCL queue
	cl::Context &oclContext = deviceDesc->GetOCLContext();
	oclQueue = new cl::CommandQueue(oclContext, deviceDesc->GetOCLDevice());
}

void OpenCLDevice::Stop() {
	HardwareDevice::Stop();

	delete oclQueue;
}

//------------------------------------------------------------------------------
// Kernels handling for hardware (aka GPU) only applications
//------------------------------------------------------------------------------

void OpenCLDevice::CompileProgram(HardwareDeviceProgram **program,
		const string &programParameters, const string &programSource,	
		const string &programName) {
	cl::Context &oclContext = deviceDesc->GetOCLContext();
	cl::Device &oclDevice = deviceDesc->GetOCLDevice();

	LR_LOG(deviceContext, "[" << programName << "] Defined symbols: " << programParameters);
	LR_LOG(deviceContext, "[" << programName << "] Compiling kernels ");

	const string oclProgramParameters = "-D LUXRAYS_OPENCL_DEVICE " +
		programParameters;
	const string oclProgramSource =
		luxrays::ocl::KernelSource_ocldevice_funcs +
		programSource;

	bool cached;
	cl::STRING_CLASS error;
	cl::Program *oclProgram = kernelCache->Compile(oclContext, oclDevice,
			oclProgramParameters, oclProgramSource,
			&cached, &error);
	if (!oclProgram) {
		LR_LOG(deviceContext, "[" << programName << "] OpenCL program compilation error" << endl << error);

		throw runtime_error(programName + " OpenCL program compilation error");
	}

	if (cached) {
		LR_LOG(deviceContext, "[" << programName << "] Program cached");
	} else {
		LR_LOG(deviceContext, "[" << programName << "] Program not cached");
	}

	if (!*program)
		*program = new OpenCLDeviceProgram();
	
	OpenCLDeviceProgram *oclDeviceProgram = dynamic_cast<OpenCLDeviceProgram *>(*program);
	assert (oclDeviceProgram);

	oclDeviceProgram->Set(oclProgram);
}

void OpenCLDevice::GetKernel(HardwareDeviceProgram *program,
		HardwareDeviceKernel **kernel, const string &kernelName) {
	if (!*kernel)
		*kernel = new OpenCLDeviceKernel();
	
	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(*kernel);
	assert (oclDeviceKernel);

	OpenCLDeviceProgram *oclDeviceProgram = dynamic_cast<OpenCLDeviceProgram *>(program);
	assert (oclDeviceProgram);

	oclDeviceKernel->Set(new cl::Kernel(*(oclDeviceProgram->Get()), kernelName.c_str()));
}

u_int OpenCLDevice::GetKernelWorkGroupSize(HardwareDeviceKernel *kernel) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	size_t size;
	oclDeviceKernel->oclKernel->getWorkGroupInfo<size_t>(deviceDesc->GetOCLDevice(),
				CL_KERNEL_WORK_GROUP_SIZE, &size);
	
	return size;
}

void OpenCLDevice::SetKernelArg(HardwareDeviceKernel *kernel,
		const u_int index, const size_t size, const void *arg) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	oclDeviceKernel->oclKernel->setArg(index, size, arg);
}

void OpenCLDevice::SetKernelArgBuffer(HardwareDeviceKernel *kernel,
		const u_int index, const HardwareDeviceBuffer *buff) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	const OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<const OpenCLDeviceBuffer *>(buff);
	if (oclDeviceBuff)
		oclDeviceKernel->oclKernel->setArg(index, *(oclDeviceBuff->oclBuff));
	else
		oclDeviceKernel->oclKernel->setArg(index, nullptr);
}

static cl::NDRange ConvertHardwareRange(const HardwareDeviceRange &range) {
	if (range.dimensions == 1)
		return cl::NDRange(range.sizes[0]);
	else if (range.dimensions == 2)
		return cl::NDRange(range.sizes[0], range.sizes[1]);
	else
		return cl::NDRange(range.sizes[0], range.sizes[1], range.sizes[2]);
}

void OpenCLDevice::EnqueueKernel(HardwareDeviceKernel *kernel,
			const HardwareDeviceRange &globalSize,
			const HardwareDeviceRange &workGroupSize) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	oclQueue->enqueueNDRangeKernel(*oclDeviceKernel->oclKernel,
			cl::NullRange,
			ConvertHardwareRange(globalSize),
			ConvertHardwareRange(workGroupSize));
}

void OpenCLDevice::EnqueueReadBuffer(const HardwareDeviceBuffer *buff,
		const bool blocking, const size_t size, void *ptr) {
	assert (buff);
	assert (!buff->IsNull());

	const OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<const OpenCLDeviceBuffer *>(buff);
	assert (oclDeviceBuff);
	assert (!oclDeviceBuff->IsNull());

	oclQueue->enqueueReadBuffer(*(oclDeviceBuff->oclBuff), blocking, 0, size, ptr);
}

void OpenCLDevice::EnqueueWriteBuffer(const HardwareDeviceBuffer *buff,
		const bool blocking, const size_t size, const void *ptr) {
	assert (buff);
	assert (!buff->IsNull());

	const OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<const OpenCLDeviceBuffer *>(buff);
	assert (oclDeviceBuff);
	assert (!oclDeviceBuff->IsNull());

	oclQueue->enqueueWriteBuffer(*(oclDeviceBuff->oclBuff), blocking, 0, size, ptr);
}

void OpenCLDevice::FlushQueue() {
	oclQueue->flush();
}

void OpenCLDevice::FinishQueue() {
	oclQueue->finish();
}

//------------------------------------------------------------------------------
// Memory management for hardware (aka GPU) only applications
//------------------------------------------------------------------------------

void OpenCLDevice::AllocBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
		void *src, const size_t size, const string &desc) {
	// Check if the buffer is too big
	if (deviceDesc->GetMaxMemoryAllocSize() < size) {
		// This is now only a WARNING and not an ERROR because NVIDIA reported
		// CL_DEVICE_MAX_MEM_ALLOC_SIZE is lower than the real limit.
		LR_LOG(deviceContext, "WARNING: the " << desc << " buffer is too big for " << GetName() <<
				" device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				deviceDesc->GetMaxMemoryAllocSize() << ")");
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
			if (src)
				oclQueue->enqueueWriteBuffer(**buff, CL_FALSE, 0, size, src);

			return;
		} else {
			// Free the buffer

			FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
			delete *buff;
			*buff = nullptr;
		}
	}

	if (desc != "")
		LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc <<
				" buffer size: " << ToMemString(size));

	cl::Context &oclContext = deviceDesc->GetOCLContext();
	*buff = new cl::Buffer(oclContext,
			clFlags,
			size, src);
	AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void OpenCLDevice::AllocBufferRO(HardwareDeviceBuffer **buff, void *src, const size_t size, const string &desc) {
	if (!*buff)
		*buff = new OpenCLDeviceBuffer();

	OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<OpenCLDeviceBuffer *>(*buff);
	assert (oclDeviceBuff);
	
	AllocBuffer(src ? (CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR) : CL_MEM_READ_ONLY, &(oclDeviceBuff->oclBuff), src, size, desc);
}

void OpenCLDevice::AllocBufferRW(HardwareDeviceBuffer **buff, void *src, const size_t size, const string &desc) {
	if (!*buff)
		*buff = new OpenCLDeviceBuffer();

	OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<OpenCLDeviceBuffer *>(*buff);
	assert (oclDeviceBuff);

	AllocBuffer(src ? (CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR) : CL_MEM_READ_WRITE, &(oclDeviceBuff->oclBuff), src, size, desc);
}

void OpenCLDevice::FreeBuffer(HardwareDeviceBuffer **buff) {
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
