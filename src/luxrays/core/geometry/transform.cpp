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

#include <ostream>

#include "luxrays/core/geometry/vector_normal.h"
#include "luxrays/core/geometry/transform.h"

namespace luxrays {

// Transform Method Definitions

std::ostream & operator<<(std::ostream &os, const Transform &t) {
	t.m.Print(os);
	return os;
}

Transform Translate(const Vector &delta) {
	Matrix4x4 m(1, 0, 0, delta.x,
			0, 1, 0, delta.y,
			0, 0, 1, delta.z,
			0, 0, 0, 1);
	Matrix4x4 minv(1, 0, 0, -delta.x,
			0, 1, 0, -delta.y,
			0, 0, 1, -delta.z,
			0, 0, 0, 1);
	return Transform(m, minv);
}

Transform Scale(float x, float y, float z) {
	Matrix4x4 m(x, 0, 0, 0,
			0, y, 0, 0,
			0, 0, z, 0,
			0, 0, 0, 1);
	Matrix4x4 minv(1.f / x, 0, 0, 0,
			0, 1.f / y, 0, 0,
			0, 0, 1.f / z, 0,
			0, 0, 0, 1);
	return Transform(m, minv);
}

Transform RotateX(float angle) {
	const float sin_t = sinf(Radians(angle));
	const float cos_t = cosf(Radians(angle));
	Matrix4x4 m(1, 0, 0, 0,
			0, cos_t, -sin_t, 0,
			0, sin_t, cos_t, 0,
			0, 0, 0, 1);
	return Transform(m, m.Transpose());
}

Transform RotateY(float angle) {
	const float sin_t = sinf(Radians(angle));
	const float cos_t = cosf(Radians(angle));
	Matrix4x4 m(cos_t, 0, sin_t, 0,
			0, 1, 0, 0,
			-sin_t, 0, cos_t, 0,
			0, 0, 0, 1);
	return Transform(m, m.Transpose());
}

Transform RotateZ(float angle) {
	const float sin_t = sinf(Radians(angle));
	const float cos_t = cosf(Radians(angle));
	Matrix4x4 m(cos_t, -sin_t, 0, 0,
			sin_t, cos_t, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);
	return Transform(m, m.Transpose());
}

Transform Rotate(float angle, const Vector &axis) {
	const Vector a = Normalize(axis);
	const float s = sinf(Radians(angle));
	const float c = cosf(Radians(angle));
	float m[4][4];

	m[0][0] = a.x * a.x + (1.f - a.x * a.x) * c;
	m[0][1] = a.x * a.y * (1.f - c) - a.z * s;
	m[0][2] = a.x * a.z * (1.f - c) + a.y * s;
	m[0][3] = 0;

	m[1][0] = a.x * a.y * (1.f - c) + a.z * s;
	m[1][1] = a.y * a.y + (1.f - a.y * a.y) * c;
	m[1][2] = a.y * a.z * (1.f - c) - a.x * s;
	m[1][3] = 0;

	m[2][0] = a.x * a.z * (1.f - c) - a.y * s;
	m[2][1] = a.y * a.z * (1.f - c) + a.x * s;
	m[2][2] = a.z * a.z + (1.f - a.z * a.z) * c;
	m[2][3] = 0;

	m[3][0] = 0;
	m[3][1] = 0;
	m[3][2] = 0;
	m[3][3] = 1;

	const Matrix4x4 o(m);
	return Transform(o, o.Transpose());
}

Transform LookAt(const Point &pos, const Point &look, const Vector &up) {
	float m[4][4];
	// Initialize fourth column of viewing matrix
	m[0][3] = pos.x;
	m[1][3] = pos.y;
	m[2][3] = pos.z;
	m[3][3] = 1;
	const Vector dir = Normalize(look - pos);
	const Vector right = Normalize(Cross(dir, up));
	const Vector newUp = Cross(right, dir);
	m[0][0] = right.x;
	m[1][0] = right.y;
	m[2][0] = right.z;
	m[3][0] = 0.;
	m[0][1] = newUp.x;
	m[1][1] = newUp.y;
	m[2][1] = newUp.z;
	m[3][1] = 0.;
	m[0][2] = dir.x;
	m[1][2] = dir.y;
	m[2][2] = dir.z;
	m[3][2] = 0.;
	const Matrix4x4 camToWorld(m);
	return Transform(camToWorld.Inverse(), camToWorld);
}

bool Transform::HasScale() const {
	//#if 0
	const float det = fabsf(m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])) -
			(m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])) +
			(m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]));
	return (det < .999f || det > 1.001f);
	/*#endif
		return false;*/
}

void TransformAccordingNormal(const Normal &nn, const Vector &woL, Vector *woW) {
	Vector sn, tn;
	const float zz = sqrtf(1.f - nn.z * nn.z);
	sn.z = 0.f;
	if (fabsf(zz) < 1e-6f) {
		sn.x = 1.f;
		sn.y = 0.f;
	} else {
		sn.x = nn.y / zz;
		sn.y = -nn.x / zz;
	}
	tn = Cross(nn, sn);
	*woW = woL.x * sn + woL.y * tn + woL.z * Vector(nn);
}

Transform Orthographic(float znear, float zfar) {
	return Scale(1.f, 1.f, 1.f / (zfar - znear)) *
			Translate(Vector(0.f, 0.f, -znear));
}

Transform Perspective(float fov, float n, float f) {
	// Perform projective divide
	const float inv_denom = 1.f / (1.f - n / f);
	const Matrix4x4 persp(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, inv_denom, -n * inv_denom,
			0, 0, 1, 0);
	// Scale to canonical viewing volume
	const float invTanAng = 1.f / tanf(Radians(fov) / 2.f);
	return Scale(invTanAng, invTanAng, 1) * Transform(persp, persp.Inverse());
}

}
