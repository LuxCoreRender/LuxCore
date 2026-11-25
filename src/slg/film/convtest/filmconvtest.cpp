/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include <boost/serialization/shared_ptr.hpp>

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

FilmConvTest::FilmConvTest(
	FilmConstPtr flm,
	const float thresholdVal,
	const u_int warmupVal,
	const u_int testStepVal,
	const bool useFilt,
	const u_int idx
) :
	film(flm),
	threshold(thresholdVal),
	warmup(warmupVal),
	testStep(testStepVal),
	useFilter(useFilt),
	imagePipelineIndex(idx),
	referenceImage(nullptr)
{
	Reset();
}

//FilmConvTest::FilmConvTest() {
	//referenceImage = NULL;
//}

FilmConvTest::~FilmConvTest() {
	delete referenceImage;
}

void FilmConvTest::Reset() {
	todoPixelsCount = GetFilm().GetWidth() * GetFilm().GetHeight();
	maxError = numeric_limits<float>::infinity();

	delete referenceImage;
	referenceImage = new GenericFrameBuffer<3, 0, float>(GetFilm().GetWidth(), GetFilm().GetHeight());

	lastSamplesCount = 0.0;
	firstTest = true;
}

bool FilmConvTest::IsTestUpdateRequired() const {
	const u_int pixelsCount = GetFilm().GetWidth() * GetFilm().GetHeight();

	// Run the test only after a initial warmup
	if (GetFilm().GetTotalSampleCount() / pixelsCount <= warmup)
		return false;

	// Do not run the test if we don't have at least testStep new samples per pixel
	if (GetFilm().GetTotalSampleCount() - lastSamplesCount <= pixelsCount * static_cast<double>(testStep))
		return false;

	return true;
}

u_int FilmConvTest::Test() {
	const u_int pixelsCount = GetFilm().GetWidth() * GetFilm().GetHeight();

	if (IsTestUpdateRequired()) {
		lastSamplesCount = GetFilm().GetTotalSampleCount();

		const u_int index = (imagePipelineIndex <= (GetFilm().GetImagePipelineCount() - 1)) ? imagePipelineIndex : 0;

		if (firstTest) {
			SLG_LOG("Convergence test first pass");

			// Copy the current image
			referenceImage->Copy(GetFilm().channel_IMAGEPIPELINEs[index]);
			firstTest = false;
		} else {
			// Check the number of pixels over the threshold
			const float *ref = referenceImage->GetPixels();
			const float *img = GetFilm().channel_IMAGEPIPELINEs[index]->GetPixels();

			todoPixelsCount = 0;
			maxError = 0.f;
			const bool hasConvChannel = GetFilm().HasChannel(Film::CONVERGENCE);

			for (u_int i = 0; i < pixelsCount; ++i) {
				const float dr = fabsf((*img++) - (*ref++));
				const float dg = fabsf((*img++) - (*ref++));
				const float db = fabsf((*img++) - (*ref++));
				const float diff = Max(Max(dr, dg), db);
				maxError = Max(maxError, diff);

				if (diff > threshold)
					++todoPixelsCount;

				// Update the CONVERGENCE channel
				if (hasConvChannel)
					*(GetFilm().channel_CONVERGENCE->GetPixel(i)) = Max(diff - threshold, 0.f);
			}

			if (hasConvChannel && useFilter) {
				GaussianBlur3x3FilterPlugin::ApplyBlurFilter(GetFilm().GetWidth(), GetFilm().GetHeight(),
						GetFilm().channel_CONVERGENCE->GetPixels(), referenceImage->GetPixels(),
						1.f, 1.f, 1.f);
			}


			// Copy the current image
			referenceImage->Copy(GetFilm().channel_IMAGEPIPELINEs[index]);

			SLG_LOG("Convergence test: ToDo Pixels = " << todoPixelsCount << ", Max. Error = " << maxError << " [" << (256.f * maxError) << "/256]");

			if ((threshold > 0.f) && (todoPixelsCount == 0))
				SLG_LOG("Convergence 100%, rendering done.");
		}
	}

	return (threshold == 0.f) ? pixelsCount : todoPixelsCount;
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
template void FilmConvTest::serialize(LuxOutputArchiveText &ar, const u_int version);
template void FilmConvTest::serialize(LuxInputArchiveText &ar, const u_int version);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
