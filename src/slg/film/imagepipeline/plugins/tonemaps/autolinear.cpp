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
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Auto-linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::AutoLinearToneMap)

AutoLinearToneMap::AutoLinearToneMap() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclAccumBuffer = NULL;

	opRGBValuesReduceKernel = NULL;
	opRGBValueAccumulateKernel = NULL;
	applyKernel = NULL;
#endif
}

AutoLinearToneMap::~AutoLinearToneMap() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete opRGBValuesReduceKernel;
	delete opRGBValueAccumulateKernel;
	delete applyKernel;

	delete oclAccumBuffer;
#endif
}

float AutoLinearToneMap::CalcLinearToneMapScale(const Film &film, const u_int index, const float Y) {
	const float gamma = GetGammaCorrectionValue(film, index);

	// Substitute exposure, fstop and sensitivity cancel out; collect constants
	const float scale = (1.25f / Y * powf(118.f / 255.f, gamma));

	return scale;
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void AutoLinearToneMap::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	float Y = 0.f;
	for (u_int i = 0; i < pixelCount; ++i) {
		if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(i))) {
			const float y = pixels[i].Y();
			if ((y <= 0.f) || isinf(y))
				continue;

			Y += y;
		}
	}
	Y /= pixelCount;

	if (Y <= 0.f)
		return;

	const float scale = CalcLinearToneMapScale(film, index, Y);

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(i)))
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			pixels[i] = scale * pixels[i];
	}
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void AutoLinearToneMap::ApplyOCL(Film &film, const u_int index) {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();
	const u_int workSize = RoundUp((pixelCount + 1) / 2, 64u);

	if (!applyKernel) {
		// Allocate buffers
		oclIntersectionDevice = film.oclIntersectionDevice;
		film.ctx->SetVerbose(true);
		oclIntersectionDevice->AllocBufferRW(&oclAccumBuffer, (workSize / 64) * sizeof(float) * 3, "Accumulation buffer");
		film.ctx->SetVerbose(false);

		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				slg::ocl::KernelSource_luxrays_types +
				slg::ocl::KernelSource_color_types +
				slg::ocl::KernelSource_color_funcs +
				slg::ocl::KernelSource_tonemap_autolinear_funcs +
				slg::ocl::KernelSource_tonemap_reduce_funcs,
				"AutoLinearToneMap");

		SLG_LOG("[AutoLinearToneMap] Compiling OpRGBValuesReduce Kernel");
		opRGBValuesReduceKernel = new cl::Kernel(*program, "OpRGBValuesReduce");
		SLG_LOG("[AutoLinearToneMap] Compiling OpRGBValueAccumulate Kernel");
		opRGBValueAccumulateKernel = new cl::Kernel(*program, "OpRGBValueAccumulate");
		SLG_LOG("[AutoLinearToneMap] Compiling AutoLinearToneMap_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "AutoLinearToneMap_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		opRGBValuesReduceKernel->setArg(argIndex++, film.GetWidth());
		opRGBValuesReduceKernel->setArg(argIndex++, film.GetHeight());
		opRGBValuesReduceKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		opRGBValuesReduceKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		opRGBValuesReduceKernel->setArg(argIndex++, *oclAccumBuffer);

		argIndex = 0;
		opRGBValueAccumulateKernel->setArg(argIndex++, workSize / 64);
		opRGBValueAccumulateKernel->setArg(argIndex++, *oclAccumBuffer);

		argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		applyKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		const float gamma = GetGammaCorrectionValue(film, index);
		applyKernel->setArg(argIndex++, gamma);
		applyKernel->setArg(argIndex++, *oclAccumBuffer);

		const double tEnd = WallClockTime();
		SLG_LOG("[AutoLinearToneMap] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*opRGBValuesReduceKernel,
			cl::NullRange, cl::NDRange(workSize), cl::NDRange(64));
	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*opRGBValueAccumulateKernel,
			cl::NullRange, cl::NDRange(64), cl::NDRange(64));

	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(pixelCount, 256u)), cl::NDRange(256));
}
#endif
