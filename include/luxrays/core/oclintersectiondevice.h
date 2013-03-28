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
	virtual void EnqueueRayBuffer(cl::Buffer &rBuff, cl::Buffer &hBuff,
		const unsigned int rayCount,
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

	virtual RayBuffer *NewRayBuffer();
	virtual RayBuffer *NewRayBuffer(const size_t size);
	virtual size_t GetQueueSize() { return pendingRayBuffers; }
	virtual void PushRayBuffer(RayBuffer *rayBuffer);
	virtual RayBuffer *PopRayBuffer();

	// OpenCL Device specific methods
	OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }
	virtual size_t GetMaxMemory() const {
		return deviceDesc->GetMaxMemory();
	}

	void DisableImageStorage(const bool v) {
		disableImageStorage = v;
	}

	//--------------------------------------------------------------------------
	// Interface for GPU only applications
	//--------------------------------------------------------------------------

	cl::Context &GetOpenCLContext() { return deviceDesc->GetOCLContext(); }
	cl::Device &GetOpenCLDevice() { return deviceDesc->GetOCLDevice(); }
	cl::CommandQueue &GetOpenCLQueue() { return *(oclQueues[0]); }

	//--------------------------------------------------------------------------
	// Data parallel interface: to trace large set of rays directly from the GPU
	//--------------------------------------------------------------------------

	void EnqueueTraceRayBuffer(cl::Buffer &rBuff,  cl::Buffer &hBuff,
		const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	friend class Context;

	static size_t RayBufferSize;

protected:
	virtual void UpdateDataSet();

private:
	static void IntersectionThread(OpenCLIntersectionDevice *renderDevice);

	void TraceRayBuffer(RayBuffer *rayBuffer,
		VECTOR_CLASS<cl::Event> &readEvent,
		VECTOR_CLASS<cl::Event> &traceEvent, cl::Event *event);
	void FreeDataSetBuffers();

	OpenCLDeviceDescription *deviceDesc;

	u_int pendingRayBuffers;

	// OpenCL items, one for each instanced queue
	u_int queueCount;
	vector<cl::CommandQueue *> oclQueues;
	vector<OpenCLKernel *> kernels;
	vector<cl::Buffer *> raysBuffs;
	vector<cl::Buffer *> hitsBuffs;

	bool reportedPermissionError, disableImageStorage;
};

}

#endif

#endif	/* _LUXRAYS_OPENCL_INTERSECTIONDEVICE_H */

