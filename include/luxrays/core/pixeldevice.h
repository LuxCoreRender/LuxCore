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
#include "luxrays/core/pixel/filter.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Tonemapping
//------------------------------------------------------------------------------

typedef enum {
	TONEMAP_LINEAR, TONEMAP_REINHARD02
} ToneMapType;

class ToneMapParams {
public:
	virtual ToneMapType GetType() const = 0;
	virtual ToneMapParams *Copy() const = 0;
	virtual ~ToneMapParams (){}; 
};

class LinearToneMapParams : public ToneMapParams {
public:
	LinearToneMapParams(const float s = 1.f) {
		scale = s;
	}

	ToneMapType GetType() const { return TONEMAP_LINEAR; }

	ToneMapParams *Copy() const {
		return new LinearToneMapParams(scale);
	}

	float scale;
};

class Reinhard02ToneMapParams : public ToneMapParams {
public:
	Reinhard02ToneMapParams(const float preS = 1.f, const float postS = 1.2f,
			const float b = 3.75f) {
		preScale = preS;
		postScale = postS;
		burn = b;
	}

	ToneMapType GetType() const { return TONEMAP_REINHARD02; }

	ToneMapParams *Copy() const {
		return new Reinhard02ToneMapParams(preScale, postScale, burn);
	}

	float preScale, postScale, burn;
};

//------------------------------------------------------------------------------
// Filtering
//------------------------------------------------------------------------------

typedef enum {
	FILTER_NONE, FILTER_PREVIEW, FILTER_GAUSSIAN
} FilterType;

//------------------------------------------------------------------------------
// Pixel Device
//------------------------------------------------------------------------------

class PixelDevice : public Device {
public:
	virtual void Init(const unsigned int w, const unsigned int h);
	virtual void ClearFrameBuffer() = 0;
	virtual void ClearSampleFrameBuffer() = 0;
	virtual void SetGamma(const float gamma = 2.2f) = 0;

	virtual SampleBuffer *GetFreeSampleBuffer() = 0;
	virtual void FreeSampleBuffer(SampleBuffer *sampleBuffer) = 0;
	virtual void AddSampleBuffer(const FilterType type, SampleBuffer *sampleBuffer) = 0;

	virtual void Merge(const SampleFrameBuffer *sfb) = 0;
	virtual const SampleFrameBuffer *GetSampleFrameBuffer() const = 0;

	virtual void UpdateFrameBuffer(const ToneMapParams &params) = 0;
	virtual const FrameBuffer *GetFrameBuffer() const = 0;

	double GetPerformance() const {
		return (statsTotalSampleTime == 0.0) ?	1.0 : (statsTotalSamplesCount / statsTotalSampleTime);
	}

	friend class Context;

protected:
	PixelDevice(const Context *context, const DeviceType type, const size_t index);
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
			const size_t devIndex);
	~NativePixelDevice();

	void Init(const unsigned int w, const unsigned int h);
	void ClearFrameBuffer();
	void ClearSampleFrameBuffer();
	void SetGamma(const float gamma = 2.2f);

	void Start();
	void Interrupt();
	void Stop();

	SampleBuffer *GetFreeSampleBuffer();
	void FreeSampleBuffer(SampleBuffer *sampleBuffer);
	void AddSampleBuffer(const FilterType type, SampleBuffer *sampleBuffer);

	void Merge(const SampleFrameBuffer *sfb);
	const SampleFrameBuffer *GetSampleFrameBuffer() const;

	void UpdateFrameBuffer(const ToneMapParams &params);
	const FrameBuffer *GetFrameBuffer() const { return frameBuffer; }

	unsigned int GetFreeDevBufferCount() {
		boost::mutex::scoped_lock lock(splatMutex);

		return static_cast<unsigned int>(freeSampleBuffers.size());
	}
	unsigned int GetTotalDevBufferCount() {
		boost::mutex::scoped_lock lock(splatMutex);

		return static_cast<unsigned int>(sampleBuffers.size());
	}

	static size_t SampleBufferSize;

	friend class Context;

private:
	static const unsigned int GammaTableSize = 1024;

	void SplatPreview(const SampleBufferElem *sampleElem);
	void SplatFiltered(const SampleBufferElem *sampleElem);

	void SplatRadiance(const Spectrum radiance, const unsigned int x, const unsigned int y, const float weight = 1.f) {
		const unsigned int offset = x + y * width;
		SamplePixel *sp = &(sampleFrameBuffer->GetPixels()[offset]);

		sp->radiance += weight * radiance;
		sp->weight += weight;
	}

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const float indexf = GammaTableSize * Clamp(x, 0.f, 1.f);
		if (indexf < 0.f)
			return 0.f;

		const unsigned int index = Min<unsigned int>((unsigned int) indexf, GammaTableSize - 1);

		return gammaTable[index];
	}

	boost::mutex splatMutex;
	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;

	std::vector<SampleBuffer *> sampleBuffers;
	std::deque<SampleBuffer *> freeSampleBuffers;

	float gammaTable[GammaTableSize];

	Filter *filter;
	FilterLUTs *filterLUTs;
};

//------------------------------------------------------------------------------
// OpenCL device
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLPixelDevice;

class OpenCLSampleBuffer : public SampleBuffer {
public:
	OpenCLSampleBuffer(OpenCLPixelDevice *dev, const size_t bufferSize);
	~OpenCLSampleBuffer();

	void Write() const;
	void Wait() const;
	void CollectStats() const;

	cl::Buffer *GetOCLBuffer() { return oclBuffer; }
	cl::Event *GetOCLEvent() { return &oclEvent; }

private:
	OpenCLPixelDevice *device;
	cl::Buffer *oclBuffer;
	cl::Event oclEvent;
};

class OpenCLPixelDevice : public PixelDevice {
public:
	OpenCLPixelDevice(const Context *context, OpenCLDeviceDescription *desc,
			const size_t index);
	~OpenCLPixelDevice();

	void Init(const unsigned int w, const unsigned int h);
	void ClearFrameBuffer();
	void ClearSampleFrameBuffer();
	void SetGamma(const float gamma = 2.2f);

	void Start();
	void Interrupt();
	void Stop();

	SampleBuffer *GetFreeSampleBuffer();
	void FreeSampleBuffer(SampleBuffer *sampleBuffer);
	void AddSampleBuffer(const FilterType type, SampleBuffer *sampleBuffer);

	void Merge(const SampleFrameBuffer *sfb);
	const SampleFrameBuffer *GetSampleFrameBuffer() const;

	void UpdateFrameBuffer(const ToneMapParams &params);
	const FrameBuffer *GetFrameBuffer() const { return frameBuffer; }

	const OpenCLDeviceDescription *GetDeviceDesc() const { return deviceDesc; }

	unsigned int GetFreeDevBufferCount() {
		boost::mutex::scoped_lock lock(splatMutex);

		return static_cast<unsigned int>(freeSampleBuffers.size());
	}
	unsigned int GetTotalDevBufferCount() {
		boost::mutex::scoped_lock lock(splatMutex);

		return static_cast<unsigned int>(sampleBuffers.size());
	}

	static size_t SampleBufferSize;

	friend class Context;
	friend class OpenCLSampleBuffer;

private:
	static const unsigned int GammaTableSize = 1024;
	static const unsigned int FilterTableSize = 16;

	void CompileKernel(cl::Context &ctx, cl::Device &device, const std::string &src,
		const char *kernelName, cl::Kernel **kernel);

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
	cl::Kernel *addSampleBufferKernel;
	cl::Kernel *addSampleBufferPreviewKernel;
	cl::Kernel *addSampleBufferGaussian2x2Kernel;
	cl::Kernel *updateFrameBufferKernel;

	// Buffers
	cl::Buffer *sampleFrameBuff;
	cl::Buffer *frameBuff;

	std::vector<OpenCLSampleBuffer *> sampleBuffers;
	std::deque<OpenCLSampleBuffer *> freeSampleBuffers;

	cl::Buffer *gammaTableBuff;
	cl::Buffer *filterTableBuff;

	float gammaTable[GammaTableSize];
	float Gaussian2x2_filterTable[FilterTableSize * FilterTableSize];
};

#endif

}

#endif	/* _LUXRAYS_PIXELDEVICE_H */
