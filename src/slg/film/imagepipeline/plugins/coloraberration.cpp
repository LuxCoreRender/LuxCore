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
		tmpBuffer(nullptr), tmpBufferSize(0) {
	hardwareDevice = nullptr;
	hwTmpBuffer = nullptr;

	applyKernel = nullptr;
	copyKernel = nullptr;
}

ColorAberrationPlugin::~ColorAberrationPlugin() {
	delete[] tmpBuffer;

	delete applyKernel;
	delete copyKernel;

	if (hardwareDevice)
		hardwareDevice->FreeBuffer(&hwTmpBuffer);
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

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			const u_int index = x + y * width;

			if (film.HasSamples(hasPN, hasSN, index)) {
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
// HardwareDevice version
//------------------------------------------------------------------------------

void ColorAberrationPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType> &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
}

void ColorAberrationPlugin::ApplyHW(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	if (!applyKernel) {
		film.ctx->SetVerbose(true);

		hardwareDevice = film.hardwareDevice;

		// Allocate OpenCL buffers
		hardwareDevice->AllocBufferRW(&hwTmpBuffer, nullptr, width * height * sizeof(Spectrum), "ColorAberration");

		// Compile sources
		const double tStart = WallClockTime();

		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D SLG_OPENCL_KERNEL");

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				opts,
				luxrays::ocl::KernelSource_luxrays_types +
				luxrays::ocl::KernelSource_utils_funcs +
				slg::ocl::KernelSource_plugin_coloraberration_funcs,
				"ColorAberrationPlugin");

		//----------------------------------------------------------------------
		// ColorAberrationPlugin_Apply kernel
		//----------------------------------------------------------------------

		SLG_LOG("[ColorAberrationPlugin] Compiling ColorAberrationPlugin_Apply Kernel");
		hardwareDevice->GetKernel(program, &applyKernel, "ColorAberrationPlugin_Apply");

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, width);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, height);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, hwTmpBuffer);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, amount);

		//----------------------------------------------------------------------
		// ColorAberrationPlugin_Copy kernel
		//----------------------------------------------------------------------

		SLG_LOG("[ColorAberrationPlugin] Compiling ColorAberrationPlugin_Copy Kernel");
		hardwareDevice->GetKernel(program, &copyKernel, "ColorAberrationPlugin_Copy");

		// Set kernel arguments
		argIndex = 0;
		hardwareDevice->SetKernelArg(copyKernel, argIndex++, width);
		hardwareDevice->SetKernelArg(copyKernel, argIndex++, height);
		hardwareDevice->SetKernelArg(copyKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(copyKernel, argIndex++, hwTmpBuffer);

		//----------------------------------------------------------------------

		delete program;

		const double tEnd = WallClockTime();
		SLG_LOG("[ColorAberrationPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(applyKernel, HardwareDeviceRange(RoundUp(width * height, 256u)),
			HardwareDeviceRange(256));
	hardwareDevice->EnqueueKernel(copyKernel, HardwareDeviceRange(RoundUp(width * height, 256u)),
			HardwareDeviceRange(256));
}
