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

#include "luxrays/core/color/spds/blackbodyspd.h"

#include "slg/kernels/kernels.h"
#include "slg/film/imagepipeline/plugins/whitebalance.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// White balance filter
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::WhiteBalance)

WhiteBalance::WhiteBalance(const float temperature, const bool reverse, const bool normalize) {
	scale = TemperatureToWhitePoint(temperature, normalize);
	if (reverse)
		scale = Spectrum(1.f) / scale;
			
	applyKernel = nullptr;
}

WhiteBalance::WhiteBalance() {
	scale = TemperatureToWhitePoint(6500.f, false);
	scale = Spectrum(1.f) / scale;

	applyKernel = nullptr;
}

WhiteBalance::WhiteBalance(const Spectrum &s): scale(s), applyKernel(nullptr) {
}

WhiteBalance::~WhiteBalance() {
	delete applyKernel;
}

ImagePipelinePlugin *WhiteBalance::Copy() const {
    return new WhiteBalance(scale);
}

void WhiteBalance::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *) film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	const u_int pixelCount = width * height;

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		pixels[i] *= scale;
	}
}

//------------------------------------------------------------------------------
// HardwareDevice version
//------------------------------------------------------------------------------

void WhiteBalance::AddHWChannelsUsed(unordered_set<Film::FilmChannelType, hash<int> > &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
}

void WhiteBalance::ApplyHW(Film &film, const u_int index) {
	HardwareDevice *hardwareDevice = film.hardwareDevice;

	if (!applyKernel) {
		// Compile sources
		const double tStart = WallClockTime();

		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D SLG_OPENCL_KERNEL");

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				opts,
				slg::ocl::KernelSource_plugin_whitebalance_funcs,
				"WhiteBalance");

		SLG_LOG("[WhiteBalance] Compiling WhiteBalance_Apply Kernel");
		hardwareDevice->GetKernel(program, &applyKernel, "WhiteBalance_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, scale.c[0]);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, scale.c[1]);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, scale.c[2]);

		const double tEnd = WallClockTime();
		SLG_LOG("[WhiteBalance] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(applyKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
			HardwareDeviceRange(256));
}
