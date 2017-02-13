/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_QUATERNION_H
#define _LUXRAYS_QUATERNION_H

#include "luxrays/core/geometry/vector.h"
using luxrays::Vector;

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/quaternion_types.cl"
}

class Quaternion {

public:
	float w;
	Vector v;

	// generate quaternion from 4x4 matrix
	Quaternion(const Matrix4x4 &m);
	Quaternion() : w(1.f), v(0.f) { }
	Quaternion(const Quaternion &q) : w(q.w), v(q.v) { }
	Quaternion(float _w, const Vector &_v) : w(_w), v(_v) { }

	Quaternion Invert() const {
		return Quaternion(w, -v);
	}

	Vector RotateVector(const Vector &v) const;

	// get the rotation matrix from quaternion
	void ToMatrix(float m[4][4]) const;
};

inline Quaternion operator +(const Quaternion& q1, const Quaternion& q2) {
	return Quaternion(q1.w + q2.w, q1.v + q2.v);
}

inline Quaternion operator -(const Quaternion& q1, const Quaternion& q2) {
	return Quaternion(q1.w - q2.w, q1.v - q2.v);
}

inline Quaternion operator *(float f, const Quaternion& q) {
	return Quaternion(q.w * f, q.v * f);
}

inline Quaternion operator *( const Quaternion& q1, const Quaternion& q2 ) {
	return Quaternion(
		q1.w*q2.w - Dot(q1.v, q2.v),
		q1.w*q2.v + q2.w*q1.v + Cross(q1.v, q2.v));
}

inline float Dot(const Quaternion &q1, const Quaternion &q2) {
	return q1.w * q2.w + Dot(q1.v, q2.v);
}

inline Quaternion Normalize(const Quaternion &q) {
	return (1.f / sqrtf(Dot(q, q))) * q;
}

Quaternion Slerp(float t, const Quaternion &q1, const Quaternion &q2);
Quaternion GetRotationBetween(const Vector &u, const Vector &v);

}

#endif // _LUXRAYS_QUATERNION_H
