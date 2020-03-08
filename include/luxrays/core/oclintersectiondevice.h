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

#ifndef _LUXRAYS_OPENCL_INTERSECTIONDEVICE_H
#define	_LUXRAYS_OPENCL_INTERSECTIONDEVICE_H

#include <deque>
#include <boost/foreach.hpp>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/ocldevice.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL devices
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

class OpenCLIntersectionDevice : public HardwareIntersectionDevice {
public:
	OpenCLIntersectionDevice(const Context *context,
		OpenCLDeviceDescription *desc, const size_t index);
	virtual ~OpenCLIntersectionDevice();

	virtual void SetDataSet(DataSet *newDataSet);
	virtual void Start();
	virtual void Stop();

	// OpenCL Device specific methods
	OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }
	virtual size_t GetMaxMemory() const {
		return deviceDesc->GetMaxMemory();
	}

	//--------------------------------------------------------------------------
	// Interface for GPU only applications
	//--------------------------------------------------------------------------

	cl::Context &GetOpenCLContext() { return deviceDesc->GetOCLContext(); }
	cl::Device &GetOpenCLDevice() { return deviceDesc->GetOCLDevice(); }
	cl::CommandQueue &GetOpenCLQueue() { return *oclQueue; }

	void EnqueueTraceRayBuffer(cl::Buffer &rBuff,  cl::Buffer &hBuff,
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

	//--------------------------------------------------------------------------
	// Memory allocation for GPU only applications
	//--------------------------------------------------------------------------

	void AllocBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
			void *src, const size_t size, const std::string &desc = "");
	void AllocBufferRO(cl::Buffer **buff, void *src, const size_t size, const std::string &desc = "");
	void AllocBufferRO(cl::Buffer **buff, const size_t size, const std::string &desc = "");
	void AllocBufferRW(cl::Buffer **buff, void *src, const size_t size, const std::string &desc = "");
	void AllocBufferRW(cl::Buffer **buff, const size_t size, const std::string &desc = "");
	void FreeBuffer(cl::Buffer **buff);

	friend class Context;

protected:
	virtual void Update();

private:
	OpenCLDeviceDescription *deviceDesc;

	cl::CommandQueue *oclQueue;

	OpenCLKernel *kernel;
};

}

#endif

#endif	/* _LUXRAYS_OPENCL_INTERSECTIONDEVICE_H */

