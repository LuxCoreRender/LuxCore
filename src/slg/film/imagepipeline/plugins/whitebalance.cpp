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

WhiteBalance::WhiteBalance(): whitePoint(TemperatureToWhitePoint(6500.f)) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = nullptr;
#endif
}

WhiteBalance::WhiteBalance(const float temperature):
		whitePoint(TemperatureToWhitePoint(temperature)) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = nullptr;
#endif
}

WhiteBalance::WhiteBalance(const Spectrum &wht_pt): whitePoint(wht_pt) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = nullptr;
#endif
}

WhiteBalance::~WhiteBalance() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;
#endif
}

ImagePipelinePlugin *WhiteBalance::Copy() const {
    return new WhiteBalance(whitePoint);
}

Spectrum WhiteBalance::TemperatureToWhitePoint(const float temperature) {
    // Same code as in the RadianceChannelScale class
    BlackbodySPD spd(temperature);
    XYZColor colorTemp = spd.ToXYZ();
    colorTemp /= colorTemp.Y();

    ColorSystem colorSpace;
    Spectrum scale = colorSpace.ToRGBConstrained(colorTemp).Clamp(0.f, 1.f);
    return scale;
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
		pixels[i] *= whitePoint;
	}
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

void WhiteBalance::ApplyOCL(Film &film, const u_int index) {
	HardwareDevice *hardwareDevice = film.oclIntersectionDevice;

	if (!applyKernel) {
		// Compile sources
		const double tStart = WallClockTime();

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				slg::ocl::KernelSource_plugin_whitebalance_funcs,
				"WhiteBalance");

		SLG_LOG("[WhiteBalance] Compiling WhiteBalance_Apply Kernel");
		hardwareDevice->GetKernel(program, &applyKernel, "WhiteBalance_Apply");

		delete program;

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetHeight());
		film.oclIntersectionDevice->SetKernelArg(applyKernel, argIndex++, film.ocl_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, whitePoint.c[0]);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, whitePoint.c[1]);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, whitePoint.c[2]);

		const double tEnd = WallClockTime();
		SLG_LOG("[WhiteBalance] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(applyKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
			HardwareDeviceRange(256));
}

#endif
