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

#ifndef _LUXRAYS_TRANSFORM_H
#define _LUXRAYS_TRANSFORM_H

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/ray.h"
#include "luxrays/core/geometry/bbox.h"
#include "luxrays/core/geometry/matrix4x4.h"
#include "luxrays/core/geometry/matrix4x4op.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/transform_types.cl"
}

class Transform;

class InvTransform {
public:
	const Transform &ref;
protected:
	InvTransform(const Transform &t) : ref(t) { }
	friend InvTransform Inverse(const Transform &t);
};

// Transform Declarations
class Transform {
public:
	// Transform Public Methods
	Transform() : m(Matrix4x4::MAT_IDENTITY), mInv(Matrix4x4::MAT_IDENTITY) { }

	explicit Transform(float mat[4][4]) : m(mat) { mInv = m.Inverse(); }

	explicit Transform(const Matrix4x4 &mat) : m(mat) { mInv = m.Inverse(); }

	Transform(const Matrix4x4 &mat, const Matrix4x4 &minv) :
		m(mat), mInv(minv) { }

	Transform(const InvTransform &t) : m(t.ref.mInv), mInv(t.ref.m) { }

	friend std::ostream &operator<<(std::ostream &, const Transform &);

	Matrix4x4 GetMatrix() const { return m; }
	bool HasScale() const;
	bool SwapsHandedness() const;
	Transform operator*(const Transform &t2) const {
		return Transform(m * t2.m, t2.mInv * mInv);
	}
	Transform operator/(const Transform &t2) const {
		return Transform(m * t2.mInv, t2.m * mInv);
	}

	// Transform Data kept public so that transforms of new objects are
	// easily added
	Matrix4x4 m, mInv;
};

inline InvTransform Inverse(const Transform &t)
{
	return InvTransform(t);
}

inline const Transform &Inverse(const InvTransform &t)
{
	return t.ref;
}

inline bool Transform::SwapsHandedness() const
{
	const float det = ((m.m[0][0] *
		(m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])) -
		(m.m[0][1] *
		(m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])) +
		(m.m[0][2] *
		(m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0])));
	return det < 0.f;
}

inline Point operator*(const Transform &t, const Point &pt)
{
	const float x = pt.x, y = pt.y, z = pt.z;
	const Matrix4x4 &m = t.m;
	const Point pr(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3],
		m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3],
		m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3]);
	const float w = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
	if (w != 1.f)
		return pr / w;
	else
		return pr;
}

inline Point &operator*=(Point &pt, const Transform &t)
{
	const float x = pt.x, y = pt.y, z = pt.z;
	const Matrix4x4 &m = t.m;
	pt.x = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3];
	pt.y = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3];
	pt.z = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3];
	const float w = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
	if (w != 1.f)
		pt /= w;
	return pt;
}

inline Point operator*(const InvTransform &t, const Point &pt)
{
	const float x = pt.x, y = pt.y, z = pt.z;
	const Matrix4x4 &m = t.ref.mInv;
	const Point pr(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3],
		m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3],
		m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3]);
	const float w = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
	if (w != 1.f)
		return pr / w;
	else
		return pr;
}

inline Point &operator*=(Point &pt, const InvTransform &t)
{
	const float x = pt.x, y = pt.y, z = pt.z;
	const Matrix4x4 &m = t.ref.mInv;
	pt.x = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3];
	pt.y = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3];
	pt.z = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3];
	const float w = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
	if (w != 1.f)
		pt /= w;
	return pt;
}

inline Vector operator*(const Transform &t, const Vector &v)
{
	const float x = v.x, y = v.y, z = v.z;
	const Matrix4x4 &m = t.m;
	return Vector(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z,
		m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z,
		m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z);
}

inline Vector &operator*=(Vector &v, const Transform &t)
{
	const float x = v.x, y = v.y, z = v.z;
	const Matrix4x4 &m = t.m;
	v.x = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z;
	v.y = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z;
	v.z = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z;
	return v;
}

inline Vector operator*(const InvTransform &t, const Vector &v)
{
	const float x = v.x, y = v.y, z = v.z;
	const Matrix4x4 &m = t.ref.mInv;
	return Vector(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z,
		m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z,
		m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z);
}

inline Vector &operator*=(Vector &v, const InvTransform &t)
{
	const float x = v.x, y = v.y, z = v.z;
	const Matrix4x4 &m = t.ref.mInv;
	v.x = m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z;
	v.y = m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z;
	v.z = m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z;
	return v;
}

inline Normal operator*(const Transform &t, const Normal &n)
{
	const float x = n.x, y = n.y, z = n.z;
	const Matrix4x4 &mInv = t.mInv;
	return Normal(mInv.m[0][0] * x + mInv.m[1][0] * y + mInv.m[2][0] * z,
		mInv.m[0][1] * x + mInv.m[1][1] * y + mInv.m[2][1] * z,
		mInv.m[0][2] * x + mInv.m[1][2] * y + mInv.m[2][2] * z);
}

inline Normal &operator*=(Normal &n, const Transform &t)
{
	const float x = n.x, y = n.y, z = n.z;
	const Matrix4x4 &mInv = t.mInv;
	n.x = mInv.m[0][0] * x + mInv.m[1][0] * y + mInv.m[2][0] * z;
	n.y = mInv.m[0][1] * x + mInv.m[1][1] * y + mInv.m[2][1] * z;
	n.z = mInv.m[0][2] * x + mInv.m[1][2] * y + mInv.m[2][2] * z;
	return n;
}

inline Normal operator*(const InvTransform &t, const Normal &n)
{
	const float x = n.x, y = n.y, z = n.z;
	const Matrix4x4 &mInv = t.ref.m;
	return Normal(mInv.m[0][0] * x + mInv.m[1][0] * y + mInv.m[2][0] * z,
		mInv.m[0][1] * x + mInv.m[1][1] * y + mInv.m[2][1] * z,
		mInv.m[0][2] * x + mInv.m[1][2] * y + mInv.m[2][2] * z);
}

inline Normal &operator*=(Normal &n, const InvTransform &t)
{
	const float x = n.x, y = n.y, z = n.z;
	const Matrix4x4 &mInv = t.ref.m;
	n.x = mInv.m[0][0] * x + mInv.m[1][0] * y + mInv.m[2][0] * z;
	n.y = mInv.m[0][1] * x + mInv.m[1][1] * y + mInv.m[2][1] * z;
	n.z = mInv.m[0][2] * x + mInv.m[1][2] * y + mInv.m[2][2] * z;
	return n;
}

inline Ray operator*(const Transform &t, const Ray &r)
{
	return Ray(t * r.o, t * r.d, r.mint, r.maxt, r.time);
}

inline Ray &operator*=(Ray &r, const Transform &t)
{
	r.o *= t;
	r.d *= t;
	return r;
}

inline Ray operator*(const InvTransform &t, const Ray &r)
{
	return Ray(t * r.o, t * r.d, r.mint, r.maxt, r.time);
}

inline Ray &operator*=(Ray &r, const InvTransform &t)
{
	r.o *= t;
	r.d *= t;
	return r;
}

inline BBox operator*(const Transform &t, const BBox &b)
{
	return Union(Union(Union(Union(Union(Union(BBox(t * b.pMin, t * b.pMax),
		t * Point(b.pMax.x, b.pMin.y, b.pMin.z)),
		t * Point(b.pMin.x, b.pMax.y, b.pMin.z)),
		t * Point(b.pMin.x, b.pMin.y, b.pMax.z)),
		t * Point(b.pMax.x, b.pMax.y, b.pMin.z)),
		t * Point(b.pMax.x, b.pMin.y, b.pMax.z)),
		t * Point(b.pMin.x, b.pMax.y, b.pMax.z));
}

inline BBox operator*(const InvTransform &t, const BBox &b)
{
	return Union(Union(Union(Union(Union(Union(BBox(t * b.pMin, t * b.pMax),
		t * Point(b.pMax.x, b.pMin.y, b.pMin.z)),
		t * Point(b.pMin.x, b.pMax.y, b.pMin.z)),
		t * Point(b.pMin.x, b.pMin.y, b.pMax.z)),
		t * Point(b.pMax.x, b.pMax.y, b.pMin.z)),
		t * Point(b.pMax.x, b.pMin.y, b.pMax.z)),
		t * Point(b.pMin.x, b.pMax.y, b.pMax.z));
}

Transform Translate(const Vector &delta);
Transform Scale(float x, float y, float z);
Transform RotateX(float angle);
Transform RotateY(float angle);
Transform RotateZ(float angle);
Transform Rotate(float angle, const Vector &axis);
Transform LookAt(const Point &pos, const Point &look, const Vector &up);
Transform Orthographic(float znear, float zfar);
Transform Perspective(float fov, float znear, float zfar);
void TransformAccordingNormal(const Normal &nn, const Vector &woL, Vector *woW);

}

#endif // _LUXRAYS_TRANSFORM_H
