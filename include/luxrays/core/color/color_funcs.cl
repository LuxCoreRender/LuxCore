#line 2 "specturm_funcs.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

bool Spectrum_IsEqual(const float3 a, const float3 b) {
	return all(isequal(a, b));
}

bool Spectrum_IsBlack(const float3 a) {
	return Spectrum_IsEqual(a, BLACK);
}

float Spectrum_Filter(const float3 s)  {
	return (s.s0 + s.s1 + s.s2) * 0.33333333f;
}

float Spectrum_Y(const float3 s) {
	return 0.212671f * s.s0 + 0.715160f * s.s1 + 0.072169f * s.s2;
}

float3 Spectrum_Clamp(const float3 s) {
	return clamp(s, BLACK, WHITE);
}

float3 Spectrum_Exp(const float3 s) {
	return (float3)(exp(s.x), exp(s.y), exp(s.z));
}

float3 Spectrum_Pow(const float3 s, const float e) {
	return (float3)(pow(s.x, e), pow(s.y, e), pow(s.z, e));
}

float3 Spectrum_Sqrt(const float3 s) {
	return (float3)(sqrt(s.x), sqrt(s.y), sqrt(s.z));
}
