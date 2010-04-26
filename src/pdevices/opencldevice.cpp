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

#include "luxrays/kernels/kernels.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/context.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL PixelDevice
//------------------------------------------------------------------------------

size_t OpenCLPixelDevice::SampleBufferSize = OPENCL_SAMPLEBUFFER_SIZE;

OpenCLPixelDevice::OpenCLPixelDevice(const Context *context, OpenCLDeviceDescription *desc,
		const unsigned int index, const unsigned int forceWorkGroupSize) :
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

	{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_ClearFB.c_str(), KernelSource_Pixel_ClearFB.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_ClearFB compilation error:\n" << strError.c_str());

			throw err;
		}

		clearFBKernel = new cl::Kernel(program, "PixelClearFB");
		clearFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &clearFBWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_ClearFB kernel work group size: " << clearFBWorkGroupSize);
		cl_ulong memSize;
		clearFBKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_ClearFB kernel memory footprint: " << memSize);

		clearFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &clearFBWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << clearFBWorkGroupSize);

		if (forceWorkGroupSize > 0) {
			clearFBWorkGroupSize = forceWorkGroupSize;
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Forced work group size: " << clearFBWorkGroupSize);
		}
	}

	{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_ClearSampleFB.c_str(), KernelSource_Pixel_ClearSampleFB.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_ClearSampleFB compilation error:\n" << strError.c_str());

			throw err;
		}

		clearSampleFBKernel = new cl::Kernel(program, "PixelClearSampleFB");
		clearSampleFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &clearSampleFBWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_ClearSampleFB kernel work group size: " << clearSampleFBWorkGroupSize);
		cl_ulong memSize;
		clearSampleFBKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_ClearSampleFB kernel memory footprint: " << memSize);

		clearSampleFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &clearSampleFBWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << clearSampleFBWorkGroupSize);

		if (forceWorkGroupSize > 0) {
			clearSampleFBWorkGroupSize = forceWorkGroupSize;
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Forced work group size: " << clearSampleFBWorkGroupSize);
		}
	}

	{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_AddSampleBuffer.c_str(), KernelSource_Pixel_AddSampleBuffer.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBuffer compilation error:\n" << strError.c_str());

			throw err;
		}

		addSampleBufferKernel = new cl::Kernel(program, "PixelAddSampleBuffer");
		addSampleBufferKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &addSampleBufferWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBuffer kernel work group size: " << addSampleBufferWorkGroupSize);
		cl_ulong memSize;
		addSampleBufferKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBuffer kernel memory footprint: " << memSize);

		addSampleBufferKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &addSampleBufferWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << addSampleBufferWorkGroupSize);

		if (forceWorkGroupSize > 0) {
			addSampleBufferWorkGroupSize = forceWorkGroupSize;
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Forced work group size: " << addSampleBufferWorkGroupSize);
		}
	}

	{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_AddSampleBufferPreview.c_str(), KernelSource_Pixel_AddSampleBufferPreview.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBufferPreview compilation error:\n" << strError.c_str());

			throw err;
		}

		addSampleBufferPreviewKernel = new cl::Kernel(program, "PixelAddSampleBufferPreview");
		addSampleBufferPreviewKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &addSampleBufferPreviewWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBufferPreview kernel work group size: " << addSampleBufferPreviewWorkGroupSize);
		cl_ulong memSize;
		addSampleBufferPreviewKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBufferPreview kernel memory footprint: " << memSize);

		addSampleBufferPreviewKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &addSampleBufferPreviewWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << addSampleBufferPreviewWorkGroupSize);

		if (forceWorkGroupSize > 0) {
			addSampleBufferPreviewWorkGroupSize = forceWorkGroupSize;
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Forced work group size: " << addSampleBufferPreviewWorkGroupSize);
		}
	}

	{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_AddSampleBufferGaussian2x2.c_str(), KernelSource_Pixel_AddSampleBufferGaussian2x2.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBufferGaussian2x2 compilation error:\n" << strError.c_str());

			throw err;
		}

		addSampleBufferGaussian2x2Kernel = new cl::Kernel(program, "PixelAddSampleBufferGaussian2x2");
		addSampleBufferGaussian2x2Kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &addSampleBufferGaussian2x2WorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBufferGaussian2x2 kernel work group size: " << addSampleBufferGaussian2x2WorkGroupSize);
		cl_ulong memSize;
		addSampleBufferGaussian2x2Kernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBufferGaussian2x2 kernel memory footprint: " << memSize);

		addSampleBufferGaussian2x2Kernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &addSampleBufferGaussian2x2WorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << addSampleBufferGaussian2x2WorkGroupSize);

		if (forceWorkGroupSize > 0) {
			addSampleBufferGaussian2x2WorkGroupSize = forceWorkGroupSize;
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Forced work group size: " << addSampleBufferGaussian2x2WorkGroupSize);
		}
	}

	{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_UpdateFrameBuffer.c_str(), KernelSource_Pixel_UpdateFrameBuffer.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_AddSampleBuffer compilation error:\n" << strError.c_str());

			throw err;
		}

		updateFrameBufferKernel = new cl::Kernel(program, "PixelUpdateFrameBuffer");
		updateFrameBufferKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &updateFrameBufferWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_UpdateBuffer kernel work group size: " << updateFrameBufferWorkGroupSize);
		cl_ulong memSize;
		updateFrameBufferKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_UpdateBuffer kernel memory footprint: " << memSize);

		updateFrameBufferKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &updateFrameBufferWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << updateFrameBufferWorkGroupSize);

		if (forceWorkGroupSize > 0) {
			updateFrameBufferWorkGroupSize = forceWorkGroupSize;
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Forced work group size: " << updateFrameBufferWorkGroupSize);
		}
	}

	//--------------------------------------------------------------------------
	// Allocate OpenCL SampleBuffers
	//--------------------------------------------------------------------------

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " SampleBuffer buffer size: " << (sizeof(SampleBufferElem) * SampleBufferSize / 1024) << "Kbytes (*" << SampleBufferCount <<")");
	for (unsigned int i = 0; i < SampleBufferCount; ++i) {
		sampleBuff[i] = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY,
				sizeof(SampleBufferElem) * SampleBufferSize);
		deviceDesc->usedMemory += sampleBuff[i]->getInfo<CL_MEM_SIZE>();
	}

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

	for (unsigned int i = 0; i < SampleBufferCount; ++i) {
		deviceDesc->usedMemory += sampleBuff[i]->getInfo<CL_MEM_SIZE>();
		delete sampleBuff[i];
	}

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

void OpenCLPixelDevice::Init(const unsigned int w, const unsigned int h) {
	PixelDevice::Init(w, h);

	//--------------------------------------------------------------------------
	// Free old buffers
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
	// Allocate new buffers
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
	clearSampleFBKernel->setArg(0, *sampleFrameBuff);
	oclQueue->enqueueNDRangeKernel(*clearSampleFBKernel, cl::NullRange,
			cl::NDRange(width, height), cl::NDRange(8, 8));
}

void OpenCLPixelDevice::ClearFrameBuffer() {
	clearFBKernel->setArg(0, *frameBuff);
	oclQueue->enqueueNDRangeKernel(*clearFBKernel, cl::NullRange,
			cl::NDRange(width, height), cl::NDRange(8, 8));
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
	assert (started);

	boost::mutex::scoped_lock lock(splatMutex);
}

void OpenCLPixelDevice::Stop() {
	boost::mutex::scoped_lock lock(splatMutex);

	PixelDevice::Stop();
}

SampleBuffer *OpenCLPixelDevice::NewSampleBuffer() {
	return new SampleBuffer(SampleBufferSize);
}

void OpenCLPixelDevice::AddSampleBuffer(const FilterType type, const SampleBuffer *sampleBuffer) {
	assert (started);

	boost::mutex::scoped_lock lock(splatMutex);

	// Look for a free SampleBuffer
	unsigned int index = SampleBufferCount;
	for (unsigned int i = 0; i < SampleBufferCount; ++i) {
		if (!(sampleBuffEvent[i]()) || (sampleBuffEvent[i].getInfo<CL_EVENT_COMMAND_EXECUTION_STATUS>() == CL_COMPLETE)) {
			// Ok, found a free buffer
			index = i;
			break;
		}
	}

	if (index == SampleBufferCount) {
		// Huston, we have a problem...
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Unable to find a free SampleBuffer");

		// This should never happen, just wait for one
		index = rand() % SampleBufferCount;
		sampleBuffEvent[index].wait();
	}

	// Colelct the statistics
	if (sampleBuffEvent[index]()) {
		statsTotalSampleTime += (sampleBuffEvent[index].getProfilingInfo<CL_PROFILING_COMMAND_END>() -
			sampleBuffEvent[index].getProfilingInfo<CL_PROFILING_COMMAND_START>()) * 10e-9;
		statsTotalSamplesCount += SampleBufferSize;
	}

	// Download the buffer to the GPU
	assert (sampleBuffer->GetSampleCount() < SampleBufferSize);
	oclQueue->enqueueWriteBuffer(
			*(sampleBuff[index]),
			CL_TRUE, // TO FIX
			0,
			sizeof(SampleBufferElem) * sampleBuffer->GetSampleCount(),
			sampleBuffer->GetSampleBuffer());

	// Run the kernel
	sampleBuffEvent[index] = cl::Event();
	switch (type) {
		case FILTER_GAUSSIAN: {
			addSampleBufferGaussian2x2Kernel->setArg(0, width);
			addSampleBufferGaussian2x2Kernel->setArg(1, height);
			addSampleBufferGaussian2x2Kernel->setArg(2, *sampleFrameBuff);
			addSampleBufferGaussian2x2Kernel->setArg(3, (unsigned int)sampleBuffer->GetSampleCount());
			addSampleBufferGaussian2x2Kernel->setArg(4, *(sampleBuff[index]));
			addSampleBufferGaussian2x2Kernel->setArg(5, FilterTableSize);
			addSampleBufferGaussian2x2Kernel->setArg(6, *filterTableBuff);
			addSampleBufferGaussian2x2Kernel->setArg(7, 16 * addSampleBufferGaussian2x2WorkGroupSize * sizeof(cl_float), NULL);
			addSampleBufferGaussian2x2Kernel->setArg(8, 16 * addSampleBufferGaussian2x2WorkGroupSize * sizeof(cl_float), NULL);

			oclQueue->enqueueNDRangeKernel(*addSampleBufferGaussian2x2Kernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(addSampleBufferGaussian2x2WorkGroupSize),
				NULL, &sampleBuffEvent[index]);
			break;
		}
		case FILTER_PREVIEW: {
			addSampleBufferPreviewKernel->setArg(0, width);
			addSampleBufferPreviewKernel->setArg(1, height);
			addSampleBufferPreviewKernel->setArg(2, *sampleFrameBuff);
			addSampleBufferPreviewKernel->setArg(3, (unsigned int)sampleBuffer->GetSampleCount());
			addSampleBufferPreviewKernel->setArg(4, *(sampleBuff[index]));

			oclQueue->enqueueNDRangeKernel(*addSampleBufferPreviewKernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(addSampleBufferPreviewWorkGroupSize),
				NULL, &sampleBuffEvent[index]);
			break;
		}
		case FILTER_NONE: {
			addSampleBufferKernel->setArg(0, width);
			addSampleBufferKernel->setArg(1, height);
			addSampleBufferKernel->setArg(2, *sampleFrameBuff);
			addSampleBufferKernel->setArg(3, (unsigned int)sampleBuffer->GetSampleCount());
			addSampleBufferKernel->setArg(4, *(sampleBuff[index]));

			oclQueue->enqueueNDRangeKernel(*addSampleBufferKernel, cl::NullRange,
				cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(addSampleBufferWorkGroupSize),
				NULL, &sampleBuffEvent[index]);
			break;
		}
		default:
			assert (false);
			break;
	}
}

void OpenCLPixelDevice::UpdateFrameBuffer() {
	boost::mutex::scoped_lock lock(splatMutex);

	// Run the kernel
	updateFrameBufferKernel->setArg(0, width);
	updateFrameBufferKernel->setArg(1, height);
	updateFrameBufferKernel->setArg(2, *sampleFrameBuff);
	updateFrameBufferKernel->setArg(3, *frameBuff);
	updateFrameBufferKernel->setArg(4, GammaTableSize);
	updateFrameBufferKernel->setArg(5, *gammaTableBuff);

	oclQueue->enqueueNDRangeKernel(*updateFrameBufferKernel, cl::NullRange,
			cl::NDRange(width, height), cl::NDRange(8, 8));

	oclQueue->enqueueReadBuffer(
			*frameBuff,
			CL_TRUE,
			0,
			sizeof(Pixel) * width * height,
			frameBuffer->GetPixels());
}

void OpenCLPixelDevice::Merge(const SampleFrameBuffer *sfb) {
}

const SampleFrameBuffer *OpenCLPixelDevice::GetSampleFrameBuffer() const {
	return sampleFrameBuffer;
}
