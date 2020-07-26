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

static void ScaledClamp(float value[4], const float low, const float high) {
	// I have already checked inside Clamp() that value[3] > 0.f
	const float invWeight = 1.f / value[3];

	const float maxValue = Max(value[0] * invWeight, Max(value[1] * invWeight, value[2] * invWeight));

	if (maxValue > 0.f) {
		if (maxValue > high) {
			const float scale = high / maxValue;

			value[0] *= scale;
			value[1] *= scale;
			value[2] *= scale;
			return;
		}

		if (maxValue < low) {
			const float scale = low / maxValue;

			value[0] *= scale;
			value[1] *= scale;
			value[2] *= scale;
			return;
		}
	}
}

void VarianceClamping::Clamp(const float expectedValue[4], float value[4]) const {
	if (value[3] <= 0.f)
		return;

	if (expectedValue[3] > 0.f) {
		// Use the current pixel value as expected value
		const float invWeight = 1.f / expectedValue[3];

		const float minExpectedValue = Min(expectedValue[0] * invWeight,
				Min(expectedValue[1] * invWeight, expectedValue[2] * invWeight));
		const float maxExpectedValue = Max(expectedValue[0] * invWeight,
				Max(expectedValue[1] * invWeight, expectedValue[2] * invWeight));

		const float delta = sqrtVarianceClampMaxValue;

		ScaledClamp(value,
				Max(minExpectedValue - delta, 0.f),
				maxExpectedValue + delta);
	} else
		ScaledClamp(value, 0.f, sqrtVarianceClampMaxValue);	
}

void VarianceClamping::Clamp(const Film &film, SampleResult &sampleResult) const {
	// Recover the current pixel value
	u_int x, y;
	if (sampleResult.useFilmSplat) {
		x = Floor2UInt(sampleResult.filmX);
		y = Floor2UInt(sampleResult.filmY);
	} else {
		x = sampleResult.pixelX;
		y = sampleResult.pixelY;
	}

	// A safety net to avoid out of bound accesses to Film channels
	const u_int *subRegion = film.GetSubRegion();
	x = luxrays::Clamp(x, subRegion[0], subRegion[1]);
	y = luxrays::Clamp(y, subRegion[2], subRegion[3]);
	
	if (sampleResult.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED)) {
		// Apply variance clamping to each radiance group. This help to avoid problems
		// with extreme clamping settings and multiple light groups
		for (u_int radianceGroupIndex = 0; radianceGroupIndex < sampleResult.radiance.Size(); ++radianceGroupIndex) {
			float expectedValue[3];
			film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[radianceGroupIndex]->GetWeightedPixel(x, y, &expectedValue[0]);

			// Use the current pixel value as expected value
			const float minExpectedValue = Min(expectedValue[0], Min(expectedValue[1], expectedValue[2]));
			const float maxExpectedValue = Max(expectedValue[0], Max(expectedValue[1], expectedValue[2]));

			sampleResult.ClampRadiance(radianceGroupIndex,
					Max(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
					maxExpectedValue + sqrtVarianceClampMaxValue);
		}
	} else if (sampleResult.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED)) {
		float expectedValue[3] = { 0.f, 0.f, 0.f };
		for (u_int i = 0; i < film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size(); ++i)
			film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AccumulateWeightedPixel(
					x, y, &expectedValue[0]);
		
		const double lightSampleCount = film.GetTotalLightSampleCount();
		const float factor = (float)((lightSampleCount > 0.0) ?
			(1.0 / lightSampleCount) :
			1.0);

		expectedValue[0] *= factor;
		expectedValue[1] *= factor;
		expectedValue[2] *= factor;
		
		// Use the current pixel value as expected value
		const float minExpectedValue = Min(expectedValue[0], Min(expectedValue[1], expectedValue[2]));
		const float maxExpectedValue = Max(expectedValue[0], Max(expectedValue[1], expectedValue[2]));
		sampleResult.ClampRadiance(
				Max(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
				maxExpectedValue + sqrtVarianceClampMaxValue);
	} else
		throw runtime_error("Unknown sample type in VarianceClamping::Clamp(): " + ToString(sampleResult.GetChannels()));
}
