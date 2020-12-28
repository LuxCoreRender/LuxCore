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
#include "slg/film/imagepipeline/plugins/backgroundimg.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Background image plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BackgroundImgPlugin)

BackgroundImgPlugin::BackgroundImgPlugin(ImageMap *map) {
	imgMap = map;
	filmImageMap = nullptr;

	hardwareDevice = nullptr;
	hwFilmImageMapDesc = nullptr;
	hwFilmImageMap = nullptr;

	applyKernel = nullptr;
}

BackgroundImgPlugin::BackgroundImgPlugin() {
	filmImageMap = nullptr;

	hardwareDevice = nullptr;
	hwFilmImageMapDesc = nullptr;
	hwFilmImageMap = nullptr;

	applyKernel = nullptr;
}

BackgroundImgPlugin::~BackgroundImgPlugin() {
	delete applyKernel;

	if (hardwareDevice) {
		hardwareDevice->FreeBuffer(&hwFilmImageMapDesc);
		hardwareDevice->FreeBuffer(&hwFilmImageMap);
	}

	delete imgMap;
	delete filmImageMap;
}

ImagePipelinePlugin *BackgroundImgPlugin::Copy() const {
	return new BackgroundImgPlugin(imgMap->Copy());
}

void BackgroundImgPlugin::UpdateFilmImageMap(const Film &film) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Check if I have to resample the image map
	if ((!filmImageMap) ||
			(filmImageMap->GetWidth() != width) || (filmImageMap->GetHeight() != height)) {
		delete filmImageMap;
		filmImageMap = nullptr;

		filmImageMap = imgMap->Copy();
		filmImageMap->Resize(width, height);
	}
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void BackgroundImgPlugin::Apply(Film &film, const u_int index) {
	if (!film.HasChannel(Film::ALPHA)) {
		// I can not work without alpha channel
		return;
	}

	// Check if I have to resample the image map
	UpdateFilmImageMap(film);

	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

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
			const u_int filmPixelIndex = x + y * width;
			if (film.HasSamples(hasPN, hasSN, filmPixelIndex)) {
				float alpha;
				film.channel_ALPHA->GetWeightedPixel(x, y, &alpha);

				// Need to flip the along the Y axis for the image
				const u_int imgPixelIndex = x + (height - y - 1) * width;
				pixels[filmPixelIndex] = Lerp(alpha,
						filmImageMap->GetStorage()->GetSpectrum(imgPixelIndex),
						pixels[filmPixelIndex]);
			}
		}
	}
}

//------------------------------------------------------------------------------
// HardwareDevice version
//------------------------------------------------------------------------------

void BackgroundImgPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType, hash<int> > &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
	hwChannelsUsed.insert(Film::ALPHA);
}

void BackgroundImgPlugin::ApplyHW(Film &film, const u_int index) {
	if (!film.HasChannel(Film::ALPHA)) {
		// I can not work without alpha channel
		return;
	}

	// Check if I have to resample the image map
	UpdateFilmImageMap(film);

	if (!applyKernel) {
		film.ctx->SetVerbose(true);

		hardwareDevice = film.hardwareDevice;

		slg::ocl::ImageMap imgMapDesc;
		imgMapDesc.desc.storageType = (luxrays::ocl::ImageMapStorageType)filmImageMap->GetStorage()->GetStorageType();
		imgMapDesc.desc.wrapType = luxrays::ocl::WRAP_CLAMP;
		imgMapDesc.desc.channelCount = filmImageMap->GetChannelCount();
		imgMapDesc.desc.width = filmImageMap->GetWidth();
		imgMapDesc.desc.height = filmImageMap->GetHeight();
		imgMapDesc.genericAddr.pageIndex = 0;
		imgMapDesc.genericAddr.pixelsIndex = 0;

		// Allocate OpenCL buffers
		hardwareDevice->AllocBufferRO(&hwFilmImageMapDesc, &imgMapDesc, sizeof(slg::ocl::ImageMap),
						"BackgroundImg image map description");
		hardwareDevice->AllocBufferRO(&hwFilmImageMap, filmImageMap->GetStorage()->GetPixelsData(),
						filmImageMap->GetStorage()->GetMemorySize(), "BackgroundImg image map");

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
				luxrays::ocl::KernelSource_color_types +
				luxrays::ocl::KernelSource_color_funcs +
				slg::ocl::KernelSource_imagemap_types +
				slg::ocl::KernelSource_imagemap_funcs +
				slg::ocl::KernelSource_plugin_backgroundimg_funcs,
				"BackgroundImgPlugin");

		//----------------------------------------------------------------------
		// BackgroundImgPlugin_Apply kernel
		//----------------------------------------------------------------------

		SLG_LOG("[BackgroundImgPlugin] Compiling BackgroundImgPlugin_Apply Kernel");
		hardwareDevice->GetKernel(program, &applyKernel, "BackgroundImgPlugin_Apply");

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_ALPHA);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, hwFilmImageMapDesc);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, hwFilmImageMap);

		//----------------------------------------------------------------------

		delete program;

		// Because imgMapDesc is a local variable
		hardwareDevice->FinishQueue();

		const double tEnd = WallClockTime();
		SLG_LOG("[BackgroundImgPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
		
		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(applyKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
			HardwareDeviceRange(256));
}
