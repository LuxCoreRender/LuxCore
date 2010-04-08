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
	virtual void Init(const unsigned int w, const unsigned int h);

	virtual SampleBuffer *NewSampleBuffer() = 0;
	virtual void PushSampleBuffer(SampleBuffer *sampleBuffer) = 0;

	virtual const FrameBuffer *GetFrameBuffer() const = 0;

protected:
	PixelDevice(const Context *context, const DeviceType type, const unsigned int index);
	virtual ~PixelDevice();

	unsigned int width, height;
};

//------------------------------------------------------------------------------
// Native CPU device
//------------------------------------------------------------------------------

class NativePixelDevice : public PixelDevice {
public:
	NativePixelDevice(const Context *context, const size_t threadIndex,
			const unsigned int devIndex);
	~NativePixelDevice();

	void Init(const unsigned int w, const unsigned int h);

	void Start();
	void Interrupt();
	void Stop();

	SampleBuffer *NewSampleBuffer();
	void PushSampleBuffer(SampleBuffer *sampleBuffer);

	const FrameBuffer *GetFrameBuffer() const;

	static size_t SampleBufferSize;

	friend class Context;

private:
	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;
};

}

#endif	/* _LUXRAYS_PIXELDEVICE_H */
