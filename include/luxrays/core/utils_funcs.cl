#line 2 "utils_funcs.cl"

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

int Mod(int a, int b) {
	if (b == 0)
		b = 1;

	a %= b;
	if (a < 0)
		a += b;

	return a;
}

float Radians(float deg) {
	return (M_PI_F / 180.f) * deg;
}

float Degrees(float rad) {
	return (180.f / M_PI_F) * rad;
}

int Ceil2Int(float val) {
	return (int)ceil(val);
}

uint Ceil2UInt(float val) {
	return (uint)ceil(val);
}

int Floor2Int(const float val) {
	return (int)floor(val);
}

unsigned int Floor2UInt(const float val) {
	return (val > 0.f) ? ((unsigned int)floor(val)) : 0;
}

float Lerp(const float t, const float v1, const float v2) {
	// Linear interpolation
	return mix(v1, v2, t);
}

float3 Lerp3(const float t, const float3 v1, const float3 v2) {
	// Linear interpolation
	return mix(v1, v2, t);
}

float Cerp(float t, float v0, float v1, float v2, float v3) {
	// Cubic interpolation
	return v1 + .5f *
			t * (v2 - v0 +
				t * (2.f * v0 - 5.f * v1 + 4.f * v2 - v3 +
					t * (3.f * (v1 - v2) + v3 - v0)));
}

float3 Cerp3(float t, float3 v0, float3 v1, float3 v2, float3 v3) {
	// Cubic interpolation
	return v1 + .5f *
			t * (v2 - v0 +
				t * (2.f * v0 - 5.f * v1 + 4.f * v2 - v3 +
					t * (3.f * (v1 - v2) + v3 - v0)));
}

float SmoothStep(const float min, const float max, const float value) {
	const float v = clamp((value - min) / (max - min), 0.f, 1.f);
	return v * v * (-2.f * v  + 3.f);
}

float CosTheta(const float3 v) {
	return v.z;
}

float SinTheta2(const float3 w) {
	return fmax(0.f, 1.f - CosTheta(w) * CosTheta(w));
}

float SinTheta(const float3 w) {
	return sqrt(SinTheta2(w));
}

float CosPhi(const float3 w) {
	const float sinTheta = SinTheta(w);
	return sinTheta > 0.f ? clamp(w.x / sinTheta, -1.f, 1.f) : 1.f;
}

float SinPhi(const float3 w) {
	const float sinTheta = SinTheta(w);
	return sinTheta > 0.f ? clamp(w.y / sinTheta, -1.f, 1.f) : 0.f;
}

float3 SphericalDirection(float sintheta, float costheta, float phi) {
	return (float3)(sintheta * cos(phi), sintheta * sin(phi), costheta);
}

float3 SphericalDirectionWithFrame(float sintheta, float costheta, float phi,
	const float3 x, const float3 y, const float3 z) {
	return sintheta * cos(phi) * x + sintheta * sin(phi) * y +
		costheta * z;
}
