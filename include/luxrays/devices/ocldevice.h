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

#ifndef _LUXRAYS_OCLDEVICE_H
#define	_LUXRAYS_OCLDEVICE_H

#include "luxrays/core/hardwaredevice.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/oclerror.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCLDeviceDescription
//------------------------------------------------------------------------------

class OpenCLDeviceDescription : public DeviceDescription {
public:
	OpenCLDeviceDescription(cl::Device &device, const size_t devIndex) :
		DeviceDescription(device.getInfo<CL_DEVICE_NAME>().c_str(),
			GetOCLDeviceType(device.getInfo<CL_DEVICE_TYPE>())),
		deviceIndex(devIndex),
		oclDevice(device), oclContext(NULL),
		enableOpenGLInterop(false),
		forceWorkGroupSize(0) { }

	virtual ~OpenCLDeviceDescription() {
		delete oclContext;
	}

	size_t GetDeviceIndex() const { return deviceIndex; }
	virtual int GetComputeUnits() const {
		return oclDevice.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
	}
	virtual size_t GetMaxMemory() const {
		return oclDevice.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
	}
	virtual size_t GetMaxMemoryAllocSize() const {
		return oclDevice.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
	}
	virtual u_int GetNativeVectorWidthFloat() const {
		return oclDevice.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT>();
	}

	u_int GetForceWorkGroupSize() const { return forceWorkGroupSize; }
	void SetForceWorkGroupSize(const u_int size) { forceWorkGroupSize = size; }

	bool HasImageSupport() const { return oclDevice.getInfo<CL_DEVICE_IMAGE_SUPPORT>() != 0 ; }
	size_t GetImage2DMaxWidth() const { return oclDevice.getInfo<CL_DEVICE_IMAGE2D_MAX_WIDTH>(); }
	size_t GetImage2DMaxHeight() const { return oclDevice.getInfo<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(); }

	bool HasOCLContext() const { return (oclContext != NULL); }
	bool HasOGLInterop() const { return enableOpenGLInterop; }
	void EnableOGLInterop() {
		if (!oclContext || enableOpenGLInterop)
			enableOpenGLInterop = true;
		else
			throw std::runtime_error("It is not possible to enable OpenGL interoperability when the OpenCL context has already been created");
	}

	cl::Context &GetOCLContext();
	cl::Device &GetOCLDevice() { return oclDevice; }

	std::string GetOpenCLVersion() const { return oclDevice.getInfo<CL_DEVICE_VERSION>(); }

	bool IsOpenCL_1_0() const {
		int major, minor;
		sscanf(oclDevice.getInfo<CL_DEVICE_VERSION>().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major > 1) || ((major == 1) && (minor >= 0));
	}
	bool IsOpenCL_1_1() const {
		int major, minor;
		sscanf(oclDevice.getInfo<CL_DEVICE_VERSION>().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major > 1) || ((major == 1) && (minor >= 1));
	}
	bool IsOpenCL_1_2() const {
		int major, minor;
		sscanf(oclDevice.getInfo<CL_DEVICE_VERSION>().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major > 1) || ((major == 1) && (minor >= 2));
	}
	bool IsOpenCL_2_0() const {
		int major, minor;
		sscanf(oclDevice.getInfo<CL_DEVICE_VERSION>().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major >= 2);
	}

	bool IsAMDPlatform() const {
		cl::Platform platform = oclDevice.getInfo<CL_DEVICE_PLATFORM>();
		return !strcmp(platform.getInfo<CL_PLATFORM_VENDOR>().c_str(), "Advanced Micro Devices, Inc.");
	}

	bool IsNVIDIAPlatform() const {
		cl::Platform platform = oclDevice.getInfo<CL_DEVICE_PLATFORM>();
		return !strcmp(platform.getInfo<CL_PLATFORM_VENDOR>().c_str(), "NVIDIA Corporation");
	}

	friend class Context;
	friend class OpenCLIntersectionDevice;

protected:
	static std::string GetDeviceType(const cl_uint type);
	static DeviceType GetOCLDeviceType(const cl_device_type type);
	static void AddDeviceDescs(const cl::Platform &oclPlatform, const DeviceType filter,
		std::vector<DeviceDescription *> &descriptions);

	size_t deviceIndex;

private:
	cl::Device oclDevice;
	cl::Context *oclContext;
	bool enableOpenGLInterop;
	u_int forceWorkGroupSize;
};

//------------------------------------------------------------------------------
// OpenCLDeviceBuffer
//------------------------------------------------------------------------------

class OpenCLDeviceBuffer : public HardwareDeviceBuffer {
public:
	OpenCLDeviceBuffer() : oclBuff(nullptr) { }
	virtual ~OpenCLDeviceBuffer() {
		delete oclBuff;
	}

	bool IsNull() const { 
		return (oclBuff == nullptr);
	}

	friend class OpenCLIntersectionDevice;

protected:
	cl::Buffer *oclBuff;
};

//------------------------------------------------------------------------------
// OpenCLIntersectionDevice
//------------------------------------------------------------------------------

class OpenCLKernel {
public:
	OpenCLKernel(OpenCLIntersectionDevice *dev) :
		device(dev), stackSize(24) {
		kernel = nullptr;
	}
	virtual ~OpenCLKernel() {
		delete kernel;
	}

	virtual void Update(const DataSet *newDataSet) = 0;
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) = 0;

	const std::string &GetIntersectionKernelSource() { return intersectionKernelSource; }
	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex) { return 0; }

	void SetMaxStackSize(const size_t s) { stackSize = s; }

protected:
	std::string intersectionKernelSource;

	OpenCLIntersectionDevice *device;
	cl::Kernel *kernel;
	size_t workGroupSize;
	size_t stackSize;
};

class OpenCLIntersectionDevice : public IntersectionDevice, public HardwareDevice {
public:
	OpenCLIntersectionDevice(const Context *context,
		OpenCLDeviceDescription *desc, const size_t devIndex);
	virtual ~OpenCLIntersectionDevice();

	virtual void SetDataSet(DataSet *newDataSet);
	virtual void Start();
	virtual void Stop();

	//--------------------------------------------------------------------------
	// Interface for GPU only applications
	//--------------------------------------------------------------------------

	cl::Context &GetOpenCLContext() { return deviceDesc->GetOCLContext(); }
	cl::Device &GetOpenCLDevice() { return deviceDesc->GetOCLDevice(); }
	cl::CommandQueue &GetOpenCLQueue() { return *oclQueue; }

	void EnqueueTraceRayBuffer(cl::Buffer &rBuff, cl::Buffer &hBuff,
		const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
		// Enqueue the intersection kernel
		kernel->EnqueueRayBuffer(*oclQueue, rBuff, hBuff, rayCount, events, event);
		statsTotalDataParallelRayCount += rayCount;
	}

	// To compile the this device intersection kernel inside application kernel
	const std::string &GetIntersectionKernelSource() { return kernel->GetIntersectionKernelSource(); }
	u_int SetIntersectionKernelArgs(cl::Kernel &oclKernel, const u_int argIndex) {
		return kernel->SetIntersectionKernelArgs(oclKernel, argIndex);
	}

	// A temporary method until when the new interface is complete
	void SetKernelArg(cl::Kernel *kernel, const u_int index, const HardwareDeviceBuffer *buff) {
		if (buff) {
			const OpenCLDeviceBuffer *oclDeviceBuff = dynamic_cast<const OpenCLDeviceBuffer *>(buff);

			kernel->setArg(index, sizeof(cl::Buffer), oclDeviceBuff->oclBuff);
		} else
			kernel->setArg(index, sizeof(cl::Buffer), nullptr);
			
	}
	
	//--------------------------------------------------------------------------
	// Memory management for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual size_t GetMaxMemory() const {
		return deviceDesc->GetMaxMemory();
	}

	virtual void AllocBufferRO(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc = "");
	virtual void AllocBufferRW(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc = "");
	virtual void FreeBuffer(HardwareDeviceBuffer **buff);

	//--------------------------------------------------------------------------
	// OpenCL Device specific methods
	//--------------------------------------------------------------------------

	OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }

	// Temporary methods until when the new interface is complete
	void AllocBufferRO(cl::Buffer **buff, void *src, const size_t size, const std::string &desc = "");
	void AllocBufferRW(cl::Buffer **buff, void *src, const size_t size, const std::string &desc = "");
	void FreeBuffer(cl::Buffer **buff);

	friend class Context;

protected:
	virtual void Update();

private:
	void AllocBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
			void *src, const size_t size, const std::string &desc = "");

	OpenCLDeviceDescription *deviceDesc;

	cl::CommandQueue *oclQueue;

	OpenCLKernel *kernel;
};

}

#endif

#endif	/* _LUXRAYS_OCLDEVICE_H */

