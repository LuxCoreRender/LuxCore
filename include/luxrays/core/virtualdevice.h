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

#ifndef _LUXRAYS_VIRTUALDEVICE_H
#define	_LUXRAYS_VIRTUALDEVICE_H

#include <deque>
#include <boost/thread/mutex.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/intersectiondevice.h"

namespace luxrays {

// The old VirtualM2OHardwareIntersectionDevice (Virtual Many to One hardware device)
// has been replaced by the new IntersectionDevice interface.

// The old VirtualM2MHardwareIntersectionDevice has been replaced by the following
// generic virtual device.

//------------------------------------------------------------------------------
// Virtual device (used mainly to see multiple device as a single one)
//------------------------------------------------------------------------------

class VirtualIntersectionDevice : public IntersectionDevice {
public:
	VirtualIntersectionDevice(const std::vector<IntersectionDevice *> &devices,
			const size_t index);
	~VirtualIntersectionDevice();

	const std::vector<IntersectionDevice *> &GetRealDevices() const { return realDevices; }

	virtual void SetMaxStackSize(const size_t s);
	virtual void SetQueueCount(const u_int count);
	virtual void SetBufferCount(const u_int count);

	virtual RayBuffer *NewRayBuffer();
	virtual RayBuffer *NewRayBuffer(const size_t size);
	virtual void PushRayBuffer(RayBuffer *rayBuffer, const u_int queueIndex = 0);
	virtual RayBuffer *PopRayBuffer(const u_int queueIndex = 0);
	virtual size_t GetQueueSize() { return pendingRayBufferDeviceIndex.size(); }

	virtual bool TraceRay(const Ray *ray, RayHit *rayHit) {
		// Update this device statistics
		statsTotalSerialRayCount += 1.0;

		// Using the underlay real devices mostly to account for
		// statsTotalSerialRayCount statistics
		traceRayRealDeviceIndex = (traceRayRealDeviceIndex + 1) % realDevices.size();
		return realDevices[traceRayRealDeviceIndex]->TraceRay(ray, rayHit);
	}

	//--------------------------------------------------------------------------
	// Statistics
	//--------------------------------------------------------------------------

	virtual double GetLoad() const;
	virtual void ResetPerformaceStats();

protected:
	void SetDataSet(DataSet *newDataSet);
	void Start();
	void Interrupt();
	void Stop();

private:
	std::vector<IntersectionDevice *> realDevices;

	std::vector<std::deque<u_int> > pendingRayBufferDeviceIndex;

	// This is used to spread the TraceRay() calls uniformly over
	// all real devices
	u_int traceRayRealDeviceIndex;
};

}

#endif	/* _LUXRAYS_VIRTUALDEVICE_H */
