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

#include <cstring>
using std::memset;

#include "luxrays/core/geometry/quaternion.h"
#include "luxrays/core/geometry/matrix4x4.h"
using luxrays::Matrix4x4;


namespace luxrays {

void orthoNormalize(float m[4][4])
{
	float len, temp[3][3];
	for (u_int i = 0; i < 3; ++i) {
		for (u_int j = 0; j < 3; ++j)
			temp[i][j] = m[i][j];
	}

	// normalize x
	len = sqrtf(temp[0][0] * temp[0][0] + temp[0][1] * temp[0][1] + temp[0][2] * temp[0][2]);
	len = (len == 0.f) ? 1.f : 1.f / len;
	temp[0][0] *= len; temp[0][1] *= len; temp[0][2] *= len;

	// z = x cross y
	temp[2][0] = (temp[0][1] * temp[1][2] - temp[0][2] * temp[1][1]);
	temp[2][1] = (temp[0][2] * temp[1][0] - temp[0][0] * temp[1][2]);
	temp[2][2] = (temp[0][0] * temp[1][1] - temp[0][1] * temp[1][0]);

	// normalize z
	len = sqrtf(temp[2][0] * temp[2][0] + temp[2][1] * temp[2][1] + temp[2][2] * temp[2][2]);
	len = (len == 0.f) ? 1.f : 1.f / len;
	temp[2][0] *= len; temp[2][1] *= len; temp[2][2] *= len;

	// y = z cross x
	temp[1][0] = (temp[2][1] * temp[0][2] - temp[2][2] * temp[0][1]);
	temp[1][1] = (temp[2][2] * temp[0][0] - temp[2][0] * temp[0][2]);
	temp[1][2] = (temp[2][0] * temp[0][1] - temp[2][1] * temp[0][0]);

	// normalize y
	len = sqrtf(temp[1][0] * temp[1][0] + temp[1][1] * temp[1][1] + temp[1][2] * temp[1][2]);
	len = (len == 0.f) ? 1.f : 1.f / len;
	temp[1][0] *= len; temp[1][1] *= len; temp[1][2] *= len;

	// update matrix
	m[0][0] = temp[0][0]; m[0][1] = temp[0][1]; m[0][2] = temp[0][2];
	m[1][0] = temp[1][0]; m[1][1] = temp[1][1]; m[1][2] = temp[1][2];
	m[2][0] = temp[2][0]; m[2][1] = temp[2][1]; m[2][2] = temp[2][2];
}

// construct a unit quaternion from a rotation matrix
Quaternion::Quaternion(const Matrix4x4 &m) {
	float ortho[4][4];
	memcpy(ortho, m.m, sizeof(float) * 16);
	orthoNormalize(ortho);

	const float trace = ortho[0][0] + ortho[1][1] + ortho[2][2] + 1.f;

	if (trace > 1e-6f) {
		const float s = sqrtf(trace) * 2.f;
		v.x = (ortho[1][2] - ortho[2][1]) / s;
		v.y = (ortho[2][0] - ortho[0][2]) / s;
		v.z = (ortho[0][1] - ortho[1][0]) / s;
		w = 0.25f * s;
	} else {
		if ( ortho[0][0] > ortho[1][1] && ortho[0][0] > ortho[2][2] )  {
			// Column 0: 
			const float s  = sqrtf(1.f + ortho[0][0] - ortho[1][1] - ortho[2][2]) * 2.f;
			v.x = 0.25f * s;
			v.y = (ortho[0][1] + ortho[1][0] ) / s;
			v.z = (ortho[2][0] + ortho[0][2] ) / s;
			w = (ortho[1][2] - ortho[2][1] ) / s;
		} else if ( ortho[1][1] > ortho[2][2] ) {
			// Column 1: 
			const float s  = sqrtf(1.f + ortho[1][1] - ortho[0][0] - ortho[2][2]) * 2.f;
			v.x = (ortho[0][1] + ortho[1][0] ) / s;
			v.y = 0.25f * s;
			v.z = (ortho[1][2] + ortho[2][1] ) / s;
			w = (ortho[2][0] - ortho[0][2] ) / s;
		} else {
			// Column 2:
			const float s  = sqrtf(1.f + ortho[2][2] - ortho[0][0] - ortho[1][1]) * 2.f;
			v.x = (ortho[2][0] + ortho[0][2] ) / s;
			v.y = (ortho[1][2] + ortho[2][1] ) / s;
			v.z = 0.25f * s;
			w = (ortho[0][1] - ortho[1][0] ) / s;
		}
	}
}

Vector Quaternion::RotateVector(const Vector &v) const {
	const Quaternion vectorQuaternion(0.f, v);
	const Quaternion rotatedQuaternion = (*this) * (vectorQuaternion * Invert());

	return rotatedQuaternion.v;
}

// get the rotation matrix from quaternion
void Quaternion::ToMatrix(float m[4][4]) const {
	const float xx = v.x * v.x;
	const float yy = v.y * v.y;
	const float zz = v.z * v.z;
	const float xy = v.x * v.y;
	const float xz = v.x * v.z;
	const float yz = v.y * v.z;
	const float xw = v.x * w;
	const float yw = v.y * w;
	const float zw = v.z * w;

	m[0][0] = 1.f - 2.f * (yy + zz);
	m[1][0] = 2.f * (xy - zw);
	m[2][0] = 2.f * (xz + yw);
	m[0][1] = 2.f * (xy + zw);
	m[1][1] = 1.f - 2.f * (xx + zz);
	m[2][1] = 2.f * (yz - xw);
	m[0][2] = 2.f * (xz - yw);
	m[1][2] = 2.f * (yz + xw);
	m[2][2] = 1.f - 2.f * (xx + yy);

	// complete matrix
	m[0][3] = m[1][3] = m[2][3] = 0.f;
	m[3][0] = m[3][1] = m[3][2] = 0.f;
	m[3][3] = 1.f;
}

Quaternion Slerp(float t, const Quaternion &q1, const Quaternion &q2) {

	float cos_phi = Dot(q1, q2);
	const float sign = (cos_phi > 0.f) ? 1.f : -1.f;
	
	cos_phi *= sign;

	float f1, f2;
	if (1.f - cos_phi > 1e-6f) {	
		const float phi = acosf(cos_phi);
		const float sin_phi = sinf(phi);
		f1 = sinf((1.f - t) * phi) / sin_phi;
		f2 = sinf(t * phi) / sin_phi;
	} else {
		// start and end are very close
		// perform linear interpolation
		f1 = 1.f - t;
		f2 = t;
	}

	return f1 * q1 + (sign * f2) * q2;
} 

// Note: u and v must be normalized.
Quaternion GetRotationBetween(const Vector &u, const Vector &v) {
  if (u == -v) {
    // 180 degree rotation around any axis (y-axis used here)
    return Quaternion(0.f, Vector(0.f, 1.f, 0.f));
  }

  const Vector half = Normalize(u + v);
  return Quaternion(Dot(u, half), Cross(u, half));
}

}
