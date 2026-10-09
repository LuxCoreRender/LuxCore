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

#ifndef _LUXRAYS_HARDWAREDEVICE_H
#define	_LUXRAYS_HARDWAREDEVICE_H

#include "luxrays/core/device.h"
#include "luxrays/usings.h"
#include "luxrays/utils/ocl.h"
#include <functional>

namespace luxrays {

//------------------------------------------------------------------------------
// HardwareDeviceRange
//------------------------------------------------------------------------------

class HardwareDeviceRange {
public:
	HardwareDeviceRange(const size_t s0) {
		sizes[0] = s0;
		sizes[1] = 0;
		sizes[2] = 0;
		dimensions = 1;
	}
	HardwareDeviceRange(const size_t s0, const size_t s1) {
		sizes[0] = s0;
		sizes[1] = s1;
		sizes[2] = 0;
		dimensions = 2;
	}
	HardwareDeviceRange(const size_t s0, const size_t s1, const size_t s2) {
		sizes[0] = s0;
		sizes[1] = s1;
		sizes[2] = s2;
		dimensions = 3;
	}
	virtual ~HardwareDeviceRange() { }

	size_t sizes[3];
	u_int dimensions;
};

//------------------------------------------------------------------------------
// HardwareDeviceKernel: a kernel to execute on a hardware device
//------------------------------------------------------------------------------

class HardwareDeviceKernel {
public:

	// Prevent copying:
	// Device kernels contain allocated resources which are not trivially
	// copyable
	HardwareDeviceKernel(const HardwareDeviceKernel&) = delete;
	HardwareDeviceKernel& operator=(const HardwareDeviceKernel&) = delete;

	virtual ~HardwareDeviceKernel() { }

	virtual bool IsNull() const = 0;

protected:
	HardwareDeviceKernel() { }
};

//------------------------------------------------------------------------------
// HardwareDeviceProgram: a program from where HardwareDeviceKernel
// can be created
//------------------------------------------------------------------------------

class HardwareDeviceProgram {
public:
    // Prevent copying:
	// Device programs contain allocated resources which are not trivially
	// copyable
    HardwareDeviceProgram(const HardwareDeviceProgram&) = delete;
    HardwareDeviceProgram& operator=(const HardwareDeviceProgram&) = delete;

	virtual ~HardwareDeviceProgram() { }

	virtual bool IsNull() const = 0;

protected:
	HardwareDeviceProgram() { }
};

// Create a unique_ptr on a derived type, but with base handler, and
// provide a reference to the derived managed object
template <typename Base, typename Derived>
std::tuple<std::unique_ptr<Base>, Derived&>
CreateUniquePtr(auto&&... args) {

    static_assert(
		std::is_base_of<Base, Derived>::value,
		"Derived must be a subclass of Base"
	);
	auto derivedPtr = std::make_unique<Derived>(std::forward<decltype(args)>(args)...);
	auto derivedRef = std::ref(*derivedPtr);  // We must capture ref before
														 // derivedProgram is moved...
	auto basePtr = static_cast<std::unique_ptr<Base>>(std::move(derivedPtr));

	auto res = std::make_tuple(std::move(basePtr), derivedRef);
	return res;
}


//------------------------------------------------------------------------------
// HardwareDeviceBuffer: a memory region allocated on an hardware device
//------------------------------------------------------------------------------

typedef enum {
	BUFFER_TYPE_NONE = 0,
	BUFFER_TYPE_READ_ONLY = 1 << 0,
	BUFFER_TYPE_READ_WRITE = 1 << 1,
	BUFFER_TYPE_OUT_OF_CORE = 1 << 2
} BufferType;

class HardwareDeviceBuffer {
public:
	virtual ~HardwareDeviceBuffer() { }

	virtual bool IsNull() const = 0;
	
	virtual size_t GetSize() const = 0;

protected:
	HardwareDeviceBuffer() { }
};

//------------------------------------------------------------------------------
// HardwareDevice
//------------------------------------------------------------------------------

class HardwareDevice : virtual public Device {
public:
	//--------------------------------------------------------------------------
	// Kernels handling for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	void SetAdditionalCompileOpts(const std::vector<std::string> &opts);
	const std::vector<std::string> &GetAdditionalCompileOpts();

	virtual HardwareDeviceProgramUPtr CompileProgram(
		const std::vector<std::string> &programParameters,
		const std::string &programSource,
		const std::string &programName
	) = 0;

	// Return true if there is an embedded pre-compiled SPIR-V module matching
	// the given program parameters and source (i.e. if the program can be
	// built by just translating the SPIR-V module instead of compiling the
	// source).
	virtual bool HasEmbeddedSPIRV(const std::vector<std::string> &programParameters,
		const std::string &programSource) const {
		return false;
	}

	// Compile a batch of programs. The default implementation compiles the
	// programs sequentially; devices supporting parallel compilation override
	// this method.
	struct ProgramRequest {
		std::vector<std::string> parameters;
		std::string source;
		std::string name;
	};
	virtual std::vector<HardwareDeviceProgramUPtr> CompilePrograms(
		const std::vector<ProgramRequest> &requests
	) {
		std::vector<HardwareDeviceProgramUPtr> programs;
		programs.reserve(requests.size());
		for (const auto &request : requests)
			programs.push_back(CompileProgram(
					request.parameters, request.source, request.name));
		return programs;
	}

	virtual HardwareDeviceKernelUPtr GetKernel(
		HardwareDeviceProgramRef program,
		const std::string &kernelName
	) = 0;
	virtual u_int GetKernelWorkGroupSize(HardwareDeviceKernelRPtr kernel) = 0;

	virtual void SetKernelArg(HardwareDeviceKernelRPtr kernel,
			const u_int index, const size_t size, const void *arg) = 0;
protected:
	virtual void SetKernelArgBuffer(HardwareDeviceKernelRPtr kernel,
			const u_int index, const HardwareDeviceBuffer *buff) = 0;
public:
	template <typename T>
	void SetKernelArg(HardwareDeviceKernelRPtr kernel, const u_int index, const T &arg) {
		SetKernelArg(kernel, index, KernelArgumentHandler<T>::Size(arg), KernelArgumentHandler<T>::Ptr(arg));
	}

	virtual void EnqueueKernel(HardwareDeviceKernelRPtr kernel,
			const HardwareDeviceRange &globalSize,
			const HardwareDeviceRange &workGroupSize) = 0;
	virtual void EnqueueReadBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, void *ptr) = 0;
	virtual void EnqueueWriteBuffer(const HardwareDeviceBuffer *buff,
			const bool blocking, const size_t size, const void *ptr) = 0;
	virtual void FlushQueue() = 0;
	virtual void FinishQueue() = 0;

	//--------------------------------------------------------------------------
	// Memory management for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	size_t GetUsedMemory() const { return usedMemory; }

	virtual void AllocBuffer(HardwareDeviceBuffer **buff, const BufferType type,
			void *src, const size_t size, const std::string &desc = "") = 0;
	virtual void AllocBufferRO(HardwareDeviceBuffer **buff,
			void *src, const size_t size, const std::string &desc = "") {
		AllocBuffer(buff, BUFFER_TYPE_READ_ONLY, src, size, desc);
	}
	virtual void AllocBufferRW(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc = "") {
		AllocBuffer(buff, BUFFER_TYPE_READ_WRITE, src, size, desc);
	}
	virtual void FreeBuffer(HardwareDeviceBuffer **buff) = 0;

	virtual ~HardwareDevice();

protected:
	template <typename T> struct KernelArgumentHandler {
		static ::size_t Size(const T&) { return sizeof(T); }
		static const T* Ptr(const T& value) { return &value; }
	};

	HardwareDevice();

	void AllocMemory(const size_t s) { usedMemory += s; }
	void FreeMemory(const size_t s) { usedMemory -= s; }

	std::vector<std::string> additionalCompileOpts;
	size_t usedMemory;
};

typedef HardwareDeviceBuffer * HardwareDeviceBufferRPtr;
template <> void HardwareDevice::SetKernelArg<HardwareDeviceBufferRPtr>(HardwareDeviceKernelRPtr kernel, const u_int index, const HardwareDeviceBufferRPtr &buff);

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
