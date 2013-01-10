#line 2 "vector_funcs.cl"

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

float SphericalTheta(const float3 v) {
	return acos(clamp(v.z, -1.f, 1.f));
}

float SphericalPhi(const float3 v) {
	float p = atan2(v.y, v.x);
	return (p < 0.f) ? p + 2.f * M_PI_F : p;
}

void CoordinateSystem(const float3 v1, float3 *v2, float3 *v3) {
	if (fabs(v1.x) > fabs(v1.y)) {
		float invLen = 1.f / sqrt(v1.x * v1.x + v1.z * v1.z);
		(*v2).x = -v1.z * invLen;
		(*v2).y = 0.f;
		(*v2).z = v1.x * invLen;
	} else {
		float invLen = 1.f / sqrt(v1.y * v1.y + v1.z * v1.z);
		(*v2).x = 0.f;
		(*v2).y = v1.z * invLen;
		(*v2).z = -v1.y * invLen;
	}

	*v3 = cross(v1, *v2);
}
