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

#include "slg/scene/scene.h"
#include "slg/imagemap/resizepolicies/resizepolicies.h"
#include "slg/imagemap/imagemapcache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapResizeMinMemPolicy::ApplyResizePolicy()
//------------------------------------------------------------------------------

ImageMap *ImageMapResizeMinMemPolicy::ApplyResizePolicy(const std::string &fileName,
		const ImageMapConfig &imgCfg, bool &toApply) const {
	ImageMap *im = new ImageMap(fileName, imgCfg);

	const u_int width = im->GetWidth();
	const u_int height = im->GetHeight();

	if (Max(width, height) > minSize) {
		u_int newWidth, newHeight;
		if (width >= height) {
			newWidth = minSize;
			newHeight = Max<u_int>(minSize * (width / (float)height), 1u);
		} else {
			newWidth = Max<u_int>(minSize * (height / (float)width), 1u);
			newHeight = minSize;
		}

		SDL_LOG("Scaling probe ImageMap: " << im->GetName() << " [from " << width << "x" << height <<
				" to " << newWidth << "x" << newHeight <<"]");

		im->Resize(newWidth, newHeight);
		im->Preprocess();

		im->SetUpInstrumentation(width, height, imgCfg);

		toApply = true;
	} else
		toApply = false;
	
	return im;
}

//------------------------------------------------------------------------------
// ImageMapResizeMinMemPolicy::Preprocess()
//------------------------------------------------------------------------------

void ImageMapResizeMinMemPolicy::Preprocess(ImageMapCache &imc, const Scene *scene,
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

		// Reload the original image map
		imc.maps[i]->Reload();
		originalMemUsed += imc.maps[i]->GetStorage()->GetMemorySize();

		// Resize the image map
		imc.maps[i]->Resize(newWidth, newHeight);
		currentMemUsed += imc.maps[i]->GetStorage()->GetMemorySize();
		
		SDL_LOG("Image maps \"" << imc.maps[i]->GetName() << "\" scaled: " <<
				originalWidth << "x" << originalHeigth << " => " <<
				newWidth << "x" << newHeight);
	}

	SDL_LOG("Memory required for original Image maps: " + ToMemString(originalMemUsed));
	SDL_LOG("Memory required for MINMEM Image maps: " + ToMemString(currentMemUsed));
	
	// Delete instrumentation for image maps checked
	for (auto i : imgMapsIndices)
		imc.maps[i]->DeleteInstrumentation();

	SDL_LOG("Applying resize policy " << ImageMapResizePolicyType2String(GetType()) << " time: " <<
			(boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");
}
