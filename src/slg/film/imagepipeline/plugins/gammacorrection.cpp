/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "luxrays/kernels/kernels.h"


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
	
#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclGammaTable = NULL;
	applyKernel = NULL;
#endif
}

GammaCorrectionPlugin::~GammaCorrectionPlugin() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;

	if (oclIntersectionDevice)
		oclIntersectionDevice->FreeBuffer(&oclGammaTable);
#endif
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

void GammaCorrectionPlugin::Apply(Film &film) {
	Spectrum *pixels = (Spectrum *)film.channel_RGB_TONEMAPPED->GetPixels();
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(i))) {
			pixels[i].c[0] = Radiance2PixelFloat(pixels[i].c[0]);
			pixels[i].c[1] = Radiance2PixelFloat(pixels[i].c[1]);
			pixels[i].c[2] = Radiance2PixelFloat(pixels[i].c[2]);
		}
	}
}

#if !defined(LUXRAYS_DISABLE_OPENCL)
void GammaCorrectionPlugin::ApplyOCL(Film &film) {
	if (!applyKernel) {
		oclIntersectionDevice = film.oclIntersectionDevice;
		oclIntersectionDevice->AllocBufferRO(&oclGammaTable, &gammaTable[0], gammaTable.size() * sizeof(float), "Gamma table");

		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"",
				slg::ocl::KernelSource_utils_funcs +
				slg::ocl::KernelSource_plugin_gammacorrection_funcs,
				"GammaCorrectionPlugin");

		SLG_LOG("[GammaCorrectionPlugin] Compiling GammaCorrectionPlugin_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "GammaCorrectionPlugin_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_RGB_TONEMAPPED));
		applyKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		applyKernel->setArg(argIndex++, *oclGammaTable);
		applyKernel->setArg(argIndex++, (u_int)gammaTable.size());

		const double tEnd = WallClockTime();
		SLG_LOG("[GammaCorrectionPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)), cl::NDRange(256));
}
#endif
