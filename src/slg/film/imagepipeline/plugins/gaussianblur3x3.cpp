/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
		tmpBuffer(NULL), tmpBufferSize(0) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclTmpBuffer = NULL;

	filterXKernel = NULL;
	filterYKernel = NULL;
#endif
}

GaussianBlur3x3FilterPlugin::GaussianBlur3x3FilterPlugin() : tmpBuffer(NULL) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclTmpBuffer = NULL;

	filterXKernel = NULL;
	filterYKernel = NULL;
#endif
}

GaussianBlur3x3FilterPlugin::~GaussianBlur3x3FilterPlugin() {
	delete[] tmpBuffer;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete filterXKernel;
	delete filterYKernel;

	if (oclIntersectionDevice)
		oclIntersectionDevice->FreeBuffer(&oclTmpBuffer);
#endif
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
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void GaussianBlur3x3FilterPlugin::ApplyOCL(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	if (!filterXKernel) {
		oclIntersectionDevice = film.oclIntersectionDevice;

		// Allocate OpenCL buffers
		film.ctx->SetVerbose(true);
		oclIntersectionDevice->AllocBufferRW(&oclTmpBuffer, width * height * sizeof(Spectrum), "GaussianBlur3x3");
		film.ctx->SetVerbose(false);

		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				luxrays::ocl::KernelSource_luxrays_types +
				slg::ocl::KernelSource_plugin_gaussianblur3x3_funcs,
				"GaussianBlur3x3FilterPlugin");

		//----------------------------------------------------------------------
		// GaussianBlur3x3FilterPlugin_FilterX kernel
		//----------------------------------------------------------------------

		SLG_LOG("[GaussianBlur3x3FilterPlugin] Compiling GaussianBlur3x3FilterPlugin_FilterX Kernel");
		filterXKernel = new cl::Kernel(*program, "GaussianBlur3x3FilterPlugin_FilterX");

		// Set kernel arguments
		u_int argIndex = 0;
		filterXKernel->setArg(argIndex++, width);
		filterXKernel->setArg(argIndex++, height);
		filterXKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		filterXKernel->setArg(argIndex++, *oclTmpBuffer);
		filterXKernel->setArg(argIndex++, weight);

		//----------------------------------------------------------------------
		// GaussianBlur3x3FilterPlugin_FilterY kernel
		//----------------------------------------------------------------------

		SLG_LOG("[GaussianBlur3x3FilterPlugin] Compiling GaussianBlur3x3FilterPlugin_FilterY Kernel");
		filterYKernel = new cl::Kernel(*program, "GaussianBlur3x3FilterPlugin_FilterY");

		// Set kernel arguments
		argIndex = 0;
		filterYKernel->setArg(argIndex++, width);
		filterYKernel->setArg(argIndex++, height);
		filterYKernel->setArg(argIndex++, *oclTmpBuffer);
		filterYKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		filterYKernel->setArg(argIndex++, weight);

		//----------------------------------------------------------------------

		delete program;

		const double tEnd = WallClockTime();
		SLG_LOG("[GaussianBlur3x3FilterPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	for (u_int i = 0; i < 3; ++i) {
		oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*filterXKernel,
				cl::NullRange, cl::NDRange(RoundUp(height, 256u)), cl::NDRange(256));
		oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*filterYKernel,
				cl::NullRange, cl::NDRange(RoundUp(width, 256u)), cl::NDRange(256));
	}
}
#endif
