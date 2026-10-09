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

#include <memory>

#include <boost/algorithm/string/trim.hpp>

#include "luxrays/core/hardwaredevice.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/usings.h"
#include "luxrays/utils/oclerror.h"
#include "luxrays/utils/oclcache.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCLDeviceDescription
//------------------------------------------------------------------------------

class OpenCLDeviceDescription : public DeviceDescription {
public:
	OpenCLDeviceDescription(const cl_device_id device, const size_t devIndex) :
		DeviceDescription(GetOCLDeviceName(device), GetOCLDeviceType(device)),
		deviceIndex(devIndex),
		oclDevice(device) {
	}

	virtual ~OpenCLDeviceDescription() {
	}

	size_t GetDeviceIndex() const { return deviceIndex; }
	virtual int GetComputeUnits() const {
		cl_uint v;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &v, nullptr));

		return v;
	}
	virtual size_t GetMaxMemory() const {
		cl_ulong v;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &v, nullptr));

		return v;
	}
	virtual size_t GetMaxMemoryAllocSize() const {
		cl_ulong v;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &v, nullptr));

		return v;
	}
	virtual u_int GetNativeVectorWidthFloat() const {
		cl_uint v;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT, sizeof(cl_uint), &v, nullptr));

		return v;
	}

	u_int GetClock() const {
		cl_uint v;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &v, nullptr));

		return v;
	}

	u_longlong GetLocalMem() const {
		cl_ulong v;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &v, nullptr));

		return v;
	}

	u_longlong GetConstMem() const {
		cl_ulong v;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &v, nullptr));

		return v;
	}

	cl_device_id GetOCLDevice() const { return oclDevice; }

	std::string GetOpenCLVersion() const {
		size_t valueSize;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_VERSION, 0, nullptr, &valueSize));
		char *value = (char *)alloca(valueSize * sizeof(char));
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_VERSION, valueSize, value, nullptr));

		return std::string(value);
	}

	bool IsOpenCL_1_0() const {
		int major, minor;
		sscanf(GetOpenCLVersion().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major > 1) || ((major == 1) && (minor >= 0));
	}
	bool IsOpenCL_1_1() const {
		int major, minor;
		sscanf(GetOpenCLVersion().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major > 1) || ((major == 1) && (minor >= 1));
	}
	bool IsOpenCL_1_2() const {
		int major, minor;
		sscanf(GetOpenCLVersion().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major > 1) || ((major == 1) && (minor >= 2));
	}
	bool IsOpenCL_2_0() const {
		int major, minor;
		sscanf(GetOpenCLVersion().c_str(), "OpenCL %d.%d", &major, &minor);
		return (major >= 2);
	}

	std::string GetOpenCLPlatform() const {
		cl_platform_id oclPlatform;
		CHECK_OCL_ERROR(clGetDeviceInfo(oclDevice, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &oclPlatform, nullptr));

		size_t platformNameSize;
		CHECK_OCL_ERROR(clGetPlatformInfo(oclPlatform, CL_PLATFORM_VENDOR, 0, nullptr, &platformNameSize));
		char *platformNameChar = (char *)alloca(platformNameSize * sizeof(char));
		CHECK_OCL_ERROR(clGetPlatformInfo(oclPlatform, CL_PLATFORM_VENDOR, platformNameSize, platformNameChar, nullptr));

		return boost::trim_copy(std::string(platformNameChar));
	}

	bool IsAMDPlatform() const {
		return !strcmp(GetOpenCLPlatform().c_str(), "Advanced Micro Devices, Inc.");
	}

	bool IsNVIDIAPlatform() const {
		return !strcmp(GetOpenCLPlatform().c_str(), "NVIDIA Corporation");
	}

	friend class Context;
	friend class OpenCLDevice;

protected:
	static std::string GetOCLPlatformName(const cl_platform_id oclPlatform);
	static std::string GetOCLDeviceName(const cl_device_id oclDevice);
	static DeviceType GetOCLDeviceType(const cl_device_id oclDevice);

	static void GetPlatformsList(std::vector<cl_platform_id> &platformsList);
	static void AddDeviceDescs(const cl_platform_id oclPlatform, const DeviceType filter,
			std::vector<DeviceDescriptionUPtr> &descriptions);

	size_t deviceIndex;

	cl_device_id oclDevice;
};

//------------------------------------------------------------------------------
// OpenCLDeviceKernel
//------------------------------------------------------------------------------

class OpenCLDeviceKernel : public HardwareDeviceKernel {
public:
	OpenCLDeviceKernel() : oclKernel(nullptr) { }
	virtual ~OpenCLDeviceKernel() {
		if (oclKernel)
			CHECK_OCL_ERROR(clReleaseKernel(oclKernel));
	}

	bool IsNull() const { 
		return (oclKernel == nullptr);
	}

	friend class OpenCLDevice;

protected:
	void Set(cl_kernel kernel) {
		if (oclKernel)
			CHECK_OCL_ERROR(clReleaseKernel(oclKernel));
		oclKernel = kernel;
	}

	cl_kernel Get() { return oclKernel; }

	cl_kernel oclKernel;
};

//------------------------------------------------------------------------------
// OpenCLDeviceProgram
//------------------------------------------------------------------------------

class OpenCLDeviceProgram : public HardwareDeviceProgram {
public:
	OpenCLDeviceProgram() : oclProgram(nullptr) { }

	virtual ~OpenCLDeviceProgram() {
		if (oclProgram)
			CHECK_OCL_ERROR(clReleaseProgram(oclProgram));
	}

	bool IsNull() const { 
		return (oclProgram == nullptr);
	}

	friend class OpenCLDevice;

protected:
	void Set(cl_program p) {
		if (oclProgram)
			CHECK_OCL_ERROR(clReleaseProgram(oclProgram));
		oclProgram = p;
	}

	cl_program Get() { return oclProgram; }

	cl_program oclProgram;
};

//------------------------------------------------------------------------------
// OpenCLDeviceBuffer
//------------------------------------------------------------------------------

class OpenCLDeviceBuffer : public HardwareDeviceBuffer {
public:
	OpenCLDeviceBuffer() : oclBuff(nullptr) { }
	virtual ~OpenCLDeviceBuffer() {
		if (oclBuff)
			CHECK_OCL_ERROR(clReleaseMemObject(oclBuff));
	}

	bool IsNull() const { 
		return (oclBuff == nullptr);
	}

	size_t GetSize() const {
		assert (oclBuff);

		return GetSize(oclBuff);
	}
	
	static size_t GetSize(cl_mem oclBuff) {
		assert (oclBuff);

		size_t value;
		CHECK_OCL_ERROR(clGetMemObjectInfo(oclBuff, CL_MEM_SIZE, sizeof(value), &value, nullptr));

		return value;
	}

	friend class OpenCLDevice;

protected:
	void Set(cl_mem buff) {
		if (oclBuff)
			CHECK_OCL_ERROR(clReleaseMemObject(oclBuff));
		oclBuff = buff;
	}

	cl_mem Get() { return oclBuff; }

	cl_mem oclBuff;
};

//------------------------------------------------------------------------------
// OpenCLDevice
//------------------------------------------------------------------------------

class OpenCLDevice : virtual public HardwareDevice {
public:
	OpenCLDevice(
		ContextConstRef context,
		OpenCLDeviceDescriptionConstRef desc,
		const size_t devIndex
	);
	virtual ~OpenCLDevice();

	virtual DeviceDescriptionConstRef GetDeviceDesc() const override {
		return deviceDesc;
	}

	virtual void PushThreadCurrentDevice() { }
	virtual void PopThreadCurrentDevice() { }

	virtual void Start();
	virtual void Stop();

	//--------------------------------------------------------------------------
	// Kernels handling for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual HardwareDeviceProgramUPtr CompileProgram(
			const std::vector<std::string> &programParameters,
			const std::string &programSource,
			const std::string &programName
	) override;

	// Compile a batch of programs in parallel
	virtual std::vector<HardwareDeviceProgramUPtr> CompilePrograms(
		const std::vector<ProgramRequest> &requests
	) override;

	virtual bool HasEmbeddedSPIRV(const std::vector<std::string> &programParameters,
		const std::string &programSource) const override;

	// Return the parameters used to compile a program: the given parameters
	// plus the device and OS related ones plus the additional compile options
	std::vector<std::string> GetCompleteProgramParameters(
		const std::vector<std::string> &programParameters) const;

	virtual HardwareDeviceKernelUPtr GetKernel(
		HardwareDeviceProgramRef program,
		const std::string &kernelName
	) override;
	virtual u_int GetKernelWorkGroupSize(HardwareDeviceKernelRPtr kernel) override;
	virtual void SetKernelArg(
			HardwareDeviceKernelRPtr kernel,
			const u_int index, const size_t size, const void *arg) override;

	virtual void EnqueueKernel(HardwareDeviceKernelRPtr kernel,
			const HardwareDeviceRange &globalSize,
			const HardwareDeviceRange &workGroupSize) override;
	virtual void EnqueueReadBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, void *ptr) override;
	virtual void EnqueueWriteBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, const void *ptr) override;
	virtual void FlushQueue() override;
	virtual void FinishQueue() override;

	//--------------------------------------------------------------------------
	// Memory management for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual void AllocBuffer(HardwareDeviceBuffer **buff, const BufferType type,
		void *src, const size_t size, const std::string &desc = "");

	virtual void FreeBuffer(HardwareDeviceBuffer **buff);

	friend class Context;

protected:
	virtual void SetKernelArgBuffer(
		HardwareDeviceKernelRPtr kernel,
		const u_int index, const HardwareDeviceBuffer *buff
	) override;

	void AllocBuffer(const cl_mem_flags clFlags, cl_mem *buff,
			void *src, const size_t size, const std::string &desc = "");

	OpenCLDeviceDescriptionConstRef deviceDesc;

	cl_context oclContext;
	cl_command_queue oclQueue;

	std::unique_ptr<luxrays::oclKernelCache> kernelCache;
};

}

#endif

#endif	/* _LUXRAYS_OCLDEVICE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
