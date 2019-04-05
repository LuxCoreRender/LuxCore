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
		const u_int warmupVal, const u_int testStepVal, const bool useFilt) :
		threshold(thresholdVal), warmup(warmupVal),	testStep(testStepVal),
		useFilter(useFilt), film(flm), referenceImage(NULL) {
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
	maxError = numeric_limits<float>::infinity();

	delete referenceImage;
	referenceImage = new GenericFrameBuffer<3, 0, float>(film->GetWidth(), film->GetHeight());

	// Allocate new framebuffers according to film size
	u_int pixelsCount = film->GetWidth() * film->GetHeight();

	// Resize vectors according to film size. Start at zero
	diffVector.resize(pixelsCount, 0);

	passCount = 0;
	lastSamplesCount = 0.0;
	firstTest = true;
}

u_int FilmConvTest::Test() {
	const u_int pixelsCount = film->GetWidth() * film->GetHeight();

	// Run the test only after a initial warmup
	if (film->GetTotalSampleCount() / pixelsCount <= warmup)
		return todoPixelsCount;

	// Do not run the test if we don't have at least batch.haltthreshold.step new samples per pixel
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
		maxError = 0.f; 
		const bool hasConvChannel = film->HasChannel(Film::CONVERGENCE);
	
		vector<float> pixelDiffVector(pixelsCount, 0);

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

			// This changes the diff value from having a pixel dimension to
			// Having a pixel/sqrt(pixel) dimension. It might impact other stuff.
			// Thereshold, for example, would use a different range
			const float imgSum = imgR + imgG + imgB;
			float diff;
			if (imgSum != 0) {
				diff = (dr + dg + db)/sqrt(imgR + imgG + imgB);
			} else {
				// ToDo: revise
				diff = 0;
			}
			
			pixelDiffVector[i] = diff;
			maxError = Max(maxError, diff);
			if (diff > threshold) ++todoPixelsCount;
		}

		// Filter convergence using a 9x9 window average. 
		// The window becomes smaller at the borders
		const int height = film->GetHeight();
		const int width = film->GetWidth();
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				float diffAccumulator = 0;
				const int minHeight = (0 > i - 4 ? 0 : i - 4);
				const int maxHeight = (height < i + 4 ? height : i + 4);
				const int minWidth = (0 > j - 4 ? 0 : j - 4);
				const int maxWidth = (width < j + 4 ? width : j + 4);
				for (int r = minHeight; r < maxHeight; r++) {
					for (int c = minWidth; c < maxWidth; c++) {
						diffAccumulator += pixelDiffVector[r * width + c];
					}
				}

				if (!(isnan(diffAccumulator) || isinf(diffAccumulator))) {
					const u_int windowSize =  (maxHeight - minHeight) * (maxWidth - minWidth);
					diffVector[i * width + j] = diffAccumulator/windowSize;
				}
			}
		}
		
		float diffMean = 0;
		float diffStd = 0;
		float accumulator = 0;

		// Calculate the difference mean after the filtering
		for (u_int j = 0; j < pixelsCount; ++j) {
			const float pixelVal = diffVector[j];
			if (isnan(pixelVal) || isinf(pixelVal)) { continue; }
			accumulator += pixelVal;
		}
		diffMean = accumulator / pixelsCount;
		
		// Calculate the difference standard deviation after the filtering
		accumulator = 0;
		for (u_int j = 0; j < pixelsCount; ++j) {
			const float pixelVal = diffVector[j];
			if (isnan(pixelVal) || isinf(pixelVal)) { continue; }
			accumulator += pow(pixelVal - diffMean, 2);
		}
		diffStd = sqrt((1.f / pixelsCount)*accumulator);


		float diffMax = numeric_limits<float>::lowest();
		float diffMin = numeric_limits<float>::infinity();
		for (u_int j = 0; j < pixelsCount; j++) {
			// Calculate standard score. Clamp value at 3 standard deviations from mean
			const float score = Clamp((diffVector[j] - diffMean) / diffStd, -3.f, 3.f);
			diffVector[j] = score;
			// Find maximum and minimum standard scores
			diffMax = Max(score, diffMax);
			diffMin = Min(score, diffMin);
		}
		
		if (hasConvChannel) {
			for (u_int i = 0; i < pixelsCount; ++i) {
				// Update CONVERGENCE channel
				const float conv = (diffVector[i] - diffMin) / (diffMax - diffMin);
				*(film->channel_CONVERGENCE->GetPixel(i)) = conv;
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
	ar & maxError;

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