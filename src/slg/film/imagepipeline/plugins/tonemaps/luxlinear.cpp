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

#include <boost/lexical_cast.hpp>

#include "luxrays/utils/serializationutils.h"
#include "slg/kernels/kernels.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/tonemaps/luxlinear.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LuxRender Linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::LuxLinearToneMap)

LuxLinearToneMap::LuxLinearToneMap() {
	sensitivity = 100.f;
	exposure = 1.f / 1000.f;
	fstop = 2.8f;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = NULL;
#endif
}

LuxLinearToneMap::LuxLinearToneMap(const float s, const float e, const float f) {
	sensitivity = s;
	exposure = e;
	fstop = f;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = NULL;
#endif
}

LuxLinearToneMap::~LuxLinearToneMap() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;
#endif
}

float LuxLinearToneMap::GetScale(const float gamma) const {
	return exposure / (fstop * fstop) * sensitivity * .65f / 10.f * powf(118.f / 255.f, gamma);
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void LuxLinearToneMap::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	const float gamma = GetGammaCorrectionValue(film, index);
	const float scale = GetScale(gamma);

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		if (film.HasSamples(hasPN, hasSN, i)) {
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			pixels[i] = scale * pixels[i];
		}
	}
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void LuxLinearToneMap::ApplyOCL(Film &film, const u_int index) {
	if (!applyKernel) {
		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(film, "",
				slg::ocl::KernelSource_tonemap_luxlinear_funcs, "LuxLinearToneMap");

		SLG_LOG("[LuxLinearToneMap] Compiling LuxLinearToneMap_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "LuxLinearToneMap_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		const float gamma = GetGammaCorrectionValue(film, index);
		const float scale = GetScale(gamma);
		applyKernel->setArg(argIndex++, scale);

		const double tEnd = WallClockTime();
		SLG_LOG("[LuxLinearToneMap] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)), cl::NDRange(256));
}
#endif
