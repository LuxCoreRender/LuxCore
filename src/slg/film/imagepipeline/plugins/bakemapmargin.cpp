/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/bakemapmargin.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BakeMapMarginPlugin plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BakeMapMarginPlugin)

BakeMapMarginPlugin::BakeMapMarginPlugin(const u_int pixels) : marginPixels(pixels) {
}

ImagePipelinePlugin *BakeMapMarginPlugin::Copy() const {
	return new BakeMapMarginPlugin(marginPixels);
}

void BakeMapMarginPlugin::Apply(Film &film, const u_int index) {
	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	const int width = (int)film.GetWidth();
	const int height = (int)film.GetHeight();

	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	vector<Spectrum> tmpPixels(width * height);
	vector<bool> srcPixelsMask(width * height);
	vector<bool> dstPixelsMask(width * height);

	#pragma omp parallel for
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const u_int pixelIndex = x + y * width;
			srcPixelsMask[pixelIndex] = film.HasSamples(hasPN, hasSN, pixelIndex);
		}
	}

	for (u_int step = 0; step < marginPixels; ++step) {
		#pragma omp parallel for
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const u_int pixelIndex = x + y * width;

				if (!srcPixelsMask[pixelIndex]) {
					// It is a pixel without any sample

					// Compute the average of all neighbors
					u_int count = 0;
					Spectrum c;
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							int xx = x + dx;
							int yy = y + dy;
							const u_int index = xx + yy * width;

							// Check if it is a valid pixel
							if ((xx < 0) || (xx >= width) ||
									(yy < 0) || (yy >= height) ||
									((dx == 0) && (dy == 0)) ||
									!srcPixelsMask[index])
								continue;

							c += pixels[index];
							++count;
						}
					}

					if (count == 0)
						dstPixelsMask[pixelIndex] = false;
					else {
						c /= count;
						tmpPixels[pixelIndex] = Spectrum(1.f,0.f,0.f);
						dstPixelsMask[pixelIndex] = true;
					}
				} else {
					// Just copy the pixel
					tmpPixels[pixelIndex] = pixels[pixelIndex];
					dstPixelsMask[pixelIndex] = true;
				}
			}
		}

		copy(tmpPixels.begin(), tmpPixels.end(), &pixels[0]);
		copy(dstPixelsMask.begin(), dstPixelsMask.end(), srcPixelsMask.begin());
	}
}
