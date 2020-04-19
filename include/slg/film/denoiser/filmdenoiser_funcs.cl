#line 2 "filmdenoiser_funcs.cl"

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

OPENCL_FORCE_INLINE void SamplesAccumulator_AtomicAdd(__global float *buff,
		const uint buffWidth, const uint buffHeight, const uint buffDepth,
		const uint line, const uint column, const uint index,
		const float value) {
	AtomicAdd(&buff[(line * buffWidth + column) * buffDepth + index], value);
}

OPENCL_FORCE_INLINE void SamplesAccumulator_AddSampleAtomic(
		const uint line, const uint column,
		const float3 sample, const float weight,
		const uint filmWidth, const uint filmHeight
		FILM_DENOISER_PARAM_DECL) {
	const float satureLevelGamma = 2.f; // used for determining the weight to give to the sample in the highest two bins, when the sample is saturated

	// Sample count
	SamplesAccumulator_AtomicAdd(filmDenoiserNbOfSamplesImage,
			filmWidth, filmHeight, 1,
			line, column, 0,
			weight);
	// Sample squared count
	SamplesAccumulator_AtomicAdd(filmDenoiserSquaredWeightSumsImage,
			filmWidth, filmHeight, 1,
			line, column, 0,
			weight * weight);

	// Samples mean
	SamplesAccumulator_AtomicAdd(filmDenoiserMeanImage,
			filmWidth, filmHeight, 3,
			line, column, 0,
			weight * sample.x);
	SamplesAccumulator_AtomicAdd(filmDenoiserMeanImage,
			filmWidth, filmHeight, 3,
			line, column, 1,
			weight * sample.y);
	SamplesAccumulator_AtomicAdd(filmDenoiserMeanImage,
			filmWidth, filmHeight, 3,
			line, column, 2,
			weight * sample.z);

	// Covariance
	SamplesAccumulator_AtomicAdd(filmDenoiserCovarImage,
			filmWidth, filmHeight, 6,
			line, column, 0,
			weight * sample.x * sample.x);
	SamplesAccumulator_AtomicAdd(filmDenoiserCovarImage,
			filmWidth, filmHeight, 6,
			line, column, 1,
			weight * sample.y * sample.y);
	SamplesAccumulator_AtomicAdd(filmDenoiserCovarImage,
			filmWidth, filmHeight, 6,
			line, column, 2,
			weight * sample.z * sample.z);
	SamplesAccumulator_AtomicAdd(filmDenoiserCovarImage,
			filmWidth, filmHeight, 6,
			line, column, 3,
			weight * sample.y * sample.z);
	SamplesAccumulator_AtomicAdd(filmDenoiserCovarImage,
			filmWidth, filmHeight, 6,
			line, column, 4,
			weight * sample.x * sample.z);
	SamplesAccumulator_AtomicAdd(filmDenoiserCovarImage,
			filmWidth, filmHeight, 6,
			line, column, 5,
			weight * sample.x * sample.y);

	int floorBinIndex;
	int ceilBinIndex;
	float binFloatIndex;
	float floorBinWeight;
	float ceilBinWeight;

	for (int channelIndex = 0; channelIndex < 3; ++channelIndex) { // fill histogram; code refactored from Ray Histogram Fusion PBRT code
		float value = (channelIndex == 0) ? sample.x : ((channelIndex == 1) ? sample.y : sample.z);
		value = (value > 0 ? value : 0);
		if (filmDenoiserGamma > 1)
			value = pow(value, 1.f / filmDenoiserGamma); // exponential scaling
		if (filmDenoiserMaxValue > 0)
			value = (value / filmDenoiserMaxValue); // normalize to the maximum value
		value = value > satureLevelGamma ? satureLevelGamma : value;

		binFloatIndex = value * (filmDenoiserNbOfBins - 2);
		floorBinIndex = (int)binFloatIndex;

		if (floorBinIndex < filmDenoiserNbOfBins - 2) // in bounds
		{
			ceilBinIndex = floorBinIndex + 1;
			ceilBinWeight = binFloatIndex - floorBinIndex;
			floorBinWeight = 1.0f - ceilBinWeight;
		} else { //out of bounds... v >= 1
			floorBinIndex = filmDenoiserNbOfBins - 2;
			ceilBinIndex = floorBinIndex + 1;
			ceilBinWeight = (value - 1.0f) / (satureLevelGamma - 1.f);
			floorBinWeight = 1.0f - ceilBinWeight;
		}

		SamplesAccumulator_AtomicAdd(filmDenoiserHistoImage,
			filmWidth, filmHeight, filmDenoiserNbOfBins * 3,
			line, column, channelIndex * filmDenoiserNbOfBins + floorBinIndex,
			weight * floorBinWeight);
		SamplesAccumulator_AtomicAdd(filmDenoiserHistoImage,
			filmWidth, filmHeight, filmDenoiserNbOfBins * 3,
			line, column, channelIndex * filmDenoiserNbOfBins + ceilBinIndex,
			weight * ceilBinWeight);
	}
}

OPENCL_FORCE_INLINE void FilmDenoiser_AddSample(
		__constant const Film* restrict film,
		const uint x, const uint y,
		__global SampleResult *sampleResult,
		const float weight,
		const uint filmWidth, const uint filmHeight
		FILM_DENOISER_PARAM_DECL) {
	if (!filmDenoiserWarmUpDone)
		return;

	const float3 sample = clamp(SampleResult_GetSpectrum(film,sampleResult, filmRadianceGroupScale) * filmDenoiserSampleScale,
			0.f, filmDenoiserMaxValue);
	
	if (!Spectrum_IsNanOrInf(sample)) {
			const int line = filmHeight - y - 1;
			const int column = x;

			SamplesAccumulator_AddSampleAtomic(line, column, sample, weight,
					filmWidth, filmHeight
					FILM_DENOISER_PARAM);
	}
}
