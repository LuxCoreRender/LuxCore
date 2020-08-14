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
#include "luxrays/utils/cudacache.h"

#if !defined(LUXRAYS_DISABLE_CUDA)

#if defined(LUXRAYS_DISABLE_OPENCL)
#error "CUDA support can be enabled only if also OpenCL support is enabled"
#endif

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
	virtual bool HasOutOfCoreMemorySupport() const;

	CUdevice GetCUDADevice() { return cudaDevice; }
	int GetCUDAComputeCapabilityMajor() const;
	int GetCUDAComputeCapabilityMinor() const;
	void SetCUDAUseOptix(const bool v) { useOptix = v; }
	bool GetCUDAUseOptix() const { return useOptix; }

	friend class Context;
	friend class CUDADevice;

protected:
	static void AddDeviceDescs(std::vector<DeviceDescription *> &descriptions);

	size_t cudaDeviceIndex;
	CUdevice cudaDevice;
	bool useOptix;
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
	CUDADeviceProgram() : cudaModule(nullptr) { }
	virtual ~CUDADeviceProgram() {
		// Module is unloaded by the device at the Stop()
	}

	bool IsNull() const { 
		return (cudaModule == nullptr);
	}

	friend class CUDADevice;

protected:
	void Set(CUmodule m) {
		// Module is unloaded by the device at the Stop()

		cudaModule = m;
	}

	CUmodule GetModule() { return cudaModule; }

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

	size_t GetSize() const {
		assert (cudaBuff != 0);

		size_t cudaSize;
		CHECK_CUDA_ERROR(cuMemGetAddressRange(0, &cudaSize, cudaBuff));

		return cudaSize;
	}
	
	//--------------------------------------------------------------------------
	// CUDADevice specific methods
	//--------------------------------------------------------------------------
	
	CUdeviceptr &GetCUDADevicePointer() { return cudaBuff; }

	friend class CUDADevice;

protected:
	CUdeviceptr cudaBuff;
};

//------------------------------------------------------------------------------
// CUDADevice
//------------------------------------------------------------------------------

class CUDADevice : virtual public HardwareDevice {
public:
	CUDADevice(const Context *context,
		CUDADeviceDescription *desc, const size_t devIndex);
	virtual ~CUDADevice();

	virtual const DeviceDescription *GetDeviceDesc() const { return deviceDesc; }

	virtual void PushThreadCurrentDevice();
	virtual void PopThreadCurrentDevice();

	//--------------------------------------------------------------------------
	// Kernels handling for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual void CompileProgram(HardwareDeviceProgram **program,
			const std::vector<std::string> &programParameters, const std::string &programSource,
			const std::string &programName);

	virtual void GetKernel(HardwareDeviceProgram *program,
			HardwareDeviceKernel **kernel,
			const std::string &kernelName);
	virtual u_int GetKernelWorkGroupSize(HardwareDeviceKernel *kernel);
	virtual void SetKernelArg(HardwareDeviceKernel *kernel,
			const u_int index, const size_t size, const void *arg);

	virtual void EnqueueKernel(HardwareDeviceKernel *kernel,
			const HardwareDeviceRange &globalSize,
			const HardwareDeviceRange &workGroupSize);
	virtual void EnqueueReadBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, void *ptr);
	virtual void EnqueueWriteBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, const void *ptr);
	virtual void FlushQueue();
	virtual void FinishQueue();

	//--------------------------------------------------------------------------
	// Memory management for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual void AllocBuffer(HardwareDeviceBuffer **buff, const BufferType type,
		void *src, const size_t size, const std::string &desc = "");
	virtual void FreeBuffer(HardwareDeviceBuffer **buff);

	//--------------------------------------------------------------------------
	// CUDADevice specific methods
	//--------------------------------------------------------------------------

	CUcontext GetCUDAContext() { return cudaContext; }
	luxrays::cudaKernelPersistentCache *GetCUDAKernelCache() { return kernelCache; }
	OptixDeviceContext GetOptixContext() { return optixContext; }

	std::vector<std::string> AddKernelOpts(const std::vector<std::string> &programParameters);
	std::string GetKernelSource(const std::string &kernelSource);

	friend class Context;

protected:
	virtual void SetKernelArgBuffer(HardwareDeviceKernel *kernel,
		const u_int index, const HardwareDeviceBuffer *buff);

	void AllocBuffer(CUdeviceptr *buff,
			void *src, const size_t size, const std::string &desc = "");

	CUDADeviceDescription *deviceDesc;
	CUcontext cudaContext;
	std::vector<CUmodule> loadedModules;
	
	luxrays::cudaKernelPersistentCache *kernelCache;
	
	OptixDeviceContext optixContext;
};

}

#endif

#endif	/* _LUXRAYS_CUDADEVICE_H */
