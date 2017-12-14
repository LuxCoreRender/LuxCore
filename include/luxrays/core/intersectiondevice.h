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

#ifndef _LUXRAYS_INTERSECTIONDEVICE_H
#define	_LUXRAYS_INTERSECTIONDEVICE_H

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"
#include "luxrays/core/geometry/raybuffer.h"

namespace luxrays {

class IntersectionDevice : public Device {
public:
	const Accelerator *GetAccelerator() const { return accel; }

	virtual size_t GetQueueSize() = 0;

	virtual void SetMaxStackSize(const size_t s) {
		stackSize = s;
	}

	//--------------------------------------------------------------------------
	// Statistics
	//--------------------------------------------------------------------------

	virtual double GetLoad() const {
		if (!started)
			return 0.0;
		return (statsDeviceTotalTime == 0.0) ? 0.0 : (1.0 - statsDeviceIdleTime / statsDeviceTotalTime);
	}

	virtual double GetTotalRaysCount() const { return statsTotalSerialRayCount + statsTotalDataParallelRayCount; }
	virtual double GetTotalPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : ((statsTotalSerialRayCount + statsTotalDataParallelRayCount) / statsTotalRayTime);
	}
	virtual double GetSerialPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : (statsTotalSerialRayCount / statsTotalRayTime);
	}
	virtual double GetDataParallelPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : (statsTotalDataParallelRayCount / statsTotalRayTime);
	}
	virtual void ResetPerformaceStats() {
		statsStartTime = WallClockTime();
		statsTotalSerialRayCount = 0.0;
		statsTotalDataParallelRayCount = 0.0;
	}

	//--------------------------------------------------------------------------
	// Data parallel interface: to trace large set of rays (from the CPU)
	//--------------------------------------------------------------------------

	// Set the number of queues to use (default 1). The device must be in stop state.
	virtual void SetQueueCount(const u_int count);
	virtual u_int GetQueueCount() const { return queueCount; }

	// Used to set the number of buffer allocated on the device. It
	// represents the number of RayBuffer you can push before to have to call
	// pop. Otherwise you end with an exception.
	virtual void SetBufferCount(const u_int count) { assert(!started); bufferCount = count; }
	virtual u_int GetBufferCount() const { return bufferCount; }

	virtual RayBuffer *NewRayBuffer() = 0;
	virtual RayBuffer *NewRayBuffer(const size_t size) = 0;
	// This method is thread safe if each thread uses a different queue
	virtual void PushRayBuffer(RayBuffer *rayBuffer, const u_int queueIndex = 0) = 0;
	// This method is thread safe if each thread uses a different queue
	virtual RayBuffer *PopRayBuffer(const u_int queueIndex = 0) = 0;

	// This method can be used to save resources by disabling the support
	// for PushRayBuffer()/PopRayBuffer() interface. Note: this is really
	// useful only with HardwareIntersectionDevice
	void SetDataParallelSupport(const bool v) {
		assert (!started);
		dataParallelSupport = v;
	}

	//--------------------------------------------------------------------------
	// Serial interface: to trace a single ray (on the CPU)
	//--------------------------------------------------------------------------

	virtual bool TraceRay(const Ray *ray, RayHit *rayHit) {
		statsTotalSerialRayCount += 1.0;
		return accel->Intersect(ray, rayHit);
	}

	friend class Context;
	friend class VirtualIntersectionDevice;

    size_t maxRayBufferSize;

protected:
	IntersectionDevice(const Context *context, const DeviceType type, const size_t index);
	virtual ~IntersectionDevice();

	virtual void SetDataSet(DataSet *newDataSet);
	virtual void Start();

	DataSet *dataSet;
	const Accelerator *accel;
	mutable double statsStartTime, statsTotalSerialRayCount, statsTotalDataParallelRayCount,
		statsDeviceIdleTime, statsDeviceTotalTime;

	u_int queueCount, bufferCount;
	size_t stackSize;

	bool dataParallelSupport;
};

class HardwareIntersectionDevice : public IntersectionDevice {
protected:
	HardwareIntersectionDevice(const Context *context,
		const DeviceType type, const size_t index) :
		IntersectionDevice(context, type, index) { }
	virtual ~HardwareIntersectionDevice() { }

	friend class VirtualIntersectionDevice;
};

//------------------------------------------------------------------------------
// Native thread devices
//------------------------------------------------------------------------------

class NativeThreadIntersectionDevice : public HardwareIntersectionDevice {
public:
	NativeThreadIntersectionDevice(const Context *context, const size_t devIndex);
	virtual ~NativeThreadIntersectionDevice();

	void SetThreadCount(const u_int count) { assert(!started); threadCount = count; }
	u_int GetThreadCount() { return threadCount; }

	virtual void SetDataSet(DataSet *newDataSet);
	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	virtual RayBuffer *NewRayBuffer();
	virtual RayBuffer *NewRayBuffer(const size_t size);
	virtual size_t GetQueueSize() { return rayBufferQueue ? rayBufferQueue->GetSizeToDo() : 0; }
	virtual void PushRayBuffer(RayBuffer *rayBuffer, const u_int queueIndex = 0);
	virtual RayBuffer *PopRayBuffer(const u_int queueIndex = 0);

	//--------------------------------------------------------------------------
	// Statistics
	//--------------------------------------------------------------------------

	virtual double GetLoad() const;

	virtual double GetTotalRaysCount() const;
	virtual double GetTotalPerformance() const;
	virtual double GetDataParallelPerformance() const;
	virtual void ResetPerformaceStats();

	friend class Context;

private:
	void UpdateTotalDataParallelRayCount() const;

	static void IntersectionThread(NativeThreadIntersectionDevice *renderDevice,
			const u_int threadIndex);

	u_int threadCount;
	std::vector<boost::thread *> intersectionThreads;
	RayBufferQueueM2M *rayBufferQueue;
	
	// Per thread statistics
	mutable std::vector<double> threadDeviceIdleTime, threadTotalDataParallelRayCount,
		threadDeviceTotalTime;
	
	bool reportedPermissionError;
};

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
