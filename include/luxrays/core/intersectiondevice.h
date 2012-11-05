/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#ifndef _LUXRAYS_INTERSECTIONDEVICE_H
#define	_LUXRAYS_INTERSECTIONDEVICE_H

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"

namespace luxrays {

class IntersectionDevice : public Device {
public:
	const DataSet *GetDataSet() const { return dataSet; }

	virtual RayBuffer *NewRayBuffer() = 0;
	virtual RayBuffer *NewRayBuffer(const size_t size) = 0;
	virtual void PushRayBuffer(RayBuffer *rayBuffer) = 0;
	virtual RayBuffer *PopRayBuffer() = 0;
	virtual size_t GetQueueSize() = 0;

	double GetPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : (statsTotalRayCount / statsTotalRayTime);
	}
	void ResetPerformaceStats() {
		statsStartTime = WallClockTime();
		statsTotalRayCount = 0;
	}
	virtual double GetLoad() const = 0;

	void SetMaxStackSize(const size_t s) {
		stackSize = s;
	}

	unsigned int GetForceWorkGroupSize() const { return forceWorkGroupSize; }
	void SetForceWorkGroupSize(const unsigned int size) const { forceWorkGroupSize = size; }

	friend class Context;
	friend class VirtualM2OHardwareIntersectionDevice;
	friend class VirtualM2MHardwareIntersectionDevice;

protected:
	IntersectionDevice(const Context *context, const DeviceType type, const size_t index);
	virtual ~IntersectionDevice();

	virtual void SetDataSet(const DataSet *newDataSet);
	virtual void UpdateDataSet() { }
	virtual void Start();

	const DataSet *dataSet;
	double statsStartTime, statsTotalRayCount, statsDeviceIdleTime, statsDeviceTotalTime;

	size_t stackSize;

	mutable unsigned int forceWorkGroupSize;
};

class HardwareIntersectionDevice : public IntersectionDevice {
protected:
	HardwareIntersectionDevice(const Context *context,
		const DeviceType type, const size_t index) :
		IntersectionDevice(context, type, index) { }
	virtual ~HardwareIntersectionDevice() { }

	virtual void SetExternalRayBufferQueue(RayBufferQueue *queue) = 0;

	virtual double GetLoad() const {
		return (statsDeviceTotalTime == 0.0) ? 0.0 : (1.0 - statsDeviceIdleTime / statsDeviceTotalTime);
	}

	friend class VirtualM2OHardwareIntersectionDevice;
	friend class VirtualM2MHardwareIntersectionDevice;
};

//------------------------------------------------------------------------------
// Native thread devices
//------------------------------------------------------------------------------

class NativeThreadIntersectionDevice : public HardwareIntersectionDevice {
public:
	NativeThreadIntersectionDevice(const Context *context,
		const size_t threadIndex, const size_t devIndex);
	virtual ~NativeThreadIntersectionDevice();

	virtual void SetDataSet(const DataSet *newDataSet);
	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	virtual RayBuffer *NewRayBuffer();
	virtual RayBuffer *NewRayBuffer(const size_t size);
	virtual size_t GetQueueSize() { return rayBufferQueue.GetSizeToDo(); }
	virtual void PushRayBuffer(RayBuffer *rayBuffer);
	virtual RayBuffer *PopRayBuffer();

	static size_t RayBufferSize;

	friend class Context;

protected:
	virtual void SetExternalRayBufferQueue(RayBufferQueue *queue);

private:
	static void IntersectionThread(NativeThreadIntersectionDevice *renderDevice);
	boost::thread *intersectionThread;
	RayBufferQueueO2O rayBufferQueue;
	RayBufferQueue *externalRayBufferQueue;
	bool reportedPermissionError;
};

//------------------------------------------------------------------------------
// OpenCL devices
//------------------------------------------------------------------------------

#define OPENCL_RAYBUFFER_SIZE 65536

#if !defined(LUXRAYS_DISABLE_OPENCL)

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
		OpenCLDeviceDescription *desc, const size_t index,
		const unsigned int forceWGSize);
	virtual ~OpenCLIntersectionDevice();

	virtual void SetDataSet(const DataSet *newDataSet);
	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	virtual RayBuffer *NewRayBuffer();
	virtual RayBuffer *NewRayBuffer(const size_t size);
	virtual size_t GetQueueSize() { return rayBufferQueue.GetSizeToDo(); }
	virtual void PushRayBuffer(RayBuffer *rayBuffer);
	virtual RayBuffer *PopRayBuffer();

	// OpenCL Device specific methods
	OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }

	void DisableImageStorage(const bool v) {
		disableImageStorage = v;
	}

	//--------------------------------------------------------------------------
	// Interface for GPU only applications
	//--------------------------------------------------------------------------

	void SetHybridRenderingSupport(const bool v) {
		assert (!started);
		hybridRenderingSupport = v;
	}
	cl::Context &GetOpenCLContext() { return deviceDesc->GetOCLContext(); }
	cl::Device &GetOpenCLDevice() { return deviceDesc->GetOCLDevice(); }
	cl::CommandQueue &GetOpenCLQueue() { return *oclQueue; }
	void EnqueueTraceRayBuffer(cl::Buffer &rBuff,  cl::Buffer &hBuff,
		const unsigned int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	friend class Context;

	static size_t RayBufferSize;

protected:
	virtual void SetExternalRayBufferQueue(RayBufferQueue *queue);
	virtual void UpdateDataSet();

private:
	static void IntersectionThread(OpenCLIntersectionDevice *renderDevice);

	void TraceRayBuffer(RayBuffer *rayBuffer,
		VECTOR_CLASS<cl::Event> &readEvent,
		VECTOR_CLASS<cl::Event> &traceEvent, cl::Event *event);
	void FreeDataSetBuffers();

	OpenCLDeviceDescription *deviceDesc;
	boost::thread *intersectionThread;

	// OpenCL items
	cl::CommandQueue *oclQueue;

	OpenCLKernel *kernel;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;

	RayBufferQueueO2O rayBufferQueue;
	RayBufferQueue *externalRayBufferQueue;

	bool reportedPermissionError, disableImageStorage, hybridRenderingSupport;
};

#endif

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
