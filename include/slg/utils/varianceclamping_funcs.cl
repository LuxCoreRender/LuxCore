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

OPENCL_FORCE_INLINE float3 VarianceClamping_GetWeightedFloat4(__global float *src) {
	const float4 val = VLOAD4F_Align(src);

	if (val.w > 0.f) {
		const float k = 1.f / val.w;
		
		return (MAKE_FLOAT3(val.x, val.y, val.z)) * k;
	} else
		return BLACK;
}

OPENCL_FORCE_INLINE void VarianceClamping_Clamp(
		__global SampleResult *sampleResult, const float sqrtVarianceClampMaxValue
		FILM_PARAM_DECL) {
	// Recover the current pixel value
	const int x = sampleResult->pixelX;
	const int y = sampleResult->pixelY;

	const uint index1 = x + y * filmWidth;
	const uint index4 = index1 * 4;

	for (uint i = 0; i < film->radianceGroupCount; ++i) {
		float3 expectedValue = BLACK;

		expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[i])[index4]));

		// Use the current pixel value as expected value
		const float minExpectedValue = fmin(expectedValue.x, fmin(expectedValue.y, expectedValue.z));
		const float maxExpectedValue = fmax(expectedValue.x, fmax(expectedValue.y, expectedValue.z));
		SampleResult_ClampRadiance(film, sampleResult, i,
				fmax(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
				maxExpectedValue + sqrtVarianceClampMaxValue);
	}
}
