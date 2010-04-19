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

#include <boost/interprocess/detail/atomic.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"
#include "luxrays/core/pixel/framebuffer.h"

namespace luxrays {

class PixelDevice : public Device {
public:
	virtual void Init(const unsigned int w, const unsigned int h);
	virtual void Reset() = 0;
	virtual void SetGamma(const float gamma = 2.2f) = 0;

	virtual SampleBuffer *NewSampleBuffer() = 0;
	virtual void PushSampleBuffer(const SampleBuffer *sampleBuffer) = 0;

	virtual void UpdateFrameBuffer() = 0;
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
	void Reset();
	void SetGamma(const float gamma = 2.2f);

	void Start();
	void Interrupt();
	void Stop();

	SampleBuffer *NewSampleBuffer();
	void PushSampleBuffer(const SampleBuffer *sampleBuffer);

	void UpdateFrameBuffer();
	const FrameBuffer *GetFrameBuffer() const { return frameBuffer; }

	static size_t SampleBufferSize;

	friend class Context;

private:
	static const unsigned int GammaTableSize = 1024;

	void AtomicAdd(float *val, const float delta) {
		union bits {
			float f;
			boost::uint32_t i;
		};

		bits oldVal, newVal;
		do {
#if (defined(__i386__) || defined(__amd64__))
	        __asm__ __volatile__ ("pause\n");
#endif
			oldVal.f = *val;
			newVal.f = oldVal.f + delta;
		} while (boost::interprocess::detail::atomic_cas32(((boost::uint32_t *)val), newVal.i, oldVal.i) != oldVal.i);
	}

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = Min<unsigned int>(
			Floor2UInt(GammaTableSize * Clamp(x, 0.f, 1.f)),
				GammaTableSize - 1);
		return gammaTable[index];
	}

	float gammaTable[GammaTableSize];

	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;
};

}

#endif	/* _LUXRAYS_PIXELDEVICE_H */
