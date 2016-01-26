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

#include "luxrays/kernels/kernels.h"
#include "slg/kernels/kernels.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/coloraberration.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Color aberration filter plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ColorAberrationPlugin)

ColorAberrationPlugin::ColorAberrationPlugin(const float a) : amount(a),
		tmpBuffer(NULL), tmpBufferSize(0) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclTmpBuffer = NULL;

	applyKernel = NULL;
	copyKernel = NULL;
#endif
}

ColorAberrationPlugin::~ColorAberrationPlugin() {
	delete[] tmpBuffer;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;
	delete copyKernel;

	if (oclIntersectionDevice)
		oclIntersectionDevice->FreeBuffer(&oclTmpBuffer);
#endif
}

ImagePipelinePlugin *ColorAberrationPlugin::Copy() const {
	return new ColorAberrationPlugin(amount);
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

Spectrum ColorAberrationPlugin::BilinearSampleImage(const Spectrum *pixels,
		const u_int width, const u_int height,
		const float x, const float y) {
	const u_int x1 = Clamp(Floor2UInt(x), 0u, width - 1);
	const u_int y1 = Clamp(Floor2UInt(y), 0u, height - 1);
	const u_int x2 = Clamp(x1 + 1, 0u, width - 1);
	const u_int y2 = Clamp(y1 + 1, 0u, height - 1);
	const float tx = Clamp(x - x1, 0.f, 1.f);
	const float ty = Clamp(y - y1, 0.f, 1.f);

	Spectrum c;
	c.AddWeighted((1.f - tx) * (1.f - ty), pixels[y1 * width + x1]);
	c.AddWeighted(tx * (1.f - ty), pixels[y1 * width + x2]);
	c.AddWeighted((1.f - tx) * ty, pixels[y2 * width + x1]);
	c.AddWeighted(tx * ty, pixels[y2 * width + x2]);

	return c;
}

void ColorAberrationPlugin::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	const float invWidth = 1.f / width;
	const float invHeight = 1.f / height;

	// Allocate the temporary buffer if required
	if ((!tmpBuffer) || (width * height != tmpBufferSize)) {
		delete tmpBuffer;

		tmpBufferSize = width * height;
		tmpBuffer = new Spectrum[tmpBufferSize];
	}

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
				const float xOffset = nx - .5f;
				const float yOffset = ny - .5f;
				const float tOffset = sqrtf(xOffset * xOffset + yOffset * yOffset);

				const float rbX = (.5f + xOffset * (1.f + tOffset * amount)) * width;
				const float rbY = (.5f + yOffset * (1.f + tOffset * amount)) * height;
				const float gX =  (.5f + xOffset * (1.f - tOffset * amount)) * width;
				const float gY =  (.5f + yOffset * (1.f - tOffset * amount)) * height;

				static const Spectrum redblue(1.f, 0.f, 1.f);
				static const Spectrum green(0.f, 1.f, 0.f);

				tmpBuffer[index] = pixels[index];
				tmpBuffer[index] += redblue * BilinearSampleImage(pixels, width, height, rbX, rbY);
				tmpBuffer[index] += green * BilinearSampleImage(pixels, width, height, gX, gY);
				// I added redblue+green luminance so I divide by 2.0 to go back
				// to original luminance
				tmpBuffer[index] *= .5;
			}
		}
	}

	copy(tmpBuffer, tmpBuffer + width * height, pixels);
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void ColorAberrationPlugin::ApplyOCL(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	if (!applyKernel) {
		oclIntersectionDevice = film.oclIntersectionDevice;

		// Allocate OpenCL buffers
		film.ctx->SetVerbose(true);
		oclIntersectionDevice->AllocBufferRW(&oclTmpBuffer, width * height * sizeof(Spectrum), "ColorAberration");
		film.ctx->SetVerbose(false);

		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				slg::ocl::KernelSource_luxrays_types +
				slg::ocl::KernelSource_utils_funcs +
				slg::ocl::KernelSource_plugin_coloraberration_funcs,
				"ColorAberrationPlugin");

		//----------------------------------------------------------------------
		// ColorAberrationPlugin_Apply kernel
		//----------------------------------------------------------------------

		SLG_LOG("[ColorAberrationPlugin] Compiling ColorAberrationPlugin_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "ColorAberrationPlugin_Apply");

		// Set kernel arguments
		u_int argIndex = 0;
		applyKernel->setArg(argIndex++, width);
		applyKernel->setArg(argIndex++, height);
		applyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		applyKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		applyKernel->setArg(argIndex++, *oclTmpBuffer);
		applyKernel->setArg(argIndex++, amount);

		//----------------------------------------------------------------------
		// ColorAberrationPlugin_Copy kernel
		//----------------------------------------------------------------------

		SLG_LOG("[ColorAberrationPlugin] Compiling ColorAberrationPlugin_Copy Kernel");
		copyKernel = new cl::Kernel(*program, "ColorAberrationPlugin_Copy");

		// Set kernel arguments
		argIndex = 0;
		copyKernel->setArg(argIndex++, width);
		copyKernel->setArg(argIndex++, height);
		copyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		copyKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		copyKernel->setArg(argIndex++, *oclTmpBuffer);

		//----------------------------------------------------------------------

		delete program;

		const double tEnd = WallClockTime();
		SLG_LOG("[ColorAberrationPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}

	oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(width * height, 256u)), cl::NDRange(256));
	oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*copyKernel,
			cl::NullRange, cl::NDRange(RoundUp(width * height, 256u)), cl::NDRange(256));
}
#endif
