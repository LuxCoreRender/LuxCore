/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

class OpenCLKernels {
public:
	OpenCLKernels(OpenCLIntersectionDevice *dev, const u_int count) :
		device(dev), stackSize(24) {
		kernels.resize(count, NULL);
	}
	virtual ~OpenCLKernels() {
		BOOST_FOREACH(cl::Kernel *kernel, kernels) {
			delete kernel;
		}
	}

	virtual void Update(const DataSet *newDataSet) = 0;
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) = 0;

	const std::string &GetIntersectionKernelSource() { return intersectionKernelSource; }
	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex) { return 0; }

	void SetMaxStackSize(const size_t s) { stackSize = s; }

protected:
	std::string intersectionKernelSource;

	OpenCLIntersectionDevice *device;
	std::vector<cl::Kernel *> kernels;
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
	virtual void Interrupt();
	virtual void Stop();

	// OpenCL Device specific methods
	OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }
	virtual size_t GetMaxMemory() const {
		return deviceDesc->GetMaxMemory();
	}

	//--------------------------------------------------------------------------
	// Data parallel interface: to trace large set of rays directly from the GPU
	//--------------------------------------------------------------------------

	virtual RayBuffer *NewRayBuffer();
	virtual RayBuffer *NewRayBuffer(const size_t size);
	virtual size_t GetQueueSize();
	virtual void PushRayBuffer(RayBuffer *rayBuffer, const u_int queueIndex = 0);
	virtual RayBuffer *PopRayBuffer(const u_int queueIndex = 0);

	//--------------------------------------------------------------------------
	// Interface for GPU only applications
	//--------------------------------------------------------------------------

	cl::Context &GetOpenCLContext() { return deviceDesc->GetOCLContext(); }
	cl::Device &GetOpenCLDevice() { return deviceDesc->GetOCLDevice(); }
	cl::CommandQueue &GetOpenCLQueue(const u_int queueIndex = 0) { return *(oclQueues[queueIndex]->oclQueue); }

	void EnqueueTraceRayBuffer(cl::Buffer &rBuff,  cl::Buffer &hBuff,
		const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event,
		const u_int queueIndex = 0) {
		// Enqueue the intersection kernel
		oclQueues[queueIndex]->EnqueueTraceRayBuffer(rBuff, hBuff, rayCount, events, event);
	}

	// To compile the this device intersection kernel inside application kernel
	const std::string &GetIntersectionKernelSource() { return kernels->GetIntersectionKernelSource(); }
	u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex) {
		return kernels->SetIntersectionKernelArgs(kernel, argIndex);
	}
	void IntersectionKernelExecuted(const u_int rayCount, const u_int queueIndex = 0) {
		oclQueues[queueIndex]->statsTotalDataParallelRayCount += rayCount;
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

	//--------------------------------------------------------------------------
	// Statistics
	//--------------------------------------------------------------------------

	virtual double GetLoad() const;

	virtual double GetTotalRaysCount() const;
	virtual double GetTotalPerformance() const;
	virtual double GetDataParallelPerformance() const;
	virtual void ResetPerformaceStats();

	friend class Context;

protected:
	virtual void Update();

private:
	static void IntersectionThread(OpenCLIntersectionDevice *renderDevice);

	void UpdateCounters() const;

	//--------------------------------------------------------------------------
	// OpenCLDeviceQueue
	//--------------------------------------------------------------------------

	class OpenCLDeviceQueue {
	public:
		OpenCLDeviceQueue(OpenCLIntersectionDevice *device, const u_int kernelIndexOffset, size_t rayBufferSize);
		~OpenCLDeviceQueue();

		void PushRayBuffer(RayBuffer *rayBuffer);
		RayBuffer *PopRayBuffer();

		void EnqueueTraceRayBuffer(cl::Buffer &rBuff,  cl::Buffer &hBuff,
			const unsigned int rayCount,
			const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
			freeElem[0]->EnqueueTraceRayBuffer(rBuff, hBuff, rayCount, events, event);
			statsTotalDataParallelRayCount += rayCount;
		}

		class OpenCLDeviceQueueElem {
		public:
			OpenCLDeviceQueueElem(OpenCLIntersectionDevice *device, cl::CommandQueue *oclQueue,
					const u_int kernelIndex, const size_t rayBufferSize);
			~OpenCLDeviceQueueElem();

			void PushRayBuffer(RayBuffer *rayBuffer);
			RayBuffer *PopRayBuffer();

			void EnqueueTraceRayBuffer(cl::Buffer &rBuff,  cl::Buffer &hBuff,
					const unsigned int rayCount,
					const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
				device->kernels->EnqueueRayBuffer(*oclQueue, kernelIndex, rBuff, hBuff, rayCount, events, event);
			}

			OpenCLIntersectionDevice *device;
			cl::CommandQueue *oclQueue;

			u_int kernelIndex;

			// Free buffers and events
			cl::Buffer *rayBuff;
			cl::Buffer *hitBuff;
			cl::Event *event;
			RayBuffer *pendingRayBuffer;
		};

		OpenCLIntersectionDevice *device;
		cl::CommandQueue *oclQueue;

		std::deque<OpenCLDeviceQueueElem *> freeElem;
		std::deque<OpenCLDeviceQueueElem *> busyElem;

		u_int pendingRayBuffers;
		double lastTimeEmptyQueue;

		double statsTotalDataParallelRayCount, statsDeviceIdleTime;
	};

	OpenCLDeviceDescription *deviceDesc;

	// OpenCL queues
	std::vector<OpenCLDeviceQueue *> oclQueues;
	OpenCLKernels *kernels;
};

}

#endif

#endif	/* _LUXRAYS_OPENCL_INTERSECTIONDEVICE_H */

