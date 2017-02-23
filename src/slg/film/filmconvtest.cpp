/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include <limits>

#include "slg/film/film.h"
#include "slg/film/filmconvtest.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmConvTest
//------------------------------------------------------------------------------

FilmConvTest::FilmConvTest(const Film *flm) : film(flm) {
	referenceImage = new GenericFrameBuffer<3, 0, float>(film->GetWidth(), film->GetHeight());

	Reset();
}

FilmConvTest::FilmConvTest() {
}

FilmConvTest::~FilmConvTest() {
}

void FilmConvTest::Reset() {
	todoPixelsCount = film->GetWidth() * film->GetHeight();
	maxError = numeric_limits<float>::infinity();

	referenceImage->Clear(0.f);
	firstTest = true;
}

u_int FilmConvTest::Test(const float threshold) {
	const u_int pixelsCount = film->GetWidth() * film->GetHeight();

	if (firstTest) {
		// Copy the current image
		referenceImage->Copy(film->channel_IMAGEPIPELINEs[0]);
		firstTest = false;

		SLG_LOG("Convergence test first pass");

		return pixelsCount;
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
