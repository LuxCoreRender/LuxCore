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

	// Compile kernels
	cl::Context &oclContext = deviceDesc->GetOCLContext();
	cl::Device &oclDevice = deviceDesc->GetOCLDevice();

	// Allocate the queue for this device
	oclQueue = new cl::CommandQueue(oclContext, oclDevice);

	//--------------------------------------------------------------------------
	// PixelReset kernel
	//--------------------------------------------------------------------------

	{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_Reset.c_str(), KernelSource_Pixel_Reset.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_Reset compilation error:\n" << strError.c_str());

			throw err;
		}

		resetKernel = new cl::Kernel(program, "PixelReset");
		resetKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &resetWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_Reset kernel work group size: " << resetWorkGroupSize);
		cl_ulong memSize;
		resetKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] Pixel_Reset kernel memory footprint: " << memSize);

		resetKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &resetWorkGroupSize);
		LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Suggested work group size: " << resetWorkGroupSize);

		if (forceWorkGroupSize > 0) {
			resetWorkGroupSize = forceWorkGroupSize;
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " Forced work group size: " << resetWorkGroupSize);
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
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] KernelSource_Pixel_AddSampleBuffer compilation error:\n" << strError.c_str());

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
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_UpdateFrameBuffer.c_str(), KernelSource_Pixel_UpdateFrameBuffer.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] KernelSource_Pixel_AddSampleBuffer compilation error:\n" << strError.c_str());

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
				CL_MEM_WRITE_ONLY,
				sizeof(SampleBufferElem) * SampleBufferSize);
		deviceDesc->usedMemory += sampleBuff[i]->getInfo<CL_MEM_SIZE>();
	}

	LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "]" << " GammaTable buffer size: " << (sizeof(float) * GammaTableSize / 1024) << "Kbytes");
	gammaTableBuff = new cl::Buffer(oclContext,
			CL_MEM_WRITE_ONLY,
			sizeof(float) * SampleBufferSize);
	deviceDesc->usedMemory += gammaTableBuff->getInfo<CL_MEM_SIZE>();

	SetGamma();
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

	deviceDesc->usedMemory -= gammaTableBuff->getInfo<CL_MEM_SIZE>();
	delete gammaTableBuff;

	delete resetKernel;
	delete addSampleBufferKernel;
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

	resetKernel->setArg(0, *sampleFrameBuff);
	oclQueue->enqueueNDRangeKernel(*resetKernel, cl::NullRange,
			cl::NDRange(width, height), cl::NDRange(8, 8));
}

void OpenCLPixelDevice::Reset() {
	resetKernel->setArg(0, *sampleFrameBuff);
	oclQueue->enqueueNDRangeKernel(*resetKernel, cl::NullRange,
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
			sizeof(float) * SampleBufferSize,
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

	// Download the buffer to the GPU
	assert (sampleBuffer->GetSampleCount() < SampleBufferSize);
	oclQueue->enqueueWriteBuffer(
			*(sampleBuff[index]),
			CL_TRUE,
			0,
			sizeof(SampleBufferElem) * sampleBuffer->GetSampleCount(),
			sampleBuffer->GetSampleBuffer());

	// Run the kernel
	sampleBuffEvent[index] = cl::Event();
	addSampleBufferKernel->setArg(0, width);
	addSampleBufferKernel->setArg(1, height);
	addSampleBufferKernel->setArg(2, *sampleFrameBuff);
	addSampleBufferKernel->setArg(3, (unsigned int)sampleBuffer->GetSampleCount());
	addSampleBufferKernel->setArg(4, *(sampleBuff[index]));

	oclQueue->enqueueNDRangeKernel(*addSampleBufferKernel, cl::NullRange,
		cl::NDRange(sampleBuffer->GetSize()), cl::NDRange(addSampleBufferWorkGroupSize),
		NULL, &sampleBuffEvent[index]);
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
