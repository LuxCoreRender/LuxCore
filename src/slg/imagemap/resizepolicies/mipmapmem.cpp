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

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "slg/scene/scene.h"
#include "slg/imagemap/imagemapcache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapResizeMipMapMemPolicy::ApplyResizePolicy()
//------------------------------------------------------------------------------

ImageMap *ImageMapResizeMipMapMemPolicy::ApplyResizePolicy(const std::string &srcFileName,
		const ImageMapConfig &imgCfg, bool &toApply) const {
	// Check/Create and TX file for each image map
	const string dstFileName = srcFileName + ".tx";

	// Check if the TX file exist and it is up to date
	if (!boost::filesystem::exists(dstFileName) ||
			(boost::filesystem::last_write_time(srcFileName) > boost::filesystem::last_write_time(dstFileName))) {
		SDL_LOG("Creating TX image file:  " << srcFileName);

		ImageMap::MakeTx(srcFileName, dstFileName);
	}

	pair<u_int, u_int> size = ImageMap::GetSize(dstFileName);
	const u_int width = size.first;
	const u_int height = size.second;

	if (Max(width, height) > minSize) {
		u_int newWidth, newHeight;
		if (width >= height) {
			newWidth = minSize;
			newHeight = Max<u_int>(minSize * (width / (float)height), 1u);
		} else {
			newWidth = Max<u_int>(minSize * (height / (float)width), 1u);
			newHeight = minSize;
		}

		SDL_LOG("Probe ImageMap: " << dstFileName << " [from " << width << "x" << height <<
				" to " << newWidth << "x" << newHeight <<"]");

		ImageMap *im = new ImageMap(dstFileName, imgCfg, newWidth, newHeight);
		im->SetUpInstrumentation(width, height, imgCfg);

		toApply = true;

		return im;
	} else {
		ImageMap *im = new ImageMap(dstFileName, imgCfg);
		toApply = false;

		return im;
	}
}

//------------------------------------------------------------------------------
// ImageMapResizeMinMemPolicy::Preprocess()
//------------------------------------------------------------------------------

void ImageMapResizeMipMapMemPolicy::Preprocess(ImageMapCache &imc, const Scene *scene,
		const bool useRTMode) const {
	if (useRTMode)
		return;

	SDL_LOG("Applying resize policy " << ImageMapResizePolicyType2String(GetType()) << "...");
	
	const double startTime = WallClockTime();
	
	// Build the list of image maps to check
	vector<u_int> imgMapsIndices;
	for (u_int i = 0; i < imc.resizePolicyToApply.size(); ++i) {
		if (imc.resizePolicyToApply[i]) {
			//SDL_LOG("Image map to process:  " << imc.maps[i]->GetName());
			imgMapsIndices.push_back(i);
		}
	}
	
	SDL_LOG("Image maps to process:  " << imgMapsIndices.size());

	// Enable instrumentation for image maps to check
	for (auto i : imgMapsIndices)
		imc.maps[i]->EnableInstrumentation();

	// Do a test render to establish the optimal image maps sizes
	CalcOptimalImageMapSizes(imc, scene, imgMapsIndices);

	size_t originalMemUsed = 0;
	size_t currentMemUsed = 0;
	for (auto i : imgMapsIndices) {
		const u_int originalWidth = imc.maps[i]->instrumentationInfo->originalWidth;
		const u_int originalHeigth = imc.maps[i]->instrumentationInfo->originalHeigth;

		u_int optimalWidth = scale * imc.maps[i]->instrumentationInfo->optimalWidth;
		u_int optimalHeigth = scale * imc.maps[i]->instrumentationInfo->optimalHeigth;

		u_int newWidth, newHeight;
		if ((optimalWidth == 0) || (optimalHeigth == 0)) {
			newWidth = imc.maps[i]->GetWidth();
			newHeight = imc.maps[i]->GetHeight();
		} else {
			if (originalWidth >= originalHeigth) {
				const float scaleFactor = optimalWidth / (float)originalWidth;

				if (scaleFactor > 1.f) {
					SDL_LOG("WARNING: image maps \"" << imc.maps[i]->GetName() << "\" too small !");
					newWidth = originalWidth;
					newHeight = originalHeigth;
				} else {
					newWidth = Max<u_int>(optimalWidth, minSize);
					newHeight = Max<u_int>(originalHeigth * (newWidth / (float)originalWidth), 1u);
				}
			} else {
				const float scaleFactor = optimalHeigth / (float)originalHeigth;

				if (scaleFactor > 1.f) {
					SDL_LOG("WARNING: image maps \"" << imc.maps[i]->GetName() << "\" too small !");
					newWidth = originalWidth;
					newHeight = originalHeigth;
				} else {
					newHeight = Max<u_int>(optimalHeigth, minSize);
					newWidth = Max<u_int>(originalWidth * (newHeight / (float)originalHeigth), 1u);
				}
			}
		}

		imc.resizePolicyToApply[i] = false;

		originalMemUsed += originalWidth * originalHeigth * imc.maps[i]->GetStorage()->GetMemoryPixelSize();

		// Reload the original image map with the best mip map level
		imc.maps[i]->Reload(newWidth, newHeight);
		currentMemUsed += imc.maps[i]->GetStorage()->GetMemorySize();
		
		SDL_LOG("Image maps \"" << imc.maps[i]->GetName() << "\" scaled: " <<
				originalWidth << "x" << originalHeigth << " => " <<
				newWidth << "x" << newHeight);
	}

	SDL_LOG("Memory required for original Image maps: " + ToMemString(originalMemUsed));
	SDL_LOG("Memory required for MIPMAPMEM Image maps: " + ToMemString(currentMemUsed));
	
	// Delete instrumentation for image maps checked
	for (auto i : imgMapsIndices)
		imc.maps[i]->DeleteInstrumentation();

	SDL_LOG("Applying resize policy " << ImageMapResizePolicyType2String(GetType()) << " time: " <<
			(boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");
}
