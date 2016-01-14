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
#include "slg/film/imagepipeline/plugins/backgroundimg.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Background image plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BackgroundImgPlugin)

BackgroundImgPlugin::BackgroundImgPlugin(const std::string &fileName, const float gamma,
		const ImageMapStorage::StorageType storageType) {
	imgMap = new ImageMap(fileName, gamma, storageType);
	filmImageMap = NULL;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclFilmImageMapDesc = NULL;
	oclFilmImageMap = NULL;

	applyKernel = NULL;
#endif
}

BackgroundImgPlugin::BackgroundImgPlugin(ImageMap *map) {
	imgMap = map;
	filmImageMap = NULL;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclFilmImageMapDesc = NULL;
	oclFilmImageMap = NULL;

	applyKernel = NULL;
#endif
}

BackgroundImgPlugin::BackgroundImgPlugin() {
	filmImageMap = NULL;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice = NULL;
	oclFilmImageMapDesc = NULL;
	oclFilmImageMap = NULL;

	applyKernel = NULL;
#endif
}

BackgroundImgPlugin::~BackgroundImgPlugin() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;

	if (oclIntersectionDevice){
		oclIntersectionDevice->FreeBuffer(&oclFilmImageMapDesc);
		oclIntersectionDevice->FreeBuffer(&oclFilmImageMap);
	}
#endif

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
		filmImageMap = NULL;

		filmImageMap = imgMap->Copy();
		filmImageMap->Resize(width, height);
	}
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void BackgroundImgPlugin::Apply(Film &film) {
	if (!film.HasChannel(Film::ALPHA)) {
		// I can not work without alpha channel
		return;
	}

	// Check if I have to resample the image map
	UpdateFilmImageMap(film);

	Spectrum *pixels = (Spectrum *)film.channel_RGB_TONEMAPPED->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	for (u_int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			const u_int filmPixelIndex = x + y * width;
			if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(filmPixelIndex))) {
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
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void BackgroundImgPlugin::ApplyOCL(Film &film) {
	if (!film.HasChannel(Film::ALPHA)) {
		// I can not work without alpha channel
		return;
	}

	// Check if I have to resample the image map
	UpdateFilmImageMap(film);

	if (!applyKernel) {
		oclIntersectionDevice = film.oclIntersectionDevice;

		slg::ocl::ImageMap imgMapDesc;
		imgMapDesc.channelCount = filmImageMap->GetChannelCount();
		imgMapDesc.width = filmImageMap->GetWidth();
		imgMapDesc.height = filmImageMap->GetHeight();
		imgMapDesc.pageIndex = 0;
		imgMapDesc.pixelsIndex = 0;
		imgMapDesc.storageType = (slg::ocl::ImageMapStorageType)filmImageMap->GetStorage()->GetStorageType();

		// Allocate OpenCL buffers
		film.ctx->SetVerbose(true);
		oclIntersectionDevice->AllocBufferRO(&oclFilmImageMapDesc, &imgMapDesc, sizeof(slg::ocl::ImageMap), "BackgroundImg image map description");
		oclIntersectionDevice->AllocBufferRO(&oclFilmImageMap, filmImageMap->GetStorage()->GetPixelsData(),
				filmImageMap->GetStorage()->GetMemorySize(), "BackgroundImg image map");
		film.ctx->SetVerbose(false);

		// Compile sources
		const double tStart = WallClockTime();

		// Set #define symbols
		stringstream ssParams;
		ssParams.precision(6);
		ssParams << scientific <<
				" -D LUXRAYS_OPENCL_KERNEL" <<
				" -D SLG_OPENCL_KERNEL";

		ssParams << " -D PARAM_HAS_IMAGEMAPS";
		ssParams << " -D PARAM_IMAGEMAPS_PAGE_0";
		ssParams << " -D PARAM_IMAGEMAPS_COUNT=1";

		switch (imgMapDesc.storageType) {
			case slg::ocl::BYTE:
				ssParams << " -D PARAM_HAS_IMAGEMAPS_BYTE_FORMAT";
				break;
			case slg::ocl::HALF:
				ssParams << " -D PARAM_HAS_IMAGEMAPS_HALF_FORMAT";
				break;
			case slg::ocl::FLOAT:
				ssParams << " -D PARAM_HAS_IMAGEMAPS_FLOAT_FORMAT";
				break;
			default:
				throw runtime_error("Unknown storage type in BackgroundImgPlugin::ApplyOCL(): " + ToString(imgMapDesc.storageType));
		}

		switch (imgMapDesc.channelCount) {
			case 1:
				ssParams << " -D PARAM_HAS_IMAGEMAPS_1xCHANNELS";
				break;
			case 2:
				ssParams << " -D PARAM_HAS_IMAGEMAPS_2xCHANNELS";
				break;
			case 3:
				ssParams << " -D PARAM_HAS_IMAGEMAPS_3xCHANNELS";
				break;
			case 4:
				ssParams << " -D PARAM_HAS_IMAGEMAPS_4xCHANNELS";
				break;
			default:
				throw runtime_error("Unknown channel count in BackgroundImgPlugin::ApplyOCL(): " + ToString(imgMapDesc.channelCount));			
		}

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				ssParams.str(),
				slg::ocl::KernelSource_utils_funcs +
				slg::ocl::KernelSource_color_types +
				slg::ocl::KernelSource_color_funcs +
				slg::ocl::KernelSource_imagemap_types +
				slg::ocl::KernelSource_imagemap_funcs +
				slg::ocl::KernelSource_plugin_backgroundimg_funcs,
				"BackgroundImgPlugin");

		//----------------------------------------------------------------------
		// BackgroundImgPlugin_Apply kernel
		//----------------------------------------------------------------------

		SLG_LOG("[BackgroundImgPlugin] Compiling BackgroundImgPlugin_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "BackgroundImgPlugin_Apply");

		// Set kernel arguments
		u_int argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_RGB_TONEMAPPED));
		applyKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		applyKernel->setArg(argIndex++, *(film.ocl_ALPHA));
		applyKernel->setArg(argIndex++, *oclFilmImageMapDesc);
		applyKernel->setArg(argIndex++, *oclFilmImageMap);

		//----------------------------------------------------------------------

		delete program;

		// Because imgMapDesc is a local variable
		oclIntersectionDevice->GetOpenCLQueue().flush();

		const double tEnd = WallClockTime();
		SLG_LOG("[BackgroundImgPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}
	
	oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)), cl::NDRange(256));
}
#endif
