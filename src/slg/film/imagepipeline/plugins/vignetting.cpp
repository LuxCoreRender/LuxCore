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
#include "slg/film/imagepipeline/plugins/vignetting.h"
#include "luxrays/kernels/kernels.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Vignetting plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::VignettingPlugin)

VignettingPlugin::VignettingPlugin(const float s) : scale(s) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = NULL;
#endif
}

VignettingPlugin::~VignettingPlugin() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;
#endif
}

ImagePipelinePlugin *VignettingPlugin::Copy() const {
	return new VignettingPlugin(scale);
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void VignettingPlugin::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	const float invWidth = 1.f / width;
	const float invHeight = 1.f / height;

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			const u_int index = x + y * width;

			if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(index))) {
				const float nx = x * invWidth;
				const float ny = y * invHeight;
				const float xOffset = (nx - .5f) * 2.f;
				const float yOffset = (ny - .5f) * 2.f;
				const float tOffset = sqrtf(xOffset * xOffset + yOffset * yOffset);

				// Normalize to range [0.f - 1.f]
				const float invOffset = 1.f - (fabsf(tOffset) * 1.42f);
				float vWeight = Lerp(invOffset, 1.f - scale, 1.f);

				pixels[index].c[0] *= vWeight;
				pixels[index].c[1] *= vWeight;
				pixels[index].c[2] *= vWeight;
			}
		}
	}
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void VignettingPlugin::ApplyOCL(Film &film, const u_int index) {
	if (!applyKernel) {
		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				slg::ocl::KernelSource_plugin_vignetting_funcs,
				"VignettingPlugin");

		SLG_LOG("[VignettingPlugin] Compiling VignettingPlugin_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "VignettingPlugin_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		applyKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		applyKernel->setArg(argIndex++, scale);

		const double tEnd = WallClockTime();
		SLG_LOG("[VignettingPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)), cl::NDRange(256));
}
#endif
