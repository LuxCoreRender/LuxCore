/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
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

#include <boost/thread/mutex.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/intersectiondevice.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Virtual Many to One hardware device
//------------------------------------------------------------------------------

class VirtualM2OHardwareIntersectionDevice {
public:
	VirtualM2OHardwareIntersectionDevice(const size_t count, HardwareIntersectionDevice *device);
	~VirtualM2OHardwareIntersectionDevice();

	IntersectionDevice *GetVirtualDevice(size_t index);
	IntersectionDevice *AddVirtualDevice();
	void RemoveVirtualDevice(IntersectionDevice *dev);

	static size_t RayBufferSize;

private:
	class VirtualM2ODevHInstance : public IntersectionDevice {
	public:
		VirtualM2ODevHInstance(VirtualM2OHardwareIntersectionDevice *device, const size_t index);
		~VirtualM2ODevHInstance();

		RayBuffer *NewRayBuffer();
		RayBuffer *NewRayBuffer(const size_t size);
		void PushRayBuffer(RayBuffer *rayBuffer);
		RayBuffer *PopRayBuffer();
		size_t GetQueueSize() { return virtualDevice->rayBufferQueue.GetSizeToDo(); }

		void PushRayBufferDone(RayBuffer *rayBuffer);

		double GetLoad() const { return virtualDevice->realDevice->GetLoad(); }

		void SetDataSet(const DataSet *newDataSet);
		void Start();
		void Interrupt();
		void Stop();

	private:
		void StopNoLock();

		size_t instanceIndex;
		VirtualM2OHardwareIntersectionDevice *virtualDevice;

		size_t pendingRayBuffers;
	};

	size_t virtualDeviceCount;
	HardwareIntersectionDevice *realDevice;
	RayBufferQueueM2O rayBufferQueue;

	boost::mutex virtualDeviceMutex;
	std::vector<VirtualM2ODevHInstance *> virtualDeviceInstances;
};

//------------------------------------------------------------------------------
// Virtual Many to Many hardware device
//------------------------------------------------------------------------------

class VirtualM2MHardwareIntersectionDevice {
public:
	VirtualM2MHardwareIntersectionDevice(const size_t count, const std::vector<HardwareIntersectionDevice *> &devices);
	~VirtualM2MHardwareIntersectionDevice();

	IntersectionDevice *GetVirtualDevice(size_t index);

	static size_t RayBufferSize;

private:
	class VirtualM2MDevHInstance : public IntersectionDevice {
	public:
		VirtualM2MDevHInstance(VirtualM2MHardwareIntersectionDevice *device, const size_t index);
		~VirtualM2MDevHInstance();

		RayBuffer *NewRayBuffer();
		RayBuffer *NewRayBuffer(const size_t size);
		void PushRayBuffer(RayBuffer *rayBuffer);
		RayBuffer *PopRayBuffer();
		size_t GetQueueSize() { return virtualDevice->rayBufferQueue.GetSizeToDo(); }

		void PushRayBufferDone(RayBuffer *rayBuffer);

		double GetLoad() const { return 1.0; }

	protected:
		void SetDataSet(const DataSet *newDataSet);
		void Start();
		void Interrupt();
		void Stop();

	private:
		size_t instanceIndex;
		VirtualM2MHardwareIntersectionDevice *virtualDevice;

		size_t pendingRayBuffers;
	};

	size_t virtualDeviceCount;
	std::vector<HardwareIntersectionDevice *> realDevices;
	RayBufferQueueM2M rayBufferQueue;

	boost::mutex virtualDeviceMutex;
	VirtualM2MDevHInstance **virtualDeviceInstances;
};

}

#endif	/* _LUXRAYS_VIRTUALDEVICE_H */
