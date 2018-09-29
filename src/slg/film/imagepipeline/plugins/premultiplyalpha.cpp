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

#include "slg/film/film.h"
#include "slg/kernels/kernels.h"
#include "slg/film/imagepipeline/plugins/premultiplyalpha.h"
#include "luxrays/kernels/kernels.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Premultiply correction plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::PremultiplyAlphaPlugin)

PremultiplyAlphaPlugin::PremultiplyAlphaPlugin() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = NULL;
#endif
}

PremultiplyAlphaPlugin::~PremultiplyAlphaPlugin() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;
#endif
}

ImagePipelinePlugin *PremultiplyAlphaPlugin::Copy() const {
	return new PremultiplyAlphaPlugin();
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void PremultiplyAlphaPlugin::Apply(Film &film, const u_int index) {
	if (!film.HasChannel(Film::ALPHA)) {
		// I can not work without alpha channel
		return;
	}

	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	#pragma omp parallel for
	for (
		// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
		unsigned
#endif
		int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			const u_int filmPixelIndex = x + y * width;

			if (film.HasSamples(hasPN, hasSN, filmPixelIndex)) {
				float alpha;
				film.channel_ALPHA->GetWeightedPixel(x, y, &alpha);

				pixels[filmPixelIndex] *= alpha;
			}
		}
	}
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void PremultiplyAlphaPlugin::ApplyOCL(Film &film, const u_int index) {
	if (!film.HasChannel(Film::ALPHA)) {
		// I can not work without alpha channel
		return;
	}

	if (!applyKernel) {
		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				slg::ocl::KernelSource_utils_funcs +
				slg::ocl::KernelSource_plugin_premultiplyalpha_funcs,
				"PremultiplyAlphaPlugin");

		SLG_LOG("[PremultiplyAlphaPlugin] Compiling PremultiplyAlphaPlugin_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "PremultiplyAlphaPlugin_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		applyKernel->setArg(argIndex++, *(film.ocl_ALPHA));

		const double tEnd = WallClockTime();
		SLG_LOG("[PremultiplyAlphaPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)), cl::NDRange(256));
}
#endif
