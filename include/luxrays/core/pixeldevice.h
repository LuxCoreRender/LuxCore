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

#ifndef _LUXRAYS_PIXELDEVICE_H
#define	_LUXRAYS_PIXELDEVICE_H

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"
#include "luxrays/core/pixel/framebuffer.h"

namespace luxrays {

class PixelDevice : public Device {
public:

	SampleBuffer *NewSampleBuffer() { return new SampleBuffer(); };
	void PushSampleBuffer(SampleBuffer *sampleBuffer) = 0;

	friend class Context;

protected:
	PixelDevice(const Context *context, const DeviceType type, const unsigned int index);
	virtual ~PixelDevice();
};

//------------------------------------------------------------------------------
// Native CPU devices
//------------------------------------------------------------------------------

/*class NativePixelDevice : public PixelDevice {
public:
	NativePixelDevice(const Context *context, const unsigned int devIndex);
	~NativePixelDevice();

	void SetDataSet(const DataSet *newDataSet);
	void Start();
	void Interrupt();
	void Stop();

	RayBuffer *NewRayBuffer();
	size_t GetQueueSize() { return 0; }
	void PushRayBuffer(RayBuffer *rayBuffer);
	RayBuffer *PopRayBuffer();

	double GetLoad() const {
		return 1.0;
	}

	static size_t RayBufferSize;

	friend class Context;

protected:
	static void AddDevices(std::vector<DeviceDescription *> &descriptions);

private:
	RayBufferSingleQueue doneRayBufferQueue;
};*/

}

#endif	/* _LUXRAYS_PIXELDEVICE_H */
