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
#include <boost/thread/mutex.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"
#include "luxrays/core/pixel/framebuffer.h"

namespace luxrays {

typedef enum {
	FILTER_NONE, FILTER_PREVIEW, FILTER_GAUSSIAN
} FilterType;

class PixelDevice : public Device {
public:
	virtual void Init(const unsigned int w, const unsigned int h);
	virtual void Reset() = 0;
	virtual void SetGamma(const float gamma = 2.2f) = 0;

	virtual SampleBuffer *NewSampleBuffer() = 0;
	virtual void AddSampleBuffer(const FilterType type, const SampleBuffer *sampleBuffer) = 0;

	virtual void Merge(const SampleFrameBuffer *sfb) = 0;
	virtual const SampleFrameBuffer *GetSampleFrameBuffer() const = 0;

	virtual void UpdateFrameBuffer() = 0;
	virtual const FrameBuffer *GetFrameBuffer() const = 0;

	friend class Context;

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
	void AddSampleBuffer(const FilterType type, const SampleBuffer *sampleBuffer);

	void Merge(const SampleFrameBuffer *sfb);
	const SampleFrameBuffer *GetSampleFrameBuffer() const;

	void UpdateFrameBuffer();
	const FrameBuffer *GetFrameBuffer() const { return frameBuffer; }

	static size_t SampleBufferSize;

	friend class Context;

private:
	static const unsigned int GammaTableSize = 1024;
	static const unsigned int FilterTableSize = 16;

	static const float Gaussian2x2_xWidth = 2.f;
	static const float Gaussian2x2_yWidth = 2.f;
	static const float Gaussian2x2_invXWidth = 1.f / 2.f;
	static const float Gaussian2x2_invYWidth = 1.f / 2.f;

	static float Gaussian2x2_filterTable[FilterTableSize * FilterTableSize];

	void SplatPreview(const SampleBufferElem *sampleElem);
	void SplatGaussian2x2(const SampleBufferElem *sampleElem);

	void SplatRadiance(const Spectrum radiance, const unsigned int x, const unsigned int y, const float weight = 1.f) {
		const unsigned int offset = x + y * width;
		SamplePixel *sp = &(sampleFrameBuffer->GetPixels()[offset]);

		AtomicAdd(&(sp->radiance.r), weight * radiance.r);
		AtomicAdd(&(sp->radiance.g), weight * radiance.g);
		AtomicAdd(&(sp->radiance.b), weight * radiance.b);

		AtomicAdd(&(sp->weight), weight);
	}

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

//------------------------------------------------------------------------------
// OpenCL device
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

#define OPENCL_SAMPLEBUFFER_SIZE 65536

class OpenCLPixelDevice : public PixelDevice {
public:
	OpenCLPixelDevice(const Context *context, OpenCLDeviceDescription *desc,
			const unsigned int index, const unsigned int forceWorkGroupSize);
	~OpenCLPixelDevice();

	void Init(const unsigned int w, const unsigned int h);
	void Reset();
	void SetGamma(const float gamma = 2.2f);

	void Start();
	void Interrupt();
	void Stop();

	SampleBuffer *NewSampleBuffer();
	void AddSampleBuffer(const FilterType type, const SampleBuffer *sampleBuffer);

	void Merge(const SampleFrameBuffer *sfb);
	const SampleFrameBuffer *GetSampleFrameBuffer() const;

	void UpdateFrameBuffer();
	const FrameBuffer *GetFrameBuffer() const { return frameBuffer; }

	const OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }
	unsigned int GetFreeDevBufferCount() const {
		unsigned long freeCount = 0;
		for (unsigned int i = 0; i < SampleBufferCount; ++i) {
			if (!(sampleBuffEvent[i]()) || (sampleBuffEvent[i].getInfo<CL_EVENT_COMMAND_EXECUTION_STATUS>() == CL_COMPLETE))
				++freeCount;
		}

		return freeCount;
	}
	unsigned int GetTotalDevBufferCount() const { return SampleBufferCount; }

	static size_t SampleBufferSize;

	friend class Context;

private:
	static const unsigned int GammaTableSize = 1024;
	static const unsigned int SampleBufferCount = 8;

	OpenCLDeviceDescription *deviceDesc;
	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;

	boost::mutex splatMutex;

	// OpenCL items
	cl::CommandQueue *oclQueue;

	// Kernels
	cl::Kernel *resetKernel;
	size_t resetWorkGroupSize;

	cl::Kernel *addSampleBufferKernel;
	size_t addSampleBufferWorkGroupSize;

	cl::Kernel *updateFrameBufferKernel;
	size_t updateFrameBufferWorkGroupSize;

	// Buffers
	cl::Buffer *sampleFrameBuff;
	cl::Buffer *frameBuff;

	cl::Buffer *sampleBuff[SampleBufferCount];
	cl::Event sampleBuffEvent[SampleBufferCount];

	cl::Buffer *gammaTableBuff;
	float gammaTable[GammaTableSize];
};

#endif

}

#endif	/* _LUXRAYS_PIXELDEVICE_H */
