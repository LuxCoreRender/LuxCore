/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_MATRIX4x4OP_H
#define _LUXRAYS_MATRIX4x4OP_H

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/ray.h"
#include "luxrays/core/geometry/bbox.h"
#include "luxrays/core/geometry/matrix4x4.h"

namespace luxrays {

inline Point operator*(const Matrix4x4 &m, const Point &pt) {
	const float x = pt.x, y = pt.y, z = pt.z;
	const Point pr(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3],
			m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3],
			m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3]);
	const float w = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
	if (w != 1.f)
		return pr / w;
	else
		return pr;
}

inline Point &operator*=(Point &pt, const Matrix4x4 &m) {
	const float x = pt.x, y = pt.y, z = pt.z;
	pt.x = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3];
	pt.y = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3];
	pt.z = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3];
	const float w = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
	if (w != 1.f)
		pt /= w;
	return pt;
}

inline Vector operator*(const Matrix4x4 &m, const Vector &v) {
	const float x = v.x, y = v.y, z = v.z;
	return Vector(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z,
			m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z,
			m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z);
}

inline Vector &operator*=(Vector &v, const Matrix4x4 &m) {
	const float x = v.x, y = v.y, z = v.z;
	v.x = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z;
	v.y = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z;
	v.z = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z;
	return v;
}

inline Normal operator*(const Matrix4x4 &m, const Normal &n) {
	const float x = n.x, y = n.y, z = n.z;
	return Normal(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z,
			m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z,
			m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z);
}

inline Normal &operator*=(Normal &n, const Matrix4x4 &m) {
	const float x = n.x, y = n.y, z = n.z;
	n.x = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z;
	n.y = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z;
	n.z = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z;
	return n;
}

inline Ray operator*(const Matrix4x4 &m, const Ray &r) {
	return Ray(m * r.o, m * r.d, r.mint, r.maxt, r.time);
}

inline Ray &operator*=(Ray &r, const Matrix4x4 &m) {
	r.o *= m;
	r.d *= m;
	return r;
}

inline BBox operator*(const Matrix4x4 &m, const BBox &b) {
	return Union(Union(Union(Union(Union(Union(BBox(m * b.pMin, m * b.pMax),
			m * Point(b.pMax.x, b.pMin.y, b.pMin.z)),
			m * Point(b.pMin.x, b.pMax.y, b.pMin.z)),
			m * Point(b.pMin.x, b.pMin.y, b.pMax.z)),
			m * Point(b.pMax.x, b.pMax.y, b.pMin.z)),
			m * Point(b.pMax.x, b.pMin.y, b.pMax.z)),
			m * Point(b.pMin.x, b.pMax.y, b.pMax.z));
}

}

#endif // _LUXRAYS_MATRIX4x4OP_H
