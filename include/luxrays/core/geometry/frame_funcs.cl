#line 2 "frame_funcs.cl"

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

	VSTORE3F(X, &frame->X.x);
	VSTORE3F(Y, &frame->Y.x);
	VSTORE3F(Z, &frame->Z.x);
}

float3 ToWorld(const float3 X, const float3 Y, const float3 Z, const float3 v) {
	return X * v.x + Y * v.y + Z * v.z;
}

float3 Frame_ToWorld(__global Frame *frame, const float3 v) {
	return ToWorld(VLOAD3F(&frame->X.x), VLOAD3F(&frame->Y.x), VLOAD3F(&frame->Z.x), v);
}

float3 ToLocal(const float3 X, const float3 Y, const float3 Z, const float3 a) {
	return (float3)(dot(a, X), dot(a, Y), dot(a, Z));
}

float3 Frame_ToLocal(__global Frame *frame, const float3 v) {
	return ToLocal(VLOAD3F(&frame->X.x), VLOAD3F(&frame->Y.x), VLOAD3F(&frame->Z.x), v);
}
