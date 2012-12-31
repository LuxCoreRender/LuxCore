#line 2 "randomgen_funcs.cl"

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

void Frame_SetFromZ(__global Frame *frame, const float3 z) {
	const float3 Z = normalize(z);
	const float3 tmpZ = Z;
	const float3 tmpX = (fabs(tmpZ.x) > 0.99f) ? (float3)(0.f, 1.f, 0.f) : (float3)(1.f, 0.f, 0.f);
	const float3 Y = normalize(cross(tmpZ, tmpX));
	const float3 X = cross(Y, tmpZ);

	vstore3(X, 0, &frame->X.x);
	vstore3(Y, 0, &frame->Y.x);
	vstore3(Z, 0, &frame->Z.x);
}

float3 ToWorld(const float3 X, const float3 Y, const float3 Z, const float3 a) {
	return X * a.x + Y * a.y + Z * a.z;
}

float3 Frame_ToWorld(__global Frame *frame, const float3 a) {
	return ToWorld(vload3(0, &frame->X.x), vload3(0, &frame->Y.x), vload3(0, &frame->Z.x), a);
}

float3 ToLocal(const float3 X, const float3 Y, const float3 Z, const float3 a) {
	return (float3)(dot(a, X), dot(a, Y), dot(a, Z));
}

float3 Frame_ToLocal(__global Frame *frame, const float3 a) {
	return ToLocal(vload3(0, &frame->X.x), vload3(0, &frame->Y.x), vload3(0, &frame->Z.x), a);
}
