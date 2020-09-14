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
	hardwareDevice = nullptr;
	hwAccumBuffer = nullptr;

	opRGBValuesReduceKernel = nullptr;
	opRGBValueAccumulateKernel = nullptr;
	applyKernel = nullptr;
}

AutoLinearToneMap::~AutoLinearToneMap() {
	delete opRGBValuesReduceKernel;
	delete opRGBValueAccumulateKernel;
	delete applyKernel;

	if (hardwareDevice)
		hardwareDevice->FreeBuffer(&hwAccumBuffer);
}

float AutoLinearToneMap::CalcLinearToneMapScale(const Film &film, const u_int index, const float Y) {
	const float gamma = GetGammaCorrectionValue(film, index);

	// Substitute exposure, fstop and sensitivity cancel out; collect constants
	const float scale = (Y > 0.f) ? (1.25f / Y * powf(118.f / 255.f, gamma)) : 1.f;

	return scale;
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void AutoLinearToneMap::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	float Y = 0.f;
	for (u_int i = 0; i < pixelCount; ++i) {
		if (film.HasSamples(hasPN, hasSN, i)) {
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
		if (film.HasSamples(hasPN, hasSN, i)) {
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			pixels[i] = scale * pixels[i];
		}
	}
}

//------------------------------------------------------------------------------
// HardwareDevice version
//------------------------------------------------------------------------------

void AutoLinearToneMap::AddHWChannelsUsed(unordered_set<Film::FilmChannelType, hash<int> > &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
}

void AutoLinearToneMap::ApplyHW(Film &film, const u_int index) {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();
	const u_int workSize = RoundUp((pixelCount + 1) / 2, 64u);

	if (!applyKernel) {
		film.ctx->SetVerbose(true);

		hardwareDevice = film.hardwareDevice;

		// Allocate buffers
		hardwareDevice->AllocBufferRW(&hwAccumBuffer, nullptr, (workSize / 64) * sizeof(float) * 3, "Accumulation");

		// Compile sources
		const double tStart = WallClockTime();

		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D SLG_OPENCL_KERNEL");

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				opts,
				luxrays::ocl::KernelSource_luxrays_types +
				luxrays::ocl::KernelSource_color_types +
				luxrays::ocl::KernelSource_color_funcs +
				slg::ocl::KernelSource_tonemap_autolinear_funcs +
				slg::ocl::KernelSource_tonemap_reduce_funcs,
				"AutoLinearToneMap");

		SLG_LOG("[AutoLinearToneMap] Compiling OpRGBValuesReduce Kernel");
		hardwareDevice->GetKernel(program, &opRGBValuesReduceKernel, "OpRGBValuesReduce");
		SLG_LOG("[AutoLinearToneMap] Compiling OpRGBValueAccumulate Kernel");
		hardwareDevice->GetKernel(program, &opRGBValueAccumulateKernel, "OpRGBValueAccumulate");
		SLG_LOG("[AutoLinearToneMap] Compiling AutoLinearToneMap_Apply Kernel");
		hardwareDevice->GetKernel(program, &applyKernel, "AutoLinearToneMap_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(opRGBValuesReduceKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(opRGBValuesReduceKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(opRGBValuesReduceKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(opRGBValuesReduceKernel, argIndex++, hwAccumBuffer);

		argIndex = 0;
		hardwareDevice->SetKernelArg(opRGBValueAccumulateKernel, argIndex++, workSize / 64);
		hardwareDevice->SetKernelArg(opRGBValueAccumulateKernel, argIndex++, hwAccumBuffer);

		argIndex = 0;
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_IMAGEPIPELINE);
		const float gamma = GetGammaCorrectionValue(film, index);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, gamma);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, hwAccumBuffer);

		const double tEnd = WallClockTime();
		SLG_LOG("[AutoLinearToneMap] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(opRGBValuesReduceKernel, HardwareDeviceRange(workSize),
			HardwareDeviceRange(64));
	hardwareDevice->EnqueueKernel(opRGBValueAccumulateKernel, HardwareDeviceRange(64),
			HardwareDeviceRange(64));
	hardwareDevice->EnqueueKernel(applyKernel, HardwareDeviceRange(RoundUp(pixelCount, 256u)),
			HardwareDeviceRange(256));
}
