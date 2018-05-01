#line 2 "filmdenoiser_funcs.cl"

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

#if defined(PARAM_FILM_DENOISER)

OPENCL_FORCE_INLINE void FilmDenoiser_AddSample(
		const uint filmWidth, const uint filmHeight,
		const int filmDenoiserWarmUpDone,
		const float filmDenoiserMaxValue,
		const float filmDenoiserSampleScale,
		const uint filmDenoiserNbOfBins,
		__global float *filmDenoiserNbOfSamplesImage,
		__global float *filmDenoiserSquaredWeightSumsImage,
		__global float *filmDenoiserMeanImage,
		__global float *filmDenoiserCovarImage,
		__global float *filmDenoiserHistoImage,
		float3 filmRadianceGroupScale[PARAM_FILM_RADIANCE_GROUP_COUNT],
		__global SampleResult *sampleResult) {
	if (!filmDenoiserWarmUpDone)
		return;

	float3 sample = clamp(SampleResult_GetSpectrum(sampleResult, filmRadianceGroupScale) * filmDenoiserSampleScale,
			0.f, filmDenoiserMaxValue);
	
	if (!Spectrum_IsNanOrInf(sample)) {
		// TODO
	}
}

#endif
