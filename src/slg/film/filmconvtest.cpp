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
#include "slg/film/filmconvtest.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmConvTest
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::FilmConvTest)

FilmConvTest::FilmConvTest(const Film *flm, const float thresholdVal,
		const u_int warmupVal, const u_int testStepVal) :
		threshold(thresholdVal), warmup(warmupVal),	testStep(testStepVal),
		film(flm), referenceImage(NULL) {
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
		// Check the number of pixels over the threshold
		const float *ref = referenceImage->GetPixels();
		const float *img = film->channel_IMAGEPIPELINEs[0]->GetPixels();
		
		todoPixelsCount = 0;
		maxError = 0.f;

		for (u_int i = 0; i < pixelsCount; ++i) {
			const float dr = fabsf((*img++) - (*ref++));
			const float dg = fabsf((*img++) - (*ref++));
			const float db = fabsf((*img++) - (*ref++));
			const float diff = Max(Max(dr, dg), db);
			maxError = Max(maxError, diff);

			if (diff > threshold)
				++todoPixelsCount;
		}
		
		// Copy the current image
		referenceImage->Copy(film->channel_IMAGEPIPELINEs[0]);

		SLG_LOG("Convergence test: ToDo Pixels = " << todoPixelsCount << ", Max. Error = " << maxError << " [" << (256.f * maxError) << "/256]");

		return todoPixelsCount;
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
}
