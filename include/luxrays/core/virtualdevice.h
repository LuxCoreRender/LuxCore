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
// has been replaced by the new IntersectionDevice interface

//------------------------------------------------------------------------------
// Virtual Many to Many hardware device
//------------------------------------------------------------------------------

class VirtualM2MHardwareIntersectionDevice {
public:
	VirtualM2MHardwareIntersectionDevice(const size_t count, const std::vector<HardwareIntersectionDevice *> &devices);
	~VirtualM2MHardwareIntersectionDevice();

	IntersectionDevice *GetVirtualDevice(size_t index);
	// The following methods can be used only if the device has been initialize
	// with count = 0
	IntersectionDevice *AddVirtualDevice();
	// The virtual devices must always be removed in reverse order
	void RemoveVirtualDevice(IntersectionDevice *dev);

	static size_t RayBufferSize;

private:
	class VirtualM2MDevHInstance : public IntersectionDevice {
	public:
		VirtualM2MDevHInstance(VirtualM2MHardwareIntersectionDevice *device, const size_t index);
		~VirtualM2MDevHInstance();

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
			traceRayRealDeviceIndex = (traceRayRealDeviceIndex + 1) % virtualDevice->realDevices.size();
			return virtualDevice->realDevices[traceRayRealDeviceIndex]->TraceRay(ray, rayHit);
		}

		virtual double GetLoad() const { return started ? 1.0 : 0.0; }

		friend class VirtualM2MHardwareIntersectionDevice;

	protected:
		void SetDataSet(const DataSet *newDataSet);
		void Start();
		void Interrupt();
		void Stop();

		size_t instanceIndex;

	private:
		VirtualM2MHardwareIntersectionDevice *virtualDevice;

		std::deque<u_int> pendingRayBufferDeviceIndex;
		// This is used to spread the TraceRay() calls uniformly over
		// all real devices
		u_int traceRayRealDeviceIndex;
	};

	size_t virtualDeviceCount;
	std::vector<HardwareIntersectionDevice *> realDevices;

	boost::mutex virtualDeviceMutex;
	std::vector<VirtualM2MDevHInstance *> virtualDeviceInstances;
};

}

#endif	/* _LUXRAYS_VIRTUALDEVICE_H */
