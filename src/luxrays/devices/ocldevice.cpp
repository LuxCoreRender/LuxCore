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

#include <memory>

#include "luxrays/devices/ocldevice.h"
#include "luxrays/kernels/kernels.h"

using namespace std;

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL Device Description
//------------------------------------------------------------------------------

string OpenCLDeviceDescription::GetOCLPlatformName(const cl_platform_id oclPlatform) {
	size_t valueSize;
	CHECK_OCL_ERROR(clGetPlatformInfo(oclPlatform, CL_PLATFORM_NAME, 0, nullptr, &valueSize));
	char *value = (char *)alloca(valueSize * sizeof(char));
	CHECK_OCL_ERROR(clGetPlatformInfo(oclPlatform, CL_PLATFORM_NAME, valueSize, value, nullptr));

	return string(value);
}

string OpenCLDeviceDescription::GetOCLDeviceName(const cl_device_id oclDevice) {
	size_t valueSize;
	CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_NAME, 0, nullptr, &valueSize));
	char *value = (char *)alloca(valueSize * sizeof(char));
	CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_NAME, valueSize, value, nullptr));

	return string(value);
}

DeviceType OpenCLDeviceDescription::GetOCLDeviceType(const cl_device_id oclDevice) {
	cl_device_type type;
	CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_TYPE, sizeof(cl_device_type), &type, nullptr));

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

void OpenCLDeviceDescription::GetPlatformsList(std::vector<cl_platform_id> &platformsList) {
	cl_uint platformsCount;
	CHECK_OCL_ERROR(clGetPlatformIDs(0, nullptr, &platformsCount));
	
	platformsList.resize(platformsCount);
	CHECK_OCL_ERROR(clGetPlatformIDs(platformsCount, &platformsList[0], nullptr));
}

void OpenCLDeviceDescription::AddDeviceDescs(const cl_platform_id oclPlatform,
	const DeviceType filter, vector<DeviceDescription *> &descriptions) {
	// Get the list of devices available on the platform
	cl_uint deviceCount;
	CHECK_OCL_ERROR(clGetDeviceIDs(oclPlatform, CL_DEVICE_TYPE_ALL, 0, nullptr, &deviceCount));
	
	cl_device_id *devices = (cl_device_id *)alloca(deviceCount * sizeof(cl_device_id));
	CHECK_OCL_ERROR(clGetDeviceIDs(oclPlatform, CL_DEVICE_TYPE_ALL, deviceCount, devices, nullptr));

	// Build the descriptions
	for (size_t i = 0; i < deviceCount; ++i) {
		DeviceType devType = GetOCLDeviceType(devices[i]);
		if (filter & devType)
			descriptions.push_back(new OpenCLDeviceDescription(devices[i], i));
	}
}

cl_context OpenCLDeviceDescription::GetOCLContext() {
	if (!oclContext) {
		// Allocate a context with the selected device
		
		cl_device_id devices[1] = { oclDevice };
		cl_int error;
		oclContext = clCreateContext(nullptr, 1, devices, nullptr, nullptr, &error);
		CHECK_OCL_ERROR(error);
	}

	return oclContext;
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
	cl_int error;
	oclQueue = clCreateCommandQueue(deviceDesc->GetOCLContext(), deviceDesc->GetOCLDevice(), 0, &error);
	CHECK_OCL_ERROR(error);
}

void OpenCLDevice::Stop() {
	HardwareDevice::Stop();

	if (oclQueue)
		CHECK_OCL_ERROR(clReleaseCommandQueue(oclQueue));
}

//------------------------------------------------------------------------------
// Kernels handling for hardware (aka GPU) only applications
//------------------------------------------------------------------------------

void OpenCLDevice::CompileProgram(HardwareDeviceProgram **program,
		const vector<string> &programParameters, const string &programSource,	
		const string &programName) {
	vector <string> oclProgramParameters = programParameters;
	oclProgramParameters.push_back("-D LUXRAYS_OPENCL_DEVICE");
#if defined (__APPLE__)
	oclProgramParameters.push_back("-D LUXRAYS_OS_APPLE");
#elif defined (WIN32)
	oclProgramParameters.push_back("-D LUXRAYS_OS_WINDOWS");
#elif defined (__linux__)
	oclProgramParameters.push_back("-D LUXRAYS_OS_LINUX");
#endif
	
	oclProgramParameters.insert(oclProgramParameters.end(),
			additionalCompileOpts.begin(), additionalCompileOpts.end());

	LR_LOG(deviceContext, "[" << programName << "] Compiler options: " << oclKernelPersistentCache::ToOptsString(oclProgramParameters));
	LR_LOG(deviceContext, "[" << programName << "] Compiling kernels ");

	const string oclProgramSource =
		luxrays::ocl::KernelSource_ocldevice_funcs +
		programSource;

	bool cached;
	string error;
	cl_program oclProgram = kernelCache->Compile(deviceDesc->GetOCLContext(), deviceDesc->GetOCLDevice(),
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

	cl_int error;
	cl_kernel k = clCreateKernel(oclDeviceProgram->Get(), kernelName.c_str(), &error);
	CHECK_OCL_ERROR(error);

	oclDeviceKernel->Set(k);
}

u_int OpenCLDevice::GetKernelWorkGroupSize(HardwareDeviceKernel *kernel) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	size_t size;
	CHECK_OCL_ERROR(clGetKernelWorkGroupInfo(oclDeviceKernel->oclKernel, deviceDesc->GetOCLDevice(),
			CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &size, nullptr));
	
	return size;
}

void OpenCLDevice::SetKernelArg(HardwareDeviceKernel *kernel,
		const u_int index, const size_t size, const void *arg) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	CHECK_OCL_ERROR(clSetKernelArg(oclDeviceKernel->oclKernel, index, size, arg));
}

void OpenCLDevice::SetKernelArgBuffer(HardwareDeviceKernel *kernel,
		const u_int index, const HardwareDeviceBuffer *buff) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	const OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<const OpenCLDeviceBuffer *>(buff);

	CHECK_OCL_ERROR(clSetKernelArg(oclDeviceKernel->oclKernel, index, sizeof(cl_mem), oclDeviceBuff ? &(oclDeviceBuff->oclBuff) : nullptr));
}

static void ConvertHardwareRange(const HardwareDeviceRange &range, size_t *globalSizeArray) {
	if (range.dimensions == 1) {
		globalSizeArray[0] = range.sizes[0];
	} else if (range.dimensions == 2) {
		globalSizeArray[0] = range.sizes[0];
		globalSizeArray[2] = range.sizes[1];
	} else {
		globalSizeArray[0] = range.sizes[0];
		globalSizeArray[2] = range.sizes[1];
		globalSizeArray[0] = range.sizes[2];
	}
}

void OpenCLDevice::EnqueueKernel(HardwareDeviceKernel *kernel,
			const HardwareDeviceRange &globalSize,
			const HardwareDeviceRange &workGroupSize) {
	assert (kernel);
	assert (!kernel->IsNull());

	OpenCLDeviceKernel *oclDeviceKernel = dynamic_cast<OpenCLDeviceKernel *>(kernel);
	assert (oclDeviceKernel);

	size_t globalSizeArray[3];
	ConvertHardwareRange(globalSize, globalSizeArray);
	size_t workGroupSizeArray[3];
	ConvertHardwareRange(workGroupSize, workGroupSizeArray);

	CHECK_OCL_ERROR(clEnqueueNDRangeKernel(oclQueue, oclDeviceKernel->oclKernel,
			globalSize.dimensions,
			nullptr, 
			globalSizeArray,
			workGroupSizeArray,
			0, nullptr, nullptr));
}

void OpenCLDevice::EnqueueReadBuffer(const HardwareDeviceBuffer *buff,
		const bool blocking, const size_t size, void *ptr) {
	assert (buff);
	assert (!buff->IsNull());

	const OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<const OpenCLDeviceBuffer *>(buff);
	assert (oclDeviceBuff);
	assert (!oclDeviceBuff->IsNull());

	CHECK_OCL_ERROR(clEnqueueReadBuffer(oclQueue, oclDeviceBuff->oclBuff,
			blocking, 0, size, ptr, 0, nullptr, nullptr));
}

void OpenCLDevice::EnqueueWriteBuffer(const HardwareDeviceBuffer *buff,
		const bool blocking, const size_t size, const void *ptr) {
	assert (buff);
	assert (!buff->IsNull());

	const OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<const OpenCLDeviceBuffer *>(buff);
	assert (oclDeviceBuff);
	assert (!oclDeviceBuff->IsNull());

	CHECK_OCL_ERROR(clEnqueueWriteBuffer(oclQueue, oclDeviceBuff->oclBuff,
			blocking, 0, size, ptr, 0, nullptr, nullptr));
}

void OpenCLDevice::FlushQueue() {
	CHECK_OCL_ERROR(clFlush(oclQueue));
}

void OpenCLDevice::FinishQueue() {
	CHECK_OCL_ERROR(clFinish(oclQueue));
}

//------------------------------------------------------------------------------
// Memory management for hardware (aka GPU) only applications
//------------------------------------------------------------------------------

void OpenCLDevice::AllocBuffer(const cl_mem_flags clFlags, cl_mem *buff,
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
			FreeMemory(OpenCLDeviceBuffer::GetSize(*buff));
			CHECK_OCL_ERROR(clReleaseMemObject(*buff));
		}

		*buff = nullptr;

		return;
	}

	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == OpenCLDeviceBuffer::GetSize(*buff)) {
			// I can reuse the buffer; just update the content

			//LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc << " buffer updated for size: " << (size / 1024) << "Kbytes");
			if (src)
				CHECK_OCL_ERROR(clEnqueueWriteBuffer(oclQueue, *buff, CL_FALSE,0, size, src, 0, nullptr, nullptr));

			return;
		} else {
			// Free the buffer

			FreeMemory(OpenCLDeviceBuffer::GetSize(*buff));
			CHECK_OCL_ERROR(clReleaseMemObject(*buff));
			*buff = nullptr;
		}
	}

	if (desc != "")
		LR_LOG(deviceContext, "[Device " << GetName() << "] " << desc <<
				" buffer size: " << ToMemString(size));

	cl_int error;
	*buff = clCreateBuffer(deviceDesc->GetOCLContext(), clFlags, size, src, &error);
	CHECK_OCL_ERROR(error);

	AllocMemory(OpenCLDeviceBuffer::GetSize(*buff));
}

void OpenCLDevice::AllocBuffer(HardwareDeviceBuffer **buff, const BufferType type,
		void *src, const size_t size, const string &desc) {
	if (!*buff)
		*buff = new OpenCLDeviceBuffer();

	OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<OpenCLDeviceBuffer *>(*buff);
	assert (oclDeviceBuff);

	cl_mem_flags clFlags = BUFFER_TYPE_NONE;
	if (type & BUFFER_TYPE_READ_ONLY)
		clFlags |= CL_MEM_READ_ONLY;
	if (type & BUFFER_TYPE_READ_WRITE)
		clFlags |= CL_MEM_READ_WRITE;
	if (src)
		clFlags |= CL_MEM_COPY_HOST_PTR;

	// Check if I was asked for out of core support
	if (type & BUFFER_TYPE_OUT_OF_CORE) {
		LR_LOG(deviceContext, "WARNING: OpenCL devices don't support out of core memory buffers: " << desc);
	}
	
	AllocBuffer(clFlags, &(oclDeviceBuff->oclBuff), src, size, desc);
}

void OpenCLDevice::FreeBuffer(HardwareDeviceBuffer **buff) {
	if (*buff && !(*buff)->IsNull()) {
		OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<OpenCLDeviceBuffer *>(*buff);
		assert (oclDeviceBuff);

		FreeMemory(oclDeviceBuff->GetSize());

		delete *buff;
		*buff = nullptr;
	}
}

}

#endif
