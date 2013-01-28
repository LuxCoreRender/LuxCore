#line 2 "specturm_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

bool Spectrum_IsEqual(const float3 a, const float3 b) {
	return all(isequal(a, b));
}

bool Spectrum_IsBlack(const float3 a) {
	return Spectrum_IsEqual(a, BLACK);
}

float Spectrum_Filter(const float3 s)  {
	return fmax(s.s0, fmax(s.s1, s.s2));
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
