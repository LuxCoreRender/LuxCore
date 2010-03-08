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
#include "device.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Virtual Many to One device
//------------------------------------------------------------------------------

class VirtualM2OIntersectionDevice {
public:
	VirtualM2OIntersectionDevice(const size_t count, IntersectionDevice *device);
	~VirtualM2OIntersectionDevice();

	IntersectionDevice *GetVirtualDevice(size_t index);

	static size_t RayBufferSize;

private:
	class VirtualM2ODevInstance : public IntersectionDevice {
	public:
		VirtualM2ODevInstance(VirtualM2OIntersectionDevice * device, const size_t index);
		~VirtualM2ODevInstance();

		RayBuffer *NewRayBuffer();
		void PushRayBuffer(RayBuffer *rayBuffer);
		RayBuffer *PopRayBuffer();
		size_t GetQueueSize() { return virtualDevice->realDevice->GetQueueSize(); }

		void PushRayBufferDone(RayBuffer *rayBuffer);

		double GetLoad() const { return virtualDevice->realDevice->GetLoad(); }

	protected:
		void SetDataSet(const DataSet *newDataSet);
		void Start();
		void Interrupt();
		void Stop();

	private:
		size_t instanceIndex;
		VirtualM2OIntersectionDevice *virtualDevice;

		size_t pendingRayBuffers;
		RayBufferSingleQueue doneRayBufferQueue;
	};

	static void RayBufferRouter(VirtualM2OIntersectionDevice *virtualDevice);

	size_t virtualDeviceCount;
	IntersectionDevice *realDevice;

	boost::mutex virtualDeviceMutex;
	VirtualM2ODevInstance **virtualDeviceInstances;

	boost::thread *routerThread;
};

//------------------------------------------------------------------------------
// Virtual One to Many device
//------------------------------------------------------------------------------

class VirtualO2MIntersectionDevice : public IntersectionDevice {
public:
	VirtualO2MIntersectionDevice(std::vector<IntersectionDevice *> devices, const size_t index);
	~VirtualO2MIntersectionDevice();

	RayBuffer *NewRayBuffer();
	void PushRayBuffer(RayBuffer *rayBuffer);
	RayBuffer *PopRayBuffer();
	size_t GetQueueSize() { return todoRayBufferQueue.GetSize(); }

	double GetLoad() const { return 1.0; }

	static size_t RayBufferSize;

protected:
	void SetDataSet(const DataSet *newDataSet);
	void Start();
	void Interrupt();
	void Stop();

private:
	// Funny names ...
	static void PusherRouter(VirtualO2MIntersectionDevice *virtualDevice);
	static void PopperRouter(VirtualO2MIntersectionDevice *virtualDevice, size_t deviceIndex);

	std::vector<IntersectionDevice *> realDevices;

	RayBufferSingleQueue todoRayBufferQueue;
	RayBufferSingleQueue doneRayBufferQueue;

	boost::thread *pusherThread;
	std::vector<boost::thread *> popperThread;
};

}

#endif	/* _LUXRAYS_VIRTUALDEVICE_H */

