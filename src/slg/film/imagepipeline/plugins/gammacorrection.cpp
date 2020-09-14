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

#include "slg/film/film.h"
#include "slg/kernels/kernels.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Gamma correction plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::GammaCorrectionPlugin)

GammaCorrectionPlugin::GammaCorrectionPlugin(const float g, const u_int tableSize) {
	gamma = g;

	gammaTable.resize(tableSize, 0.f);
	float x = 0.f;
	const float dx = 1.f / tableSize;
	for (u_int i = 0; i < tableSize; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / g);
	
	hardwareDevice = nullptr;
	hwGammaTable = nullptr;
	applyKernel = nullptr;
}

GammaCorrectionPlugin::~GammaCorrectionPlugin() {
	delete applyKernel;

	if (hardwareDevice)
		hardwareDevice->FreeBuffer(&hwGammaTable);
}

ImagePipelinePlugin *GammaCorrectionPlugin::Copy() const {
	return new GammaCorrectionPlugin(gamma, gammaTable.size());
}

float GammaCorrectionPlugin::Radiance2PixelFloat(const float x) const {
	// Very slow !
	//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

	const u_int tableSize = gammaTable.size();
	const int index = Clamp<int>(Floor2UInt(tableSize * Clamp(x, 0.f, 1.f)), 0, tableSize - 1);
	return gammaTable[index];
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void GammaCorrectionPlugin::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

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
			pixels[i].c[0] = Radiance2PixelFloat(pixels[i].c[0]);
			pixels[i].c[1] = Radiance2PixelFloat(pixels[i].c[1]);
			pixels[i].c[2] = Radiance2PixelFloat(pixels[i].c[2]);
		}
	}
}

//------------------------------------------------------------------------------
// HardwareDevice version
//------------------------------------------------------------------------------

void GammaCorrectionPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType, hash<int> > &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
}

void GammaCorrectionPlugin::ApplyHW(Film &film, const u_int index) {
	if (!applyKernel) {
		film.ctx->SetVerbose(true);

		hardwareDevice = film.hardwareDevice;

		// Allocate buffers
		hardwareDevice->AllocBufferRO(&hwGammaTable, &gammaTable[0], gammaTable.size() * sizeof(float), "Gamma table");

		// Compile sources
		const double tStart = WallClockTime();

		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D SLG_OPENCL_KERNEL");

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				opts,
				luxrays::ocl::KernelSource_utils_funcs +
				slg::ocl::KernelSource_plugin_gammacorrection_funcs,
				"GammaCorrectionPlugin");

		SLG_LOG("[GammaCorrectionPlugin] Compiling GammaCorrectionPlugin_Apply Kernel");
		hardwareDevice->GetKernel(program, &applyKernel, "GammaCorrectionPlugin_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, hwGammaTable);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, (u_int)gammaTable.size());

		const double tEnd = WallClockTime();
		SLG_LOG("[GammaCorrectionPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(applyKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
			HardwareDeviceRange(256));
}
