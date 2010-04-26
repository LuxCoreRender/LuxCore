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
	virtual void AllocateSampleBuffers(const unsigned int count) = 0;

	virtual void Init(const unsigned int w, const unsigned int h);
	virtual void ClearFrameBuffer() = 0;
	virtual void ClearSampleFrameBuffer() = 0;
	virtual void SetGamma(const float gamma = 2.2f) = 0;

	virtual SampleBuffer *GetFreeSampleBuffer() = 0;
	virtual void FreeSampleBuffer(const SampleBuffer *sampleBuffer) = 0;
	virtual void AddSampleBuffer(const FilterType type, const SampleBuffer *sampleBuffer) = 0;

	virtual void Merge(const SampleFrameBuffer *sfb) = 0;
	virtual const SampleFrameBuffer *GetSampleFrameBuffer() const = 0;

	virtual void UpdateFrameBuffer() = 0;
	virtual const FrameBuffer *GetFrameBuffer() const = 0;

	double GetPerformance() const {
		return (statsTotalSampleTime == 0.0) ?	1.0 : (statsTotalSamplesCount / statsTotalSampleTime);
	}

	friend class Context;

protected:
	PixelDevice(const Context *context, const DeviceType type, const unsigned int index);
	virtual ~PixelDevice();

	virtual void Start();

	unsigned int width, height;
	double statsTotalSampleTime, statsTotalSamplesCount;
};

//------------------------------------------------------------------------------
// Native CPU device
//------------------------------------------------------------------------------

class NativePixelDevice : public PixelDevice {
public:
	NativePixelDevice(const Context *context, const size_t threadIndex,
			const unsigned int devIndex);
	~NativePixelDevice();

	void AllocateSampleBuffers(const unsigned int count);

	void Init(const unsigned int w, const unsigned int h);
	void ClearFrameBuffer();
	void ClearSampleFrameBuffer();
	void SetGamma(const float gamma = 2.2f);

	void Start();
	void Interrupt();
	void Stop();

	SampleBuffer *GetFreeSampleBuffer();
	void FreeSampleBuffer(const SampleBuffer *sampleBuffer);
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

	/* Not supported by VisualC++
	static const float Gaussian2x2_xWidth = 2.f;
	static const float Gaussian2x2_yWidth = 2.f;
	static const float Gaussian2x2_invXWidth = 1.f / 2.f;
	static const float Gaussian2x2_invYWidth = 1.f / 2.f;*/

#define Gaussian2x2_xWidth 2.f
#define Gaussian2x2_yWidth 2.f
#define Gaussian2x2_invXWidth (1.f / 2.f)
#define Gaussian2x2_invYWidth (1.f / 2.f)

	void SplatPreview(const SampleBufferElem *sampleElem);
	void SplatGaussian2x2(const SampleBufferElem *sampleElem);

	void SplatRadiance(const Spectrum radiance, const unsigned int x, const unsigned int y, const float weight = 1.f) {
		const unsigned int offset = x + y * width;
		SamplePixel *sp = &(sampleFrameBuffer->GetPixels()[offset]);

		sp->radiance += weight * radiance;
		sp->weight += weight;
	}

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = Min<unsigned int>(
			Floor2UInt(GammaTableSize * Clamp(x, 0.f, 1.f)),
				GammaTableSize - 1);
		return gammaTable[index];
	}

	boost::mutex splatMutex;
	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;

	std::vector<SampleBuffer *> sampleBuffers;
	std::vector<bool> sampleBuffersUsed;

	float gammaTable[GammaTableSize];
	float Gaussian2x2_filterTable[FilterTableSize * FilterTableSize];
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

	void AllocateSampleBuffers(const unsigned int count);

	void Init(const unsigned int w, const unsigned int h);
	void ClearFrameBuffer();
	void ClearSampleFrameBuffer();
	void SetGamma(const float gamma = 2.2f);

	void Start();
	void Interrupt();
	void Stop();

	SampleBuffer *GetFreeSampleBuffer();
	void FreeSampleBuffer(const SampleBuffer *sampleBuffer);
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
	static const unsigned int FilterTableSize = 16;
	static const unsigned int SampleBufferCount = 6;

	OpenCLDeviceDescription *deviceDesc;
	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;

	boost::mutex splatMutex;

	// OpenCL items
	cl::CommandQueue *oclQueue;

	// Kernels
	cl::Kernel *clearFBKernel;
	size_t clearFBWorkGroupSize;

	cl::Kernel *clearSampleFBKernel;
	size_t clearSampleFBWorkGroupSize;

	cl::Kernel *addSampleBufferKernel;
	size_t addSampleBufferWorkGroupSize;
	cl::Kernel *addSampleBufferPreviewKernel;
	size_t addSampleBufferPreviewWorkGroupSize;
	cl::Kernel *addSampleBufferGaussian2x2Kernel;
	size_t addSampleBufferGaussian2x2WorkGroupSize;

	cl::Kernel *updateFrameBufferKernel;
	size_t updateFrameBufferWorkGroupSize;

	// Buffers
	cl::Buffer *sampleFrameBuff;
	cl::Buffer *frameBuff;

	cl::Buffer *sampleBuff[SampleBufferCount];
	cl::Event sampleBuffEvent[SampleBufferCount];

	cl::Buffer *gammaTableBuff;
	cl::Buffer *filterTableBuff;

	float gammaTable[GammaTableSize];
	float Gaussian2x2_filterTable[FilterTableSize * FilterTableSize];
};

#endif

}

#endif	/* _LUXRAYS_PIXELDEVICE_H */
