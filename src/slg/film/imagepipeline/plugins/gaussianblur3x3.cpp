/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "luxrays/kernels/kernels.h"
#include "slg/kernels/kernels.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/gaussianblur3x3.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// GaussianBlur filter plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::GaussianBlur3x3FilterPlugin)

GaussianBlur3x3FilterPlugin::GaussianBlur3x3FilterPlugin(const float w) : weight(w),
		tmpBuffer(nullptr), tmpBufferSize(0) {
	hardwareDevice = nullptr;
	hwTmpBuffer = nullptr;

	filterXKernel = nullptr;
	filterYKernel = nullptr;
}

GaussianBlur3x3FilterPlugin::GaussianBlur3x3FilterPlugin() : tmpBuffer(nullptr) {
	hardwareDevice = nullptr;
	hwTmpBuffer = nullptr;

	filterXKernel = nullptr;
	filterYKernel = nullptr;
}

GaussianBlur3x3FilterPlugin::~GaussianBlur3x3FilterPlugin() {
	delete[] tmpBuffer;

	delete filterXKernel;
	delete filterYKernel;

	if (hardwareDevice)
		hardwareDevice->FreeBuffer(&hwTmpBuffer);
}

ImagePipelinePlugin *GaussianBlur3x3FilterPlugin::Copy() const {
	return new GaussianBlur3x3FilterPlugin(weight);
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

template<class T> void GaussianBlur3x3FilterPlugin::ApplyBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const T *src,
		T *dst,
		const float aF,
		const float bF,
		const float cF) {
	// Do left edge
	T a;
	T b = src[0];
	T c = src[1];

	const float leftTotF = bF + cF;
	const T bLeftK = bF / leftTotF;
	const T cLeftK = cF / leftTotF;
	dst[0] = bLeftK  * b + cLeftK * c;

    // Main loop
	const float totF = aF + bF + cF;
	const T aK = aF / totF;
	const T bK = bF / totF;
	const T cK = cF / totF;

	for (u_int x = 1; x < filmWidth - 1; ++x) {
		a = b;
		b = c;
		c = src[x + 1];

		dst[x] = aK * a + bK  * b + cK * c;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const T aRightK = aF / rightTotF;
	const T bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[filmWidth - 1] = aRightK  * a + bRightK * b;
}

template<class T> void GaussianBlur3x3FilterPlugin::ApplyBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const T *src,
		T *dst,
		const float aF,
		const float bF,
		const float cF) {
	// Do left edge
	T a;
	T b = src[0];
	T c = src[filmWidth];

	const float leftTotF = bF + cF;
	const T bLeftK = bF / leftTotF;
	const T cLeftK = cF / leftTotF;
	dst[0] = bLeftK  * b + cLeftK * c;

    // Main loop
	const float totF = aF + bF + cF;
	const T aK = aF / totF;
	const T bK = bF / totF;
	const T cK = cF / totF;

    for (u_int y = 1; y < filmHeight - 1; ++y) {
		const unsigned index = y * filmWidth;

		a = b;
		b = c;
		c = src[index + filmWidth];

		dst[index] = aK * a + bK  * b + cK * c;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const T aRightK = aF / rightTotF;
	const T bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[(filmHeight - 1) * filmWidth] = aRightK  * a + bRightK * b;
}

template<class T> void GaussianBlur3x3FilterPlugin::ApplyBlurFilter(const u_int width, const u_int height,
		T *src, T *tmpBuffer,
		const float aF, const float bF, const float cF) {
	
	for (u_int i = 0; i < 3; ++i) {
		#pragma omp parallel for
		for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
				int y = 0; y < height; ++y) {
			const u_int index = y * width;
			GaussianBlur3x3FilterPlugin::ApplyBlurFilterXR1<T>(width, height, &src[index], &tmpBuffer[index],
					aF, bF, cF);
		}

		#pragma omp parallel for
		for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
				int x = 0; x < width; ++x)
			GaussianBlur3x3FilterPlugin::ApplyBlurFilterYR1<T>(width, height, &tmpBuffer[x], &src[x],
					aF, bF, cF);
	}
}

namespace slg {
// Explicit instantiations
template void GaussianBlur3x3FilterPlugin::ApplyBlurFilter(const u_int width, const u_int height,
		float *src, float *tmpBuffer,
		const float aF, const float bF, const float cF);
template void GaussianBlur3x3FilterPlugin::ApplyBlurFilter(const u_int width, const u_int height,
		Spectrum *src, Spectrum *tmpBuffer,
		const float aF, const float bF, const float cF);
}

void GaussianBlur3x3FilterPlugin::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Allocate the temporary buffer if required
	if ((!tmpBuffer) || (width * height != tmpBufferSize)) {
		delete tmpBuffer;

		tmpBufferSize = width * height;
		tmpBuffer = new Spectrum[tmpBufferSize];
	}

	ApplyBlurFilter<Spectrum>(width, height, &pixels[index], &tmpBuffer[index], weight, 1.f, weight);
}

//------------------------------------------------------------------------------
// HardwareDevice version
//------------------------------------------------------------------------------

void GaussianBlur3x3FilterPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType, hash<int> > &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
}

void GaussianBlur3x3FilterPlugin::ApplyHW(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	if (!filterXKernel) {
		film.ctx->SetVerbose(true);

		hardwareDevice = film.hardwareDevice;

		// Allocate OpenCL buffers
		hardwareDevice->AllocBufferRW(&hwTmpBuffer, nullptr, width * height * sizeof(Spectrum), "GaussianBlur3x3");

		// Compile sources
		const double tStart = WallClockTime();

		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D SLG_OPENCL_KERNEL");

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				opts,
				luxrays::ocl::KernelSource_luxrays_types +
				slg::ocl::KernelSource_plugin_gaussianblur3x3_funcs,
				"GaussianBlur3x3FilterPlugin");

		//----------------------------------------------------------------------
		// GaussianBlur3x3FilterPlugin_FilterX kernel
		//----------------------------------------------------------------------

		SLG_LOG("[GaussianBlur3x3FilterPlugin] Compiling GaussianBlur3x3FilterPlugin_FilterX Kernel");
		hardwareDevice->GetKernel(program, &filterXKernel, "GaussianBlur3x3FilterPlugin_FilterX");

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(filterXKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(filterXKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(filterXKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(filterXKernel, argIndex++, hwTmpBuffer);
		hardwareDevice->SetKernelArg(filterXKernel, argIndex++, weight);

		//----------------------------------------------------------------------
		// GaussianBlur3x3FilterPlugin_FilterY kernel
		//----------------------------------------------------------------------

		SLG_LOG("[GaussianBlur3x3FilterPlugin] Compiling GaussianBlur3x3FilterPlugin_FilterY Kernel");
		hardwareDevice->GetKernel(program, &filterYKernel, "GaussianBlur3x3FilterPlugin_FilterY");

		// Set kernel arguments
		argIndex = 0;
		hardwareDevice->SetKernelArg(filterYKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(filterYKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(filterYKernel, argIndex++, hwTmpBuffer);
		hardwareDevice->SetKernelArg(filterYKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(filterYKernel, argIndex++, weight);

		//----------------------------------------------------------------------

		delete program;

		const double tEnd = WallClockTime();
		SLG_LOG("[GaussianBlur3x3FilterPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	for (u_int i = 0; i < 3; ++i) {
		hardwareDevice->EnqueueKernel(filterXKernel, HardwareDeviceRange(RoundUp(height, 256u)),
			HardwareDeviceRange(256));
		hardwareDevice->EnqueueKernel(filterYKernel, HardwareDeviceRange(RoundUp(width, 256u)),
			HardwareDeviceRange(256));
	}
}
