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

#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/context.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// OpenCL PixelDevice
//------------------------------------------------------------------------------

size_t OpenCLPixelDevice::SampleBufferSize = 4096;

OpenCLPixelDevice::OpenCLPixelDevice(const Context *context, OpenCLDeviceDescription *desc,
		const cl::Device &device, const unsigned int index, const unsigned int forceWorkGroupSize) :
		PixelDevice(context, DEVICE_TYPE_OPENCL, index) {
	deviceName = (device.getInfo<CL_DEVICE_NAME>() + "Pixel").c_str();

	sampleFrameBuffer = NULL;
	frameBuffer = NULL;

	SetGamma();

	// Compile kernels

	/*{
		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_Pixel_Reset.c_str(), KernelSource_Pixel_Reset.length()));
		cl::Program program = cl::Program(*oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(device);
			program.build(buildDevice, "-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName << "] BVH compilation error:\n" << strError.c_str());

			throw err;
		}
	}*/
}

OpenCLPixelDevice::~OpenCLPixelDevice() {
	if (started)
		PixelDevice::Stop();

	delete sampleFrameBuffer;
	delete frameBuffer;
}

void OpenCLPixelDevice::Init(const unsigned int w, const unsigned int h) {
	PixelDevice::Init(w, h);

	delete sampleFrameBuffer;
	delete frameBuffer;

	sampleFrameBuffer = new SampleFrameBuffer(width, height);
	sampleFrameBuffer->Reset();

	frameBuffer = new FrameBuffer(width, height);
}

void OpenCLPixelDevice::Reset() {
	sampleFrameBuffer->Reset();
}

void OpenCLPixelDevice::SetGamma(const float gamma) {
}

void OpenCLPixelDevice::Start() {
	PixelDevice::Start();
}

void OpenCLPixelDevice::Interrupt() {
	assert (started);
}

void OpenCLPixelDevice::Stop() {
	PixelDevice::Stop();
}

SampleBuffer *OpenCLPixelDevice::NewSampleBuffer() {
	return new SampleBuffer(SampleBufferSize);
}

void OpenCLPixelDevice::AddSampleBuffer(const FilterType type, const SampleBuffer *sampleBuffer) {
	assert (started);
}

void OpenCLPixelDevice::UpdateFrameBuffer() {
}

void OpenCLPixelDevice::Merge(const SampleFrameBuffer *sfb) {
}

const SampleFrameBuffer *OpenCLPixelDevice::GetSampleFrameBuffer() const {
	return sampleFrameBuffer;
}
