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

#include <boost/lexical_cast.hpp>

#include "luxrays/kernels/kernels.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/kernels/kernels.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/tonemaps/reinhard02.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Reinhard02 tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Reinhard02ToneMap)

Reinhard02ToneMap::Reinhard02ToneMap() {
	preScale = 1.f;
	postScale = 1.2f;
	burn = 3.75f;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclAccumBuffer = NULL;

	opRGBValuesReduceKernel = NULL;
	opRGBValueAccumulateKernel = NULL;
	applyKernel = NULL;
#endif
}

Reinhard02ToneMap::Reinhard02ToneMap(const float preS, const float postS, const float b) {
	preScale = preS;
	postScale = postS;
	burn = b;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclAccumBuffer = NULL;

	opRGBValuesReduceKernel = NULL;
	opRGBValueAccumulateKernel = NULL;
	applyKernel = NULL;
#endif
}

Reinhard02ToneMap::~Reinhard02ToneMap() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete opRGBValuesReduceKernel;
	delete opRGBValueAccumulateKernel;
	delete applyKernel;

	delete oclAccumBuffer;
#endif
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void Reinhard02ToneMap::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	RGBColor *rgbPixels = (RGBColor *)pixels;

	const float alpha = .1f;
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	float Ywa = 0.f;
	for (u_int i = 0; i < pixelCount; ++i) {
		if (film.HasSamples(hasPN, hasSN, i) && !rgbPixels[i].IsInf())
			Ywa += logf(Max(rgbPixels[i].Y(), 1e-6f));
	}

	if (pixelCount > 0)
		Ywa = expf(Ywa / pixelCount);

	// Avoid division by zero
	if (Ywa == 0.f)
		Ywa = 1.f;

	const float invB2 = (burn > 0.f) ? 1.f / (burn * burn) : 1e5f;
	const float scale = alpha / Ywa;
	const float preS = scale / preScale;
	const float postS = scale * postScale;

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		if (film.HasSamples(hasPN, hasSN, i)) {
			const float ys = rgbPixels[i].Y() * preS;
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			rgbPixels[i] *= postS * (1.f + ys * invB2) / (1.f + ys);
		}
	}
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void Reinhard02ToneMap::ApplyOCL(Film &film, const u_int index) {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();
	const u_int workSize = RoundUp((pixelCount + 1) / 2, 64u);

	if (!applyKernel) {
		// Allocate buffers
		oclIntersectionDevice = film.oclIntersectionDevice;
		film.ctx->SetVerbose(true);
		oclIntersectionDevice->AllocBufferRW(&oclAccumBuffer, (workSize / 64) * sizeof(float) * 3, "Accumulation");
		film.ctx->SetVerbose(false);

		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				luxrays::ocl::KernelSource_luxrays_types +
				luxrays::ocl::KernelSource_color_types +
				luxrays::ocl::KernelSource_color_funcs +
				slg::ocl::KernelSource_tonemap_reinhard02_funcs +
				slg::ocl::KernelSource_tonemap_reduce_funcs,
				"Reinhard02ToneMap");

		SLG_LOG("[Reinhard02ToneMap] Compiling OpRGBValuesReduce Kernel");
		opRGBValuesReduceKernel = new cl::Kernel(*program, "OpRGBValuesReduce");
		SLG_LOG("[Reinhard02ToneMap] Compiling OpRGBValueAccumulate Kernel");
		opRGBValueAccumulateKernel = new cl::Kernel(*program, "OpRGBValueAccumulate");
		SLG_LOG("[Reinhard02ToneMap] Compiling Reinhard02ToneMap_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "Reinhard02ToneMap_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		opRGBValuesReduceKernel->setArg(argIndex++, film.GetWidth());
		opRGBValuesReduceKernel->setArg(argIndex++, film.GetHeight());
		opRGBValuesReduceKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		opRGBValuesReduceKernel->setArg(argIndex++, *oclAccumBuffer);

		argIndex = 0;
		opRGBValueAccumulateKernel->setArg(argIndex++, workSize / 64);
		opRGBValueAccumulateKernel->setArg(argIndex++, *oclAccumBuffer);

		argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		const float gamma = GetGammaCorrectionValue(film, index);
		applyKernel->setArg(argIndex++, gamma);
		applyKernel->setArg(argIndex++, preScale);
		applyKernel->setArg(argIndex++, postScale);
		applyKernel->setArg(argIndex++, burn);
		applyKernel->setArg(argIndex++, *oclAccumBuffer);

		const double tEnd = WallClockTime();
		SLG_LOG("[Reinhard02ToneMap] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*opRGBValuesReduceKernel,
			cl::NullRange, cl::NDRange(workSize), cl::NDRange(64));
	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*opRGBValueAccumulateKernel,
			cl::NullRange, cl::NDRange(64), cl::NDRange(64));

	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(pixelCount, 256u)), cl::NDRange(256));
}
#endif
