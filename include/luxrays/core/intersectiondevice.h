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
#include "luxrays/core/geometry/raybuffer.h"

namespace luxrays {

class IntersectionDevice : public Device {
public:
	const DataSet *GetDataSet() const { return dataSet; }

	virtual size_t GetQueueSize() = 0;

	void SetMaxStackSize(const size_t s) {
		stackSize = s;
	}

	//--------------------------------------------------------------------------
	// Statistics
	//--------------------------------------------------------------------------

	double GetTotalRaysCount() const { return statsTotalSerialRayCount + statsTotalDataParallelRayCount; }
	double GetTotalPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : ((statsTotalSerialRayCount + statsTotalDataParallelRayCount) / statsTotalRayTime);
	}
	double GetSerialPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : (statsTotalSerialRayCount / statsTotalRayTime);
	}
	double GetDataParallelPerformance() const {
		const double statsTotalRayTime = WallClockTime() - statsStartTime;
		return (statsTotalRayTime == 0.0) ?	1.0 : (statsTotalDataParallelRayCount / statsTotalRayTime);
	}
	void ResetPerformaceStats() {
		statsStartTime = WallClockTime();
		statsTotalSerialRayCount = 0.0;
		statsTotalDataParallelRayCount = 0.0;
	}
	virtual double GetLoad() const = 0;

	//--------------------------------------------------------------------------
	// Data parallel interface: to trace large set of rays (from the CPU)
	//--------------------------------------------------------------------------

	virtual RayBuffer *NewRayBuffer() = 0;
	virtual RayBuffer *NewRayBuffer(const size_t size) = 0;
	virtual void PushRayBuffer(RayBuffer *rayBuffer) = 0;
	virtual RayBuffer *PopRayBuffer() = 0;

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
		return dataSet->Intersect(ray, rayHit);
	}

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
	double statsStartTime, statsTotalSerialRayCount, statsTotalDataParallelRayCount,
		statsDeviceIdleTime, statsDeviceTotalTime;

	size_t stackSize;

	bool dataParallelSupport;
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

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
