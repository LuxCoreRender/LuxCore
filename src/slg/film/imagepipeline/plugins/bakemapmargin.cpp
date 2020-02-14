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

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/bakemapmargin.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BakeMapMarginPlugin plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BakeMapMarginPlugin)

BakeMapMarginPlugin::BakeMapMarginPlugin(const u_int pixels, const float threshold) :
		marginPixels(pixels), samplesThreshold(threshold) {
}

ImagePipelinePlugin *BakeMapMarginPlugin::Copy() const {
	return new BakeMapMarginPlugin(marginPixels, samplesThreshold);
}

void BakeMapMarginPlugin::Apply(Film &film, const u_int index) {
	Apply(film, index, marginPixels, samplesThreshold, false);
}

void BakeMapMarginPlugin::Apply(Film &film, const u_int index,
		const u_int marginPixels, const float samplesThreshold,
		const bool applyAlpha) {
	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	const bool hasAlpha = film.HasChannel(Film::ALPHA);

	const int width = (int)film.GetWidth();
	const int height = (int)film.GetHeight();
	const u_int pixelCount = width * height;

	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	vector<Spectrum> tmpPixels(pixelCount);
	vector<float> tmpAlphas(pixelCount);
	vector<bool> srcPixelsMask(pixelCount);
	vector<bool> dstPixelsMask(pixelCount);

	#pragma omp parallel for
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const u_int pixelIndex = x + y * width;
			srcPixelsMask[pixelIndex] = film.HasThresholdSamples(hasPN, hasSN, pixelIndex, samplesThreshold);
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
					float a = 0.f;
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
							if (hasAlpha) {
								float value;
								film.channel_ALPHA->GetWeightedPixel(index, &value);
								a += value;
							}
							++count;
						}
					}

					if (count == 0) {
						tmpPixels[pixelIndex] = Spectrum();
						dstPixelsMask[pixelIndex] = false;

						if (hasAlpha)
							tmpAlphas[pixelIndex] = 1.f;
					} else {
						c /= count;
						tmpPixels[pixelIndex] = c;
						dstPixelsMask[pixelIndex] = true;

						if (hasAlpha) {
							a /= count;
							tmpAlphas[pixelIndex] = a;
						}
					}
				} else {
					// Just copy the pixel
					tmpPixels[pixelIndex] = pixels[pixelIndex];
					dstPixelsMask[pixelIndex] = true;
					
					if (hasAlpha) {
						float value;
						film.channel_ALPHA->GetWeightedPixel(pixelIndex, &value);

						tmpAlphas[pixelIndex] = value;
					}
				}
			}
		}

		copy(tmpPixels.begin(), tmpPixels.end(), &pixels[0]);
		if (hasAlpha && applyAlpha) {
			for (int y = 0; y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					float *a = film.channel_ALPHA->GetPixel(x, y);
					a[0] = tmpAlphas[x + y * width];
					a[1] = 1.f;
				}
			}
		}
		copy(dstPixelsMask.begin(), dstPixelsMask.end(), srcPixelsMask.begin());
	}
}
