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
}

BackgroundImgPlugin::BackgroundImgPlugin(ImageMap *map) {
	imgMap = map;
	filmImageMap = NULL;
}

BackgroundImgPlugin::~BackgroundImgPlugin() {
	delete imgMap;
	delete filmImageMap;
}

ImagePipelinePlugin *BackgroundImgPlugin::Copy() const {
	return new BackgroundImgPlugin(imgMap->Copy());
}

void BackgroundImgPlugin::Apply(const Film &film, Spectrum *pixels) const {
	if (!film.HasChannel(Film::ALPHA)) {
		// I can not work without alpha channel
		return;
	}

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
