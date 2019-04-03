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

		u_int zeroDiffs = 0;
		u_int nanDiffs = 0;
		u_int infDiffs = 0;
		
		std::vector<float> pixelDiffVector(pixelsCount, 0);

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

			float diff = (dr + dg + db)/sqrt(imgR + imgG + imgB);
			if (isnan(diff) || isinf(diff)) {
				// SLG_LOG("NaN/Inf at diff calculation: " << diff << " " << dr << " " << dg << " " << db << " " << imgR << " " << imgG << " " << imgB);
				// ToDo: revise
				diff = 1;
			}

			if (diff == 0.f) { zeroDiffs++; }
			if (isnan(diff)) { nanDiffs++; /* SLG_LOG("NaN Diff: " << dr << " " << dg << " " << db << " " << imgR << " " << imgB << " " << imgG); */ }
			if (isinf(diff)) { infDiffs++; /* SLG_LOG("Inf Diff: " << dr << " " << dg << " " << db << " " << imgR << " " << imgB << " " << imgG); */ }
			
			diffVector[i] = diff;
			pixelDiffVector[i] = diff;
			maxError = Max(maxError, diff);
			if (diff > threshold) ++todoPixelsCount;
		}

		const int height = film->GetHeight();
		const int width = film->GetWidth();
		SLG_LOG("pixelsCount: " << pixelsCount);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float diffAccumulator = 0;
				for (int r = (0 > y - 4 ? 0 : y - 4); r < (height < y + 4 ? height : y + 4); r++) {
					if (r >= height || r < 0) { SLG_LOG("wrong r: " << r);}
					for (int c = (0 > x - 4 ? 0 : x - 4); c < (width < x + 4 ? width : x + 4); c++) {
						if (c >= width || c < 0) { SLG_LOG("wrong c: " << c);}
						diffAccumulator += pixelDiffVector[r * width + c];
					}
				}
				if (!(isnan(diffAccumulator) || isinf(diffAccumulator))) {
					if ((y * width + x) > (int)pixelsCount)  { SLG_LOG("wrong pixel: " << (y * width + x));}
					diffVector[y * width + x] = diffAccumulator/81;
				}
			}
		}
		
		float diffMean = 0;
		float diffStd = 0;
		float accumulator = 0;

		for (u_int j = 0; j < pixelsCount; ++j) {
			const float pixelVal = diffVector[j];
			if (isnan(pixelVal) || isinf(pixelVal)) { continue; }
			accumulator += pixelVal;
		}
		diffMean = accumulator/pixelsCount;
		
		accumulator = 0;
		for (u_int j = 0; j < pixelsCount; ++j) {
			const float pixelVal = diffVector[j];
			if (isnan(pixelVal) || isinf(pixelVal)) { continue; }
			accumulator += pow(pixelVal - diffMean, 2);
		}
		SLG_LOG("accumulator: " << accumulator);
		diffStd = sqrt((1.f/pixelsCount)*accumulator);
		

		float diffMax = -3;
		float diffMin = 3;
		for (u_int j = 0; j < pixelsCount; j++) {
			// Calculate standard score
			const float score = Clamp((diffVector[j] - diffMean)/diffStd, -3.f, 3.f);
			diffVector[j] = score;
			diffMax = Max(score, diffMax);
			diffMin = Min(score, diffMin);
		}
		SLG_LOG("Mean: " << diffMean << " Std: " << diffStd <<" Max: " << diffMax << " Min: " << diffMin)

		SLG_LOG("Zero diffs detected: " << zeroDiffs);
		SLG_LOG("NaN diffs detected: " << nanDiffs);
		SLG_LOG("inf diffs detected: " << infDiffs);
		
		if (hasConvChannel) {
			SLG_LOG("update Convergence");
			
			float maxConv = 0;
			float minConv = 0;
			float bin1 = 0;
			float bin2 = 0;
			float bin3 = 0;
			float bin4 = 0;
			float bin5 = 0;
			for (u_int i = 0; i < pixelsCount; ++i) {
				// Update CONVERGENCE channel
				const float pixelVal = diffVector[i];
				if (isnan(pixelVal) || isinf(pixelVal)) {
					continue;
				}
				float conv = (pixelVal - diffMin)/(diffMax - diffMin);
				if (isnan(conv) || isinf(conv)) {
					SLG_LOG("Tried to store NaN/inf conv on CONVERGENCE: " << pixelVal << " " << diffMax << " " << diffMin);
					// Set maximum likelyhood of being sampled
					*(film->channel_CONVERGENCE->GetPixel(i)) = 1;
				} else {
					*(film->channel_CONVERGENCE->GetPixel(i)) = conv;
				}
				maxConv = Max(*(film->channel_CONVERGENCE->GetPixel(i)), maxConv);
				minConv = Min(*(film->channel_CONVERGENCE->GetPixel(i)), minConv);
				if (conv > 0.8) bin5++;
				if (conv > 0.6) bin4++;
				if (conv > 0.4) bin3++;
				if (conv > 0.2) bin2++;
				if (conv > 0.0) bin1++;
			}
			SLG_LOG("Max conv: " << maxConv << " Min conv: " << minConv);
			SLG_LOG("Bins: " << bin1 << " " << bin2 << " " << bin3 << " " << bin4 << " " << bin5);
		}

		if (hasConvChannel && useFilter) {
			GaussianBlur3x3FilterPlugin::ApplyBlurFilter(film->GetWidth(), film->GetHeight(),
					film->channel_CONVERGENCE->GetPixels(), referenceImage->GetPixels(),
					1.f, 1.f, 1.f);
		}

		// Copy the current image
		referenceImage->Copy(film->channel_IMAGEPIPELINEs[0]);

		SLG_LOG("Convergence test: ToDo Pixels = " << todoPixelsCount << ", Max. Error = " << maxError << " [" << (256.f * maxError) << "/256]");

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