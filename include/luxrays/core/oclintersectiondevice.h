/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _LUXRAYS_OPENCL_INTERSECTIONDEVICE_H
#define	_LUXRAYS_OPENCL_INTERSECTIONDEVICE_H

#include <deque>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/ocldevice.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL devices
//------------------------------------------------------------------------------

class OpenCLKernel {
public:
	OpenCLKernel(OpenCLIntersectionDevice *dev) : device(dev), kernel(NULL),
		stackSize(24) { }
	virtual ~OpenCLKernel() { delete kernel; }

	virtual void FreeBuffers() = 0;
	virtual void UpdateDataSet(const DataSet *newDataSet) = 0;
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) = 0;

	void SetMaxStackSize(const size_t s) { stackSize = s; }

protected:
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

	virtual void SetDataSet(const DataSet *newDataSet);
	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	// OpenCL Device specific methods
	OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }
	virtual size_t GetMaxMemory() const {
		return deviceDesc->GetMaxMemory();
	}

	void DisableImageStorage(const bool v) {
		disableImageStorage = v;
	}

	//--------------------------------------------------------------------------
	// Data parallel interface: to trace large set of rays directly from the GPU
	//--------------------------------------------------------------------------

	virtual RayBuffer *NewRayBuffer();
	virtual RayBuffer *NewRayBuffer(const size_t size);
	virtual size_t GetQueueSize() { return pendingRayBuffers; }
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

	//--------------------------------------------------------------------------
	// Statistics
	//--------------------------------------------------------------------------

	// TODO
	virtual double GetLoad() const { return 1.0; }

	virtual double GetTotalRaysCount() const;
	virtual double GetTotalPerformance() const;
	virtual double GetDataParallelPerformance() const;
	virtual void ResetPerformaceStats();

	friend class Context;

	static size_t RayBufferSize;

protected:
	virtual void UpdateDataSet();

private:
	static void IntersectionThread(OpenCLIntersectionDevice *renderDevice);

	void UpdateTotalDataParallelRayCount() const;

	//--------------------------------------------------------------------------
	// OpenCLDeviceQueue
	//--------------------------------------------------------------------------

	class OpenCLDeviceQueue {
	public:
		OpenCLDeviceQueue(OpenCLIntersectionDevice *device);
		~OpenCLDeviceQueue();

		void UpdateDataSet();

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
			OpenCLDeviceQueueElem(OpenCLIntersectionDevice *device, cl::CommandQueue *oclQueue);
			~OpenCLDeviceQueueElem();

			void UpdateDataSet();

			void PushRayBuffer(RayBuffer *rayBuffer);
			RayBuffer *PopRayBuffer();

			void EnqueueTraceRayBuffer(cl::Buffer &rBuff,  cl::Buffer &hBuff,
			const unsigned int rayCount,
			const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
				kernel->EnqueueRayBuffer(*oclQueue, rBuff, hBuff, rayCount, events, event);
			}

			OpenCLIntersectionDevice *device;
			cl::CommandQueue *oclQueue;

			OpenCLKernel *kernel;

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

		double statsTotalDataParallelRayCount;
	};

	OpenCLDeviceDescription *deviceDesc;

	u_int pendingRayBuffers;

	// OpenCL queues
	vector<OpenCLDeviceQueue *> oclQueues;

	bool reportedPermissionError, disableImageStorage;
};

}

#endif

#endif	/* _LUXRAYS_OPENCL_INTERSECTIONDEVICE_H */

