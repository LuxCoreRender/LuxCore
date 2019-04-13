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

#include <limits>

#include "slg/film/film.h"
#include "slg/film/convtest/filmconvtest.h"
#include "slg/film/imagepipeline/plugins/gaussianblur3x3.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmConvTest
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::FilmConvTest)

FilmConvTest::FilmConvTest(const Film *flm, const float thresholdVal,
		const u_int warmupVal, const u_int testStepVal, const u_int filtScale) :
		threshold(thresholdVal), warmup(warmupVal),	testStep(testStepVal),
		filterScale(filtScale), film(flm), referenceImage(NULL) {
	Reset();
}

FilmConvTest::FilmConvTest() {
	referenceImage = NULL;
}

FilmConvTest::~FilmConvTest() {
	delete referenceImage;
}

void FilmConvTest::Reset() {
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
	}

	passCount = 0;
	lastSamplesCount = 0.0;
	firstTest = true;
}

u_int FilmConvTest::Test() {
	const u_int pixelsCount = film->GetWidth() * film->GetHeight();

	// Run the test only after a initial warmup
	if (film->GetTotalSampleCount() / pixelsCount <= warmup)
		return todoPixelsCount;

	// Do not run the test if we don't have at least convergence.step new samples per pixel
	const double newSamplesCount = film->GetTotalSampleCount();
	if (newSamplesCount  - lastSamplesCount <= pixelsCount * static_cast<double>(testStep))
		return todoPixelsCount;
	lastSamplesCount = newSamplesCount;

	if (firstTest) {
		// Copy the current image
		referenceImage->Copy(film->channel_IMAGEPIPELINEs[0]);
		firstTest = false;

		SLG_LOG("Convergence test first pass");

		return todoPixelsCount;
	} else {
		SLG_LOG("Convergence test step");

		// Check the number of pixels over the threshold
		const float *ref = referenceImage->GetPixels();
		const float *img = film->channel_IMAGEPIPELINEs[0]->GetPixels();
		
		todoPixelsCount = 0;
		maxDiff = 0.f; 
		const bool hasConvChannel = film->HasChannel(Film::CONVERGENCE);
		const bool hasNoiseChannel = film->HasChannel(Film::NOISE);
	
		vector<float> pixelErrorVector(pixelsCount, 0);

		// At least one of the two metrics needs to be calculated
		if (hasNoiseChannel || hasConvChannel) {
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


				if (hasNoiseChannel) {
					const float imgSum = imgR + imgG + imgB;
					const float error = (imgSum != 0.f) ?
						((dr + dg + db) / sqrt(imgR + imgG + imgB)) : 0.f;
					pixelErrorVector[i] = error;
				}

				// Using different metric for CONVERGENCE channel and the noise halt threshold
				const float diff = Max(Max(dr, dg), db);
				
				maxDiff = Max(maxDiff, diff);
				if (diff > threshold) ++todoPixelsCount;

				if (hasConvChannel) {
					*(film->channel_CONVERGENCE->GetPixel(i)) = diff;
				}
			}
		}

		if (hasNoiseChannel && filterScale > 0) {
			// Filter convergence using a (filterScale+1)*(filterScale+1) window average. 
			// The window becomes smaller at the borders

			SLG_LOG("Filter scale: " << filterScale);
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

					if (!(isnan(diffAccumulator) || isinf(diffAccumulator))) {
						const u_int windowSize =  (maxHeight - minHeight) * (maxWidth - minWidth);
						errorVector[i * width + j] = diffAccumulator / windowSize;
					}
				}
			}
			
			float errorMean = 0;
			float errorStd = 0;
			float accumulator = 0;

			// Calculate the difference mean after the filtering
			for (u_int j = 0; j < pixelsCount; ++j) {
				const float pixelVal = errorVector[j];
				if (isnan(pixelVal) || isinf(pixelVal)) { continue; }
				accumulator += pixelVal;
			}
			errorMean = accumulator / pixelsCount;
			
			// Calculate the difference standard deviation after the filtering
			accumulator = 0;
			for (u_int j = 0; j < pixelsCount; ++j) {
				const float pixelVal = errorVector[j];
				if (isnan(pixelVal) || isinf(pixelVal)) { continue; }
				accumulator += pow(pixelVal - errorMean, 2);
			}
			errorStd = sqrt((1.f / pixelsCount) * accumulator);


			float errorMax = -numeric_limits<float>::infinity();
			float errorMin = numeric_limits<float>::infinity();
			for (u_int j = 0; j < pixelsCount; j++) {
				// Calculate standard score. Clamp value at 3 standard deviations from mean
				const float score = Clamp((errorVector[j] - errorMean) / errorStd, -3.f, 3.f);
				errorVector[j] = score;
				// Find maximum and minimum standard scores
				errorMax = Max(score, errorMax);
				errorMin = Min(score, errorMin);
			}
			

			for (u_int i = 0; i < pixelsCount; ++i) {
				// Update NOISE channel
				const float noise = (errorVector[i] - errorMin) / (errorMax - errorMin);
				*(film->channel_NOISE->GetPixel(i)) = noise;
			}
		}

		// Copy the current image
		referenceImage->Copy(film->channel_IMAGEPIPELINEs[0]);

		SLG_LOG("Convergence test: ToDo Pixels = " << todoPixelsCount);

		if ((threshold > 0.f) && (todoPixelsCount == 0))
			SLG_LOG("Convergence 100%, rendering done.");

		return (threshold == 0.f) ? pixelsCount : todoPixelsCount;
	}
}

template<class Archive> void FilmConvTest::serialize(Archive &ar, const u_int version) {
	ar & todoPixelsCount;
	ar & maxDiff;

	ar & threshold;
	ar & warmup;
	ar & testStep;
	ar & film;
	ar & referenceImage;
	ar & lastSamplesCount;
	ar & firstTest;
}

namespace slg {
// Explicit instantiations for portable archives
template void FilmConvTest::serialize(LuxOutputArchive &ar, const u_int version);
template void FilmConvTest::serialize(LuxInputArchive &ar, const u_int version);
// The following 2 lines shouldn't be required but they are with GCC 5
template void FilmConvTest::serialize(boost::archive::polymorphic_oarchive &ar, const u_int version);
template void FilmConvTest::serialize(boost::archive::polymorphic_iarchive &ar, const u_int version);
}