#line 2 "specturm_funcs.cl"

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

OPENCL_FORCE_INLINE bool Spectrum_IsEqual(const float3 a, const float3 b) {
	return all(isequal(a, b));
}

OPENCL_FORCE_INLINE bool Spectrum_IsBlack(const float3 a) {
	return Spectrum_IsEqual(a, BLACK);
}

OPENCL_FORCE_INLINE bool Spectrum_IsNan(const float3 a) {
	return any(isnan(a));
}

OPENCL_FORCE_INLINE bool Spectrum_IsInf(const float3 a) {
	return any(isinf(a));
}

OPENCL_FORCE_INLINE bool Spectrum_IsNanOrInf(const float3 a) {
	return Spectrum_IsNan(a) || Spectrum_IsInf(a);
}

OPENCL_FORCE_INLINE float Spectrum_Filter(const float3 s)  {
	return (s.s0 + s.s1 + s.s2) * 0.33333333f;
}

OPENCL_FORCE_INLINE float Spectrum_Y(const float3 s) {
	return 0.212671f * s.s0 + 0.715160f * s.s1 + 0.072169f * s.s2;
}

OPENCL_FORCE_INLINE float3 Spectrum_Clamp(const float3 s) {
	return clamp(s, BLACK, WHITE);
}

OPENCL_FORCE_INLINE float3 Spectrum_Exp(const float3 s) {
	return (float3)(exp(s.x), exp(s.y), exp(s.z));
}

OPENCL_FORCE_INLINE float3 Spectrum_Pow(const float3 s, const float e) {
	return (float3)(pow(s.x, e), pow(s.y, e), pow(s.z, e));
}

OPENCL_FORCE_INLINE float3 Spectrum_Sqrt(const float3 s) {
	return (float3)(sqrt(s.x), sqrt(s.y), sqrt(s.z));
}

OPENCL_FORCE_INLINE float3 Spectrum_ScaledClamp(const float3 c, const float low, const float high) {
	float3 ret = c;

	const float maxValue = fmax(c.x, fmax(c.y, c.z));
	if (maxValue > 0.f) {
		if (maxValue > high) {
			const float scale = high / maxValue;
			ret *= scale;
		}

		if (maxValue < low) {
			const float scale = low / maxValue;
			ret *= scale;
		}
	}

	return ret;
}
