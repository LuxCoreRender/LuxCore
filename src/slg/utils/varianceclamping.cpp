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

//------------------------------------------------------------------------------

static void ScaledClamp3(float value[3], const float low, const float high) {
	const float maxValue = Max(value[0], Max(value[1], value[2]));

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

void VarianceClamping::Clamp3(const float expectedValue[4], float value[3]) const {
	if (expectedValue[3] > 0.f) {
		// Use the current pixel value as expected value
		const float invWeight = 1.f / expectedValue[3];

		const float minExpectedValue = Min(expectedValue[0] * invWeight,
				Min(expectedValue[1] * invWeight, expectedValue[2] * invWeight));
		const float maxExpectedValue = Max(expectedValue[0] * invWeight,
				Max(expectedValue[1] * invWeight, expectedValue[2] * invWeight));

		ScaledClamp3(value,
				Max(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
				maxExpectedValue + sqrtVarianceClampMaxValue);
	} else
		ScaledClamp3(value, 0.f, sqrtVarianceClampMaxValue);	
}

//------------------------------------------------------------------------------

static void ScaledClamp4(float value[4], const float low, const float high) {
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

void VarianceClamping::Clamp4(const float expectedValue[4], float value[4]) const {
	if (value[3] <= 0.f)
		return;

	if (expectedValue[3] > 0.f) {
		// Use the current pixel value as expected value
		const float invWeight = 1.f / expectedValue[3];

		const float minExpectedValue = Min(expectedValue[0] * invWeight,
				Min(expectedValue[1] * invWeight, expectedValue[2] * invWeight));
		const float maxExpectedValue = Max(expectedValue[0] * invWeight,
				Max(expectedValue[1] * invWeight, expectedValue[2] * invWeight));

		ScaledClamp4(value,
				Max(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
				maxExpectedValue + sqrtVarianceClampMaxValue);
	} else
		ScaledClamp4(value, 0.f, sqrtVarianceClampMaxValue);	
}

//------------------------------------------------------------------------------

void VarianceClamping::ClampFilm(Film &dstFilm , const Film &srcFilm,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) const {
	if (dstFilm.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED) && srcFilm.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(dstFilm.GetRadianceGroupCount(), srcFilm.GetRadianceGroupCount()); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					float *srcPixel = srcFilm.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					const float *dstPixel = dstFilm.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(dstOffsetX + x, dstOffsetY + y);

					Clamp4(dstPixel, srcPixel);
				}
			}
		}
	}
}

void VarianceClamping::ClampFilm(Film &dstFilm , const Film &srcFilm) const {
	ClampFilm(dstFilm, srcFilm, 0, 0, dstFilm.GetWidth(), dstFilm.GetHeight(), 0, 0);
}

//------------------------------------------------------------------------------

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
			Clamp3(film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[radianceGroupIndex]->GetPixel(x, y),
					sampleResult.radiance[radianceGroupIndex].c);
		}
		
		// Clamp the AOVs too

		if (film.HasChannel(Film::DIRECT_DIFFUSE))
			Clamp3(film.channel_DIRECT_DIFFUSE->GetPixel(x, y), sampleResult.directDiffuse.c);

		if (film.HasChannel(Film::DIRECT_GLOSSY))
			Clamp3(film.channel_DIRECT_GLOSSY->GetPixel(x, y), sampleResult.directGlossy.c);

		if (film.HasChannel(Film::EMISSION))
			Clamp3(film.channel_EMISSION->GetPixel(x, y), sampleResult.emission.c);

		if (film.HasChannel(Film::INDIRECT_DIFFUSE))
			Clamp3(film.channel_INDIRECT_DIFFUSE->GetPixel(x, y), sampleResult.indirectDiffuse.c);

		if (film.HasChannel(Film::INDIRECT_GLOSSY))
			Clamp3(film.channel_INDIRECT_GLOSSY->GetPixel(x, y), sampleResult.indirectGlossy.c);

		if (film.HasChannel(Film::INDIRECT_SPECULAR))
			Clamp3(film.channel_INDIRECT_SPECULAR->GetPixel(x, y), sampleResult.indirectSpecular.c);
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
		const float minRadiance = Max(minExpectedValue - sqrtVarianceClampMaxValue, 0.f);
		const float maxRadiance = maxExpectedValue + sqrtVarianceClampMaxValue;
		
		for (u_int radianceGroupIndex = 0; radianceGroupIndex < sampleResult.radiance.Size(); ++radianceGroupIndex)
			sampleResult.radiance[radianceGroupIndex] = sampleResult.radiance[radianceGroupIndex].ScaledClamp(minRadiance, maxRadiance);
	}
}
