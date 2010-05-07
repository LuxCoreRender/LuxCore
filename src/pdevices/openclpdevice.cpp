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

#include <cstdio>
#include <algorithm>

#include "luxrays/kernels/kernels.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/context.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL PixelDevice
//------------------------------------------------------------------------------

OpenCLSampleBuffer::OpenCLSampleBuffer(OpenCLPixelDevice *dev, const size_t bufferSize) : SampleBuffer(bufferSize) {
	device = dev;
	oclBuffer = new cl::Buffer(device->deviceDesc->GetOCLContext(),
		CL_MEM_READ_ONLY,
		sizeof(SampleBufferElem) * OpenCLPixelDevice::SampleBufferSize);
	device->deviceDesc->usedMemory += oclBuffer->getInfo<CL_MEM_SIZE>();
}

OpenCLSampleBuffer::~OpenCLSampleBuffer() {
	device->deviceDesc->usedMemory -= oclBuffer->getInfo<CL_MEM_SIZE>();
	delete oclBuffer;
}

void OpenCLSampleBuffer::Write() const {
	assert (GetSampleCount() <= OpenCLPixelDevice::SampleBufferSize);

	// Download the buffer to the GPU
	device->oclQueue->enqueueWriteBuffer(
			*oclBuffer,
			CL_FALSE,
			0,
			sizeof(SampleBufferElem) * GetSampleCount(),
			GetSampleBuffer());
}

void OpenCLSampleBuffer::Wait() const {
	if (oclEvent())
		oclEvent.wait();
}

void OpenCLSampleBuffer::CollectStats() const {
	if (oclEvent()) {
		device->statsTotalSampleTime += (oclEvent.getProfilingInfo<CL_PROFILING_COMMAND_END>() -
				oclEvent.getProfilingInfo<CL_PROFILING_COMMAND_START>()) * 1e-9;
		device->statsTotalSamplesCount += GetSampleCount();
	}
}

//------------------------------------------------------------------------------

size_t OpenCLPixelDevice::SampleBufferSize = 65536;

OpenCLPixelDevice::OpenCLPixelDevice(const Context *context, OpenCLDeviceDescription *desc,
		const unsigned int index) :
		PixelDevice(context, DEVICE_TYPE_OPENCL, index) {
	deviceDesc  = desc;
	deviceName = (desc->GetName() +"Pixel").c_str();

	sampleFrameBuffer = NULL;
	frameBuffer = NULL;
	sampleFrameBuff = NULL;
	frameBuff = NULL;

	// Initialize Gaussian2x2_filterTable
	const float alpha = 2.f;
	const float expX = expf(-alpha * Gaussian2x2_xWidth * Gaussian2x2_xWidth);
	const float expY = expf(-alpha * Gaussian2x2_yWidth * Gaussian2x2_yWidth);

	float *ftp2x2 = Gaussian2x2_filterTable;
	for (u_int y = 0; y < FilterTableSize; ++y) {
		const float fy = (static_cast<float>(y) + .5f) * Gaussian2x2_yWidth / FilterTableSize;
		for (u_int x = 0; x < FilterTableSize; ++x) {
			const float fx = (static_cast<float>(x) + .5f) * Gaussian2x2_xWidth / FilterTableSize;
			*ftp2x2++ = Max<float>(0.f, expf(-alpha * fx * fx) - expX) *
					Max<float>(0.f, expf(-alpha * fy * fy) - expY);
		}
	}

	// Compile kernels
	cl::Context &oclContext = deviceDesc->GetOCLContext();
	cl::Device &oclDevice = deviceDesc->GetOCLDevice();

	// Allocate the queue for this device
	oclQueue = new cl::CommandQueue(oclContext, oclDevice, CL_QUEUE_PROFILING_ENABLE);

	//--------------------------------------------------------------------------
	// PixelReset kernel
	//--------------------------------------------------------------------------

	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_ClearFB, "PixelClearFB", &clearFBKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_ClearSampleFB, "PixelClearSampleFB", &clearSampleFBKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_AddSampleBuffer, "PixelAddSampleBuffer", &addSampleBufferKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_AddSampleBufferPreview, "PixelAddSampleBufferPreview", &addSampleBufferPreviewKernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_AddSampleBufferGaussian2x2, "PixelAddSampleBufferGaussian2x2", &addSampleBufferGaussian2x2Kernel);
	CompileKernel(oclContext, oclDevice, KernelSource_Pixel_UpdateFrameBuffer, "PixelUpdateFrameBuffer", &updateFrameBufferKernel);

	//--------------------------------------------------------------------------
	// Allocate OpenCL SampleBuffers
	//--------------------------------------------------------------------------

	sampleBuffers.resize(0);
	freeSampleBuffers.resize(0);

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " GammaTable buffer size: " << (sizeof(float) * GammaTableSize / 1024) << "Kbytes");
	gammaTableBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY,
			sizeof(float) * GammaTableSize);
	deviceDesc->usedMemory += gammaTableBuff->getInfo<CL_MEM_SIZE>();
	SetGamma();

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " FilterTable buffer size: " << (sizeof(float) * FilterTableSize  * FilterTableSize / 1024) << "Kbytes");
	filterTableBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY,
			sizeof(float) * FilterTableSize * FilterTableSize);
	deviceDesc->usedMemory += filterTableBuff->getInfo<CL_MEM_SIZE>();
	oclQueue->enqueueWriteBuffer(
			*filterTableBuff,
			CL_FALSE,
			0,
			sizeof(float) * FilterTableSize * FilterTableSize,
			Gaussian2x2_filterTable);
}

OpenCLPixelDevice::~OpenCLPixelDevice() {
	if (started)
		PixelDevice::Stop();

	delete sampleFrameBuffer;
	delete frameBuffer;

	for (size_t i = 0; i < sampleBuffers.size(); ++i)
		delete sampleBuffers[i];

	if (sampleFrameBuff) {
		deviceDesc->usedMemory -= sampleFrameBuff->getInfo<CL_MEM_SIZE>();
		delete sampleFrameBuff;
	}

	if (frameBuff) {
		deviceDesc->usedMemory -= frameBuff->getInfo<CL_MEM_SIZE>();
		delete frameBuff;
	}

	deviceDesc->usedMemory -= filterTableBuff->getInfo<CL_MEM_SIZE>();
	delete filterTableBuff;

	deviceDesc->usedMemory -= gammaTableBuff->getInfo<CL_MEM_SIZE>();
	delete gammaTableBuff;

	delete clearFBKernel;
	delete clearSampleFBKernel;
	delete addSampleBufferKernel;
	delete addSampleBufferPreviewKernel;
	delete addSampleBufferGaussian2x2Kernel;
	delete updateFrameBufferKernel;
	delete oclQueue;
}

void OpenCLPixelDevice::CompileKernel(cl::Context &ctx, cl::Device &device, const std::string &src,
		const char *kernelName, cl::Kernel **kernel) {
	// Compile sources
	cl::Program::Sources source(1, std::make_pair(src.c_str(), src.length()));
	cl::Program program = cl::Program(ctx, source);
	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(device);
		program.build(buildDevice, "-I.");
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] " << kernelName << " compilation error:\n" << strError.c_str());

		throw err;
	}

	*kernel = new cl::Kernel(program, kernelName);
}

void OpenCLPixelDevice::Init(const unsigned int w, const unsigned int h) {
	PixelDevice::Init(w, h);

	//--------------------------------------------------------------------------
	// Free old frame buffers
	//--------------------------------------------------------------------------

	delete sampleFrameBuffer;
	delete frameBuffer;

	if (sampleFrameBuff) {
		deviceDesc->usedMemory -= sampleFrameBuff->getInfo<CL_MEM_SIZE>();
		delete sampleFrameBuff;
	}

	if (frameBuff) {
		deviceDesc->usedMemory -= frameBuff->getInfo<CL_MEM_SIZE>();
		delete frameBuff;
	}

	//--------------------------------------------------------------------------
	// Allocate new frame buffers
	//--------------------------------------------------------------------------

	sampleFrameBuffer = new SampleFrameBuffer(width, height);
	frameBuffer = new FrameBuffer(width, height);

	cl::Context &oclContext = deviceDesc->GetOCLContext();

	// Allocate OpenCL buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " SampleFrameBuffer buffer size: " << (sizeof(SamplePixel) * width * height / 1024) << "Kbytes");
	sampleFrameBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(SamplePixel) * width * height);
	deviceDesc->usedMemory += sampleFrameBuff->getInfo<CL_MEM_SIZE>();

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " FrameBuffer buffer size: " << (sizeof(Pixel) * width * height / 1024) << "Kbytes");
	frameBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Pixel) * width * height);
	deviceDesc->usedMemory += frameBuff->getInfo<CL_MEM_SIZE>();

	ClearSampleFrameBuffer();
	ClearFrameBuffer();
}

void OpenCLPixelDevice::ClearSampleFrameBuffer() {
	clearSampleFBKernel->setArg(0, width);
	clearSampleFBKernel->setArg(1, height);
	clearSampleFBKernel->setArg(2, *sampleFrameBuff);
	oclQueue->enqueueNDRangeKernel(*clearSampleFBKernel, cl::NullRange,
			cl::NDRange(RoundUp(width, 8), RoundUp(height, 8)), cl::NDRange(8, 8));
}

void OpenCLPixelDevice::ClearFrameBuffer() {
	clearFBKernel->setArg(0, width);
	clearFBKernel->setArg(1, height);
	clearFBKernel->setArg(2, *frameBuff);
	oclQueue->enqueueNDRangeKernel(*clearFBKernel, cl::NullRange,
			cl::NDRange(RoundUp(width, 8), RoundUp(height, 8)), cl::NDRange(8, 8));
}

void OpenCLPixelDevice::SetGamma(const float gamma) {
	float x = 0.f;
	const float dx = 1.f / GammaTableSize;
	for (unsigned int i = 0; i < GammaTableSize; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);

	oclQueue->enqueueWriteBuffer(
			*gammaTableBuff,
			CL_FALSE,
			0,
			sizeof(float) * GammaTableSize,
			gammaTable);
}

void OpenCLPixelDevice::Start() {
	boost::mutex::scoped_lock lock(splatMutex);

	PixelDevice::Start();
}

void OpenCLPixelDevice::Interrupt() {
	boost::mutex::scoped_lock lock(splatMutex);
	assert (started);
}

void OpenCLPixelDevice::Stop() {
	boost::mutex::scoped_lock lock(splatMutex);

	oclQueue->finish();
	PixelDevice::Stop();
}

SampleBuffer *OpenCLPixelDevice::GetFreeSampleBuffer() {
	boost::mutex::scoped_lock lock(splatMutex);

	// Look for a free buffer
	if (freeSampleBuffers.size() > 3) {
		OpenCLSampleBuffer *osb = freeSampleBuffers.front();
		freeSampleBuffers.pop_front();

		osb->Wait();
		osb->CollectStats();
		osb->Reset();
		return osb;
	} else {
		// Need to allocate a more buffers
		for (int i = 0; i < 5; ++i) {
			OpenCLSampleBuffer *osb = new OpenCLSampleBuffer(this, SampleBufferSize);
			sampleBuffers.push_back(osb);
			freeSampleBuffers.push_back(osb);
		}

		OpenCLSampleBuffer *osb = new OpenCLSampleBuffer(this, SampleBufferSize);
		sampleBuffers.push_back(osb);

		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " SampleBuffer buffer size: " << (sizeof(SampleBufferElem) * SampleBufferSize / 1024) << "Kbytes (*" << sampleBuffers.size() <<")");

		return osb;
	}
}

void OpenCLPixelDevice::FreeSampleBuffer(SampleBuffer *sampleBuffer) {
	boost::mutex::scoped_lock lock(splatMutex);

	freeSampleBuffers.push_back((OpenCLSampleBuffer *)sampleBuffer);
}

void OpenCLPixelDevice::AddSampleBuffer(const FilterType type, SampleBuffer *sampleBuffer) {
	boost::mutex::scoped_lock lock(splatMutex);
	assert (started);

	OpenCLSampleBuffer *osb = (OpenCLSampleBuffer *)sampleBuffer;

	// Download the buffer to the GPU
	osb->Write();

	// Run the kernel
	*(osb->GetOCLEvent()) = cl::Event();
	switch (type) {
		case FILTER_GAUSSIAN: {
			addSampleBufferGaussian2x2Kernel->setArg(0, width);
			addSampleBufferGaussian2x2Kernel->setArg(1, height);
			addSampleBufferGaussian2x2Kernel->setArg(2, *sampleFrameBuff);
			addSampleBufferGaussian2x2Kernel->setArg(3, (unsigned int)osb->GetSampleCount());
			addSampleBufferGaussian2x2Kernel->setArg(4, *(osb->GetOCLBuffer()));
			addSampleBufferGaussian2x2Kernel->setArg(5, *filterTableBuff);

			oclQueue->enqueueNDRangeKernel(*addSampleBufferGaussian2x2Kernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(64),
				NULL, osb->GetOCLEvent());
			break;
		}
		case FILTER_PREVIEW: {
			addSampleBufferPreviewKernel->setArg(0, width);
			addSampleBufferPreviewKernel->setArg(1, height);
			addSampleBufferPreviewKernel->setArg(2, *sampleFrameBuff);
			addSampleBufferPreviewKernel->setArg(3, (unsigned int)osb->GetSampleCount());
			addSampleBufferPreviewKernel->setArg(4, *(osb->GetOCLBuffer()));

			oclQueue->enqueueNDRangeKernel(*addSampleBufferPreviewKernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(64),
				NULL, osb->GetOCLEvent());
			break;
		}
		case FILTER_NONE: {
			addSampleBufferKernel->setArg(0, width);
			addSampleBufferKernel->setArg(1, height);
			addSampleBufferKernel->setArg(2, *sampleFrameBuff);
			addSampleBufferKernel->setArg(3, (unsigned int)osb->GetSampleCount());
			addSampleBufferKernel->setArg(4, *(osb->GetOCLBuffer()));

			oclQueue->enqueueNDRangeKernel(*addSampleBufferKernel, cl::NullRange,
				cl::NDRange(osb->GetSize()), cl::NDRange(64),
				NULL, osb->GetOCLEvent());
			break;
		}
		default:
			assert (false);
			break;
	}

	freeSampleBuffers.push_back(osb);
}

void OpenCLPixelDevice::UpdateFrameBuffer(const ToneMapParams &params) {
	cl::Event event;

	{
		boost::mutex::scoped_lock lock(splatMutex);

		// Run the kernel
		updateFrameBufferKernel->setArg(0, width);
		updateFrameBufferKernel->setArg(1, height);
		updateFrameBufferKernel->setArg(2, *sampleFrameBuff);
		updateFrameBufferKernel->setArg(3, *frameBuff);
		updateFrameBufferKernel->setArg(4, *gammaTableBuff);

		oclQueue->enqueueNDRangeKernel(*updateFrameBufferKernel, cl::NullRange,
				cl::NDRange(RoundUp(width, 8), RoundUp(height, 8)), cl::NDRange(8, 8));

		oclQueue->enqueueReadBuffer(
				*frameBuff,
				CL_FALSE,
				0,
				sizeof(Pixel) * width * height,
				frameBuffer->GetPixels(),
				NULL,
				&event);
	}

	event.wait();
}

void OpenCLPixelDevice::Merge(const SampleFrameBuffer *sfb) {
	throw std::runtime_error("Internal error: OpenCLPixelDevice::Merge() not yet implemented");
}

const SampleFrameBuffer *OpenCLPixelDevice::GetSampleFrameBuffer() const {
	return sampleFrameBuffer;
}
