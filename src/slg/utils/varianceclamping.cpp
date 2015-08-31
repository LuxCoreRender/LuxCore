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

#include "slg/utils/varianceclamping.h"
#include "slg/film/sampleresult.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// VarianceClamping
//------------------------------------------------------------------------------

VarianceClamping::VarianceClamping() :
		sqrtVarianceClampMaxValue(0.f) {
}

VarianceClamping::VarianceClamping(const float sqrtMaxValue) :
		sqrtVarianceClampMaxValue(sqrtMaxValue) {
}

static void ScaledClamp(float value[4], const float cap) {
	if (value[3] > 0.f) {
		const float maxValue = Max(value[0], Max(value[1], value[2]));
		const float scaledCap = cap * value[3];

		if ((maxValue > 0.f) && (maxValue > scaledCap)) {
			const float scale = scaledCap / maxValue;

			value[0] *= scale;
			value[1] *= scale;
			value[2] *= scale;
		}
	}
}

void VarianceClamping::Clamp(const float expectedValue[4], float value[4]) const {
	// Use the current pixel value as expected value
	const float maxExpectedValue = (expectedValue[3] > 0.f) ?
		(Max(expectedValue[0], Max(expectedValue[1], expectedValue[2])) / expectedValue[3]) :
		0.f;
	const float capValue = maxExpectedValue + sqrtVarianceClampMaxValue;

	ScaledClamp(value, capValue);
}

void VarianceClamping::Clamp(const Film &film, SampleResult &sampleResult) const {
	// Recover the current pixel value
	const int x = Min(Max(Ceil2Int(sampleResult.filmX - .5f), 0), (int)film.GetWidth());
	const int y = Min(Max(Ceil2Int(sampleResult.filmY - .5f), 0), (int)film.GetHeight());

	float expectedValue[3] = { 0.f, 0.f, 0.f };
	if (sampleResult.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size(); ++i)
			film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AccumulateWeightedPixel(
					x, y, &expectedValue[0]);
	} else {
		for (u_int i = 0; i < film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size(); ++i)
			film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AccumulateWeightedPixel(
					x, y, &expectedValue[0]);			
	}

	// Use the current pixel value as expected value
	const float maxExpectedValue = Max(expectedValue[0], Max(expectedValue[1], expectedValue[2]));
	const float minExpectedValue = Min(expectedValue[0], Min(expectedValue[1], expectedValue[2]));
	sampleResult.ClampRadiance(
			Max(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
			maxExpectedValue + sqrtVarianceClampMaxValue);
}
