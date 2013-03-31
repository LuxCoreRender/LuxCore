/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
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

	virtual double GetLoad() const { return started ? 1.0 : 0.0; }

	static size_t RayBufferSize;

protected:
	void SetDataSet(const DataSet *newDataSet);
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
