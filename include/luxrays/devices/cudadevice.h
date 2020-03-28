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

#ifndef _LUXRAYS_CUDADEVICE_H
#define	_LUXRAYS_CUDADEVICE_H

#include "luxrays/core/hardwaredevice.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/cuda.h"

#if defined(LUXRAYS_ENABLE_CUDA)

namespace luxrays {

//------------------------------------------------------------------------------
// CUDADeviceDescription
//------------------------------------------------------------------------------

class CUDADeviceDescription : public DeviceDescription {
public:
	CUDADeviceDescription(CUdevice cudaDevice, const size_t devIndex);
	virtual ~CUDADeviceDescription();

	virtual int GetComputeUnits() const;
	virtual u_int GetNativeVectorWidthFloat() const;
	virtual size_t GetMaxMemory() const;
	virtual size_t GetMaxMemoryAllocSize() const;

	CUdevice GetCUDADevice() { return cudaDevice; }

	friend class Context;
	friend class CUDADevice;

protected:
	static void AddDeviceDescs(std::vector<DeviceDescription *> &descriptions);

	size_t deviceIndex;

	CUdevice cudaDevice;
};

//------------------------------------------------------------------------------
// CUDADeviceKernel
//------------------------------------------------------------------------------

class CUDADeviceKernel : public HardwareDeviceKernel {
public:
	CUDADeviceKernel() : cudaKernel(nullptr) { }
	virtual ~CUDADeviceKernel() {
		for (auto p : args)
			delete [] (char *)p;
	}

	bool IsNull() const { 
		return (cudaKernel == nullptr);
	}

	friend class CUDADevice;

protected:
	void Set(CUfunction kernel) {
		cudaKernel = kernel;
	}

	CUfunction Get() { return cudaKernel; }

	CUfunction cudaKernel;
	std::vector<void *> args;
};

//------------------------------------------------------------------------------
// CUDADeviceProgram
//------------------------------------------------------------------------------

class CUDADeviceProgram : public HardwareDeviceProgram {
public:
	CUDADeviceProgram() : cudaProgram(nullptr), cudaModule(nullptr) { }
	virtual ~CUDADeviceProgram() {
		if (cudaProgram) {
			CHECK_NVRTC_ERROR(nvrtcDestroyProgram(&cudaProgram));
		}
		// Module is unloaded by the device at the Stop()
	}

	bool IsNull() const { 
		return (cudaProgram == nullptr);
	}

	friend class CUDADevice;

protected:
	void Set(nvrtcProgram p, CUmodule m) {
		if (cudaProgram) {
			CHECK_NVRTC_ERROR(nvrtcDestroyProgram(&cudaProgram));
		}

		cudaProgram = p;
		cudaModule = m;
	}

	nvrtcProgram GetProgram() { return cudaProgram; }
	CUmodule GetModule() { return cudaModule; }

	nvrtcProgram cudaProgram;
	CUmodule cudaModule;
};

//------------------------------------------------------------------------------
// CUDADeviceBuffer
//------------------------------------------------------------------------------

class CUDADeviceBuffer : public HardwareDeviceBuffer {
public:
	CUDADeviceBuffer() : cudaBuff(0) { }
	virtual ~CUDADeviceBuffer() {
	}

	bool IsNull() const { 
		return (cudaBuff == 0);
	}

	friend class CUDADevice;

protected:
	CUdeviceptr cudaBuff;
};

//------------------------------------------------------------------------------
// CUDADevice
//------------------------------------------------------------------------------

class CUDADevice : public HardwareDevice {
public:
	CUDADevice(const Context *context,
		CUDADeviceDescription *desc, const size_t devIndex);
	virtual ~CUDADevice();

	virtual void Start();
	virtual void Stop();

	//--------------------------------------------------------------------------
	// Kernels handling for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual void CompileProgram(HardwareDeviceProgram **program,
			const std::string &programParameters, const std::string &programSource,
			const std::string &programName);

	virtual void GetKernel(HardwareDeviceProgram *program,
			HardwareDeviceKernel **kernel,
			const std::string &kernelName);
	virtual void SetKernelArg(HardwareDeviceKernel *kernel,
			const u_int index, const size_t size, const void *arg);

	virtual void EnqueueKernel(HardwareDeviceKernel *kernel,
			const HardwareDeviceRange &workGroupSize,
			const HardwareDeviceRange &globalSize);
	virtual void EnqueueReadBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, void *ptr);
	virtual void EnqueueWriteBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, const void *ptr);
	virtual void FlushQueue();
	virtual void FinishQueue();

	//--------------------------------------------------------------------------
	// Memory management for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual size_t GetMaxMemory() const {
		return deviceDesc->GetMaxMemory();
	}

	virtual void AllocBufferRO(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc = "");
	virtual void AllocBufferRW(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc = "");
	virtual void FreeBuffer(HardwareDeviceBuffer **buff);

	friend class Context;

protected:
	virtual void SetKernelArgBuffer(HardwareDeviceKernel *kernel,
		const u_int index, const HardwareDeviceBuffer *buff);

	void AllocBuffer(CUdeviceptr *buff,
			void *src, const size_t size, const std::string &desc = "");

	CUDADeviceDescription *deviceDesc;
	CUcontext cudaContext;
	std::vector<CUmodule> loadedModules;
};

}

#endif

#endif	/* _LUXRAYS_CUDADEVICE_H */
