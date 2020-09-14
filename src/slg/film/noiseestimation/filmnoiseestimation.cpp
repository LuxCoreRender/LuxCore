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

#include <limits>

#include "slg/film/film.h"
#include "slg/film/noiseestimation/filmnoiseestimation.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmNoiseEstimation
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::FilmNoiseEstimation)

FilmNoiseEstimation::FilmNoiseEstimation(const Film *flm, const u_int warmupVal, 
		const u_int testStepVal, const u_int filtScale, const u_int index) :
		warmup(warmupVal),	testStep(testStepVal),
		filterScale(filtScale), imagePipelineIndex(index), film(flm), referenceImage(NULL) {
	Reset();
}

FilmNoiseEstimation::FilmNoiseEstimation() {
	referenceImage = NULL;
}

FilmNoiseEstimation::~FilmNoiseEstimation() {
	delete referenceImage;
}

void FilmNoiseEstimation::Reset() {
	todoPixelsCount = film->GetWidth() * film->GetHeight();
	maxDiff = numeric_limits<float>::infinity();

	delete referenceImage;
	referenceImage = new GenericFrameBuffer<3, 0, float>(film->GetWidth(), film->GetHeight());

	// Allocate new framebuffers according to film size
	u_int pixelsCount = film->GetWidth() * film->GetHeight();
	const bool hasNoiseChannel = film->HasChannel(Film::NOISE);

	if (hasNoiseChannel) {
		// Resize vectors according to film size. Start at zero
		errorVector.resize(pixelsCount, 0);

		// The film could have not been yet initialized
		if (film->channel_NOISE)
			film->channel_NOISE->Clear(numeric_limits<float>::infinity());
	}

	lastSamplesCount = 0.0;
	firstTest = true;
}

bool FilmNoiseEstimation::IsTestUpdateRequired() const {
	if (!film->HasChannel(Film::NOISE))
		return false;

	const u_int pixelsCount = film->GetWidth() * film->GetHeight();

	// Run the test only after a initial warmup
	if (film->GetTotalSampleCount() / pixelsCount <= warmup)
		return false;

	// Do not run the test if we don't have at least testStep new samples per pixel
	if (film->GetTotalSampleCount() - lastSamplesCount <= pixelsCount * static_cast<double>(testStep))
		return false;

	return true;
}

void FilmNoiseEstimation::Test() {
	if (!IsTestUpdateRequired())
		return;
	
	lastSamplesCount = film->GetTotalSampleCount();

	const u_int index = (imagePipelineIndex <= (film->GetImagePipelineCount() - 1)) ? imagePipelineIndex : 0;

	if (firstTest) {
		SLG_LOG("Noise estimation: first pass");

		// Copy the current image
		referenceImage->Copy(film->channel_IMAGEPIPELINEs[index]);
		firstTest = false;
	} else {
		// Check if all pixels has been sampled

		const u_int pixelsCount = film->GetWidth() * film->GetHeight();
		const bool hasPN = film->HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
		const bool hasSN = film->HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

		bool hasEnoughSamples = true;
		for (u_int i = 0; i < pixelsCount; ++i) {
			if (!film->HasThresholdSamples(hasPN, hasSN, i, testStep)) {
				hasEnoughSamples = false;
				break;
			}
		}

		if (hasEnoughSamples) {
			const float *ref = referenceImage->GetPixels();
			const float *img = film->channel_IMAGEPIPELINEs[index]->GetPixels();

			vector<float> pixelErrorVector(pixelsCount, 0);

			// Calculate difference per pixel between images 
			for (u_int i = 0; i < pixelsCount; ++i) {
				const float refR = *ref++;
				const float refG = *ref++;
				const float refB = *ref++;

				const float imgR = *img++;
				const float imgG = *img++;
				const float imgB = *img++;

				const float dr = fabsf(imgR - refR);
				const float dg = fabsf(imgG - refG);
				const float db = fabsf(imgB - refB);

				const float imgSum = imgR + imgG + imgB;
				const float error = (imgSum != 0.f) ?
					((dr + dg + db) / sqrt(imgR + imgG + imgB)) : 0.f;
				pixelErrorVector[i] = error;
			}

			if (filterScale > 0) {
				// Filter noise channel using a (filterScale+1)*(filterScale+1) window average. 
				// The window becomes smaller at the borders

				const int height = film->GetHeight();
				const int width = film->GetWidth();
				for (int i = 0; i < height; i++) {
					for (int j = 0; j < width; j++) {
						float diffAccumulator = 0;
						const int minHeight = 0 > i - static_cast<int>(filterScale) ? 0 : i - filterScale;
						const int maxHeight = height < i + static_cast<int>(filterScale) ? height : i + filterScale;
						const int minWidth = 0 > j - static_cast<int>(filterScale) ? 0 : j - filterScale;
						const int maxWidth = width < j + static_cast<int>(filterScale) ? width : j + filterScale;
						for (int r = minHeight; r < maxHeight; r++) {
							for (int c = minWidth; c < maxWidth; c++) {
								diffAccumulator += pixelErrorVector[r * width + c];
							}
						}
						const u_int windowSize =  (maxHeight - minHeight) * (maxWidth - minWidth);
						errorVector[i * width + j] = diffAccumulator / windowSize;
					}
				}

				float errorMean = 0;
				float errorStd = 0;
				float accumulator = 0;

				// Calculate the error mean after the filtering
				for (u_int j = 0; j < pixelsCount; ++j) {
					const float pixelVal = errorVector[j];
					if (isnan(pixelVal) || isinf(pixelVal))
						continue;

					accumulator += pixelVal;
				}
				errorMean = accumulator / pixelsCount;

				// Calculate the error standard deviation after the filtering
				accumulator = 0;
				for (u_int j = 0; j < pixelsCount; ++j) {
					const float pixelVal = errorVector[j];
					if (isnan(pixelVal) || isinf(pixelVal))
						continue;

					const float delta = pixelVal - errorMean;
					accumulator += delta * delta;
				}
				errorStd = sqrt((1.f / pixelsCount) * accumulator);

				// Remove outliers
				float errorMax = -numeric_limits<float>::infinity();
				float errorMin = numeric_limits<float>::infinity();
				for (u_int j = 0; j < pixelsCount; j++) {
					// Calculate standard score. Clamp value at 6 standard deviations from mean
					const float score = Clamp((errorVector[j] - errorMean) / errorStd, -6.f, 6.f);
					errorVector[j] = score;

					// Find maximum and minimum standard scores
					errorMax = Max(score, errorMax);
					errorMin = Min(score, errorMin);
				}

				// Normalize error values
				for (u_int i = 0; i < pixelsCount; ++i) {
					// Update NOISE channel
						const float noise = (errorVector[i] - errorMin) / (errorMax - errorMin);
						*(film->channel_NOISE->GetPixel(i)) = noise;
				}

				SLG_LOG("Noise estimation: Error mean = " << errorMean);
			}
		}

		// Copy the current image
		referenceImage->Copy(film->channel_IMAGEPIPELINEs[index]);
	}
}


template<class Archive> void FilmNoiseEstimation::serialize(Archive &ar, const u_int version) {
	ar & warmup;
	ar & testStep;
	ar & film;
	ar & referenceImage;
	ar & lastSamplesCount;
	ar & firstTest;
}

namespace slg {
// Explicit instantiations for portable archives
template void FilmNoiseEstimation::serialize(LuxOutputArchive &ar, const u_int version);
template void FilmNoiseEstimation::serialize(LuxInputArchive &ar, const u_int version);
}