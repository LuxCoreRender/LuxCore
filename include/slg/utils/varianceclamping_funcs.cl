#line 2 "varianceclamping_funcs.cl"

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

float3 VarianceClamping_GetWeightedFloat4(__global float *src) {
	const float4 val = VLOAD4F_Align(src);

	if (val.w > 0.f) {
		const float k = 1.f / val.w;
		
		return ((float3)(val.x, val.y, val.z)) * k;
	}
}

void VarianceClamping_Clamp(__global SampleResult *sampleResult, const float sqrtVarianceClampMaxValue
	FILM_PARAM_DECL) {
	// Recover the current pixel value
	const int x = Floor2Int(sampleResult->filmX);
	const int y = Floor2Int(sampleResult->filmY);
	const uint index1 = x + y * filmWidth;
	const uint index4 = index1 * 4;

	float3 expectedValue = 0.f;

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[0])[index4]));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[1])[index4]));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[2])[index4]));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[3])[index4]));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[4])[index4]));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[5])[index4]));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[6]])[index4]));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	expectedValue += VarianceClamping_GetWeightedFloat4(&((filmRadianceGroup[7])[index4]));
#endif

	// Use the current pixel value as expected value
	const float minExpectedValue = fmin(expectedValue.x, fmin(expectedValue.y, expectedValue.z));
	const float maxExpectedValue = fmax(expectedValue.x, fmax(expectedValue.y, expectedValue.z));
	SampleResult_ClampRadiance(sampleResult,
			fmax(minExpectedValue - sqrtVarianceClampMaxValue, 0.f),
			maxExpectedValue + sqrtVarianceClampMaxValue);
}
