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

#ifndef _LUXRAYS_OPENCL_PIXELDEVICE_H
#define	_LUXRAYS_OPENCL_PIXELDEVICE_H

#include "luxrays/core/pixeldevice.h"
#include "luxrays/opencl/opencl.h"

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL device
//------------------------------------------------------------------------------

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

}

#endif	/* _LUXRAYS_OPENCL_PIXELDEVICE_H */

