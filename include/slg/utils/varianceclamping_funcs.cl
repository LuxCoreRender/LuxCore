#line 2 "varianceclamping_funcs.cl"

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

OPENCL_FORCE_INLINE void VarianceClamping_ScaledClamp3(__global float *value, const float low, const float high) {
	const float maxValue = fmax(value[0], fmax(value[1], value[2]));

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

OPENCL_FORCE_INLINE void VarianceClamping_Clamp3(const float sqrtVarianceClampMaxValue,
		__global const float *expectedValue, __global float *value) {
	if (expectedValue[3] > 0.f) {
		// Use the current pixel value as expected value
		const float invWeight = 1.f / expectedValue[3];

		const float minExpectedValue = fmin(expectedValue[0] * invWeight,
				fmin(expectedValue[1] * invWeight, expectedValue[2] * invWeight));
		const float maxExpectedValue = fmax(expectedValue[0] * invWeight,
				fmax(expectedValue[1] * invWeight, expectedValue[2] * invWeight));

		VarianceClamping_ScaledClamp3(value,
				fmax(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
				maxExpectedValue + sqrtVarianceClampMaxValue);
	} else
		VarianceClamping_ScaledClamp3(value, 0.f, sqrtVarianceClampMaxValue);	
}

OPENCL_FORCE_INLINE void VarianceClamping_Clamp(
		__global SampleResult *sampleResult, const float sqrtVarianceClampMaxValue
		FILM_PARAM_DECL) {
	// Recover the current pixel value
	const int x = sampleResult->pixelX;
	const int y = sampleResult->pixelY;

	const uint index1 = x + y * filmWidth;
	const uint index4 = index1 * 4;

	// Apply variance clamping to each radiance group. This help to avoid problems
	// with extreme clamping settings and multiple light groups

	for (uint radianceGroupIndex = 0; radianceGroupIndex < film->radianceGroupCount; ++radianceGroupIndex) {
		VarianceClamping_Clamp3(sqrtVarianceClampMaxValue,
				&((filmRadianceGroup[radianceGroupIndex])[index4]),
				sampleResult->radiancePerPixelNormalized[radianceGroupIndex].c);
	}

	// Clamp the AOVs too

	if (film->hasChannelDirectDiffuse)
		VarianceClamping_Clamp3(sqrtVarianceClampMaxValue, &filmDirectDiffuse[index4], sampleResult->directDiffuse.c);

	if (film->hasChannelDirectGlossy)
		VarianceClamping_Clamp3(sqrtVarianceClampMaxValue, &filmDirectGlossy[index4], sampleResult->directGlossy.c);

	if (film->hasChannelEmission)
		VarianceClamping_Clamp3(sqrtVarianceClampMaxValue, &filmEmission[index4], sampleResult->emission.c);

	if (film->hasChannelIndirectDiffuse)
		VarianceClamping_Clamp3(sqrtVarianceClampMaxValue, &filmIndirectDiffuse[index4], sampleResult->indirectDiffuse.c);

	if (film->hasChannelIndirectGlossy)
		VarianceClamping_Clamp3(sqrtVarianceClampMaxValue, &filmIndirectGlossy[index4], sampleResult->indirectGlossy.c);

	if (film->hasChannelIndirectSpecular)
		VarianceClamping_Clamp3(sqrtVarianceClampMaxValue, &filmIndirectSpecular[index4], sampleResult->indirectSpecular.c);
}
