#line 2 "utils_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

int Mod(int a, int b) {
	if (b == 0)
		b = 1;

	a %= b;
	if (a < 0)
		a += b;

	return a;
}

int Floor2Int(const float val) {
	return (int)floor(val);
}

float Lerp(const float t, const float v1, const float v2) {
	return mix(v1, v2, t);
}

float3 Lerp3(const float t, const float3 v1, const float3 v2) {
	return mix(v1, v2, t);
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

float3 SphericalDirection(float sintheta, float costheta, float phi) {
	return (float3)(sintheta * cos(phi), sintheta * sin(phi), costheta);
}
