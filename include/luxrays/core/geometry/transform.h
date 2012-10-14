/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#ifndef _LUXRAYS_TRANSFORM_H
#define _LUXRAYS_TRANSFORM_H

#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/ray.h"
#include "luxrays/core/geometry/bbox.h"
#include "luxrays/core/geometry/matrix4x4.h"

namespace luxrays {

// Transform Declarations
class Transform {
public:
	// Transform Public Methods
	Transform() : m(MAT_IDENTITY), mInv(MAT_IDENTITY) { }

	Transform(float mat[4][4]) : m(mat) { mInv = m.Inverse(); }

	Transform(const Matrix4x4 &mat) : m(mat) { mInv = m.Inverse(); }

	Transform(const Matrix4x4 &mat, const Matrix4x4 &minv) :
		m(mat), mInv(minv) { }

	friend std::ostream &operator<<(std::ostream &, const Transform &);

	Transform GetInverse() const { return Transform(mInv, m); }

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

private:
	// Transform private static data
	static const Matrix4x4 MAT_IDENTITY;
};

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

inline Point operator/(const Transform &t, const Point &pt)
{
	const float x = pt.x, y = pt.y, z = pt.z;
	const Matrix4x4 &m = t.mInv;
	const Point pr(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z + m.m[0][3],
		m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z + m.m[1][3],
		m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z + m.m[2][3]);
	const float w = m.m[3][0] * x + m.m[3][1] * y + m.m[3][2] * z + m.m[3][3];
	if (w != 1.f)
		return pr / w;
	else
		return pr;
}

inline Vector operator*(const Transform &t, const Vector &v)
{
	const float x = v.x, y = v.y, z = v.z;
	const Matrix4x4 &m = t.m;
	return Vector(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z,
			m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z,
			m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z);
}

inline Vector operator/(const Transform &t, const Vector &v)
{
	const float x = v.x, y = v.y, z = v.z;
	const Matrix4x4 &m = t.mInv;
	return Vector(m.m[0][0] * x + m.m[0][1] * y + m.m[0][2] * z,
			m.m[1][0] * x + m.m[1][1] * y + m.m[1][2] * z,
			m.m[2][0] * x + m.m[2][1] * y + m.m[2][2] * z);
}

inline Normal operator*(const Transform &t, const Normal &n)
{
	const float x = n.x, y = n.y, z = n.z;
	const Matrix4x4 &mInv = t.mInv;
	return Normal(mInv.m[0][0] * x + mInv.m[1][0] * y + mInv.m[2][0] * z,
			mInv.m[0][1] * x + mInv.m[1][1] * y + mInv.m[2][1] * z,
			mInv.m[0][2] * x + mInv.m[1][2] * y + mInv.m[2][2] * z);
}

inline Normal operator/(const Transform &t, const Normal &n)
{
	const float x = n.x, y = n.y, z = n.z;
	const Matrix4x4 &mInv = t.m;
	return Normal(mInv.m[0][0] * x + mInv.m[1][0] * y + mInv.m[2][0] * z,
			mInv.m[0][1] * x + mInv.m[1][1] * y + mInv.m[2][1] * z,
			mInv.m[0][2] * x + mInv.m[1][2] * y + mInv.m[2][2] * z);
}

inline Ray operator*(const Transform &t, const Ray &r)
{
	return Ray(t * r.o, t * r.d, r.mint, r.maxt, r.time);
}

inline Ray operator/(const Transform &t, const Ray &r)
{
	return Ray(t / r.o, t / r.d, r.mint, r.maxt, r.time);
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

inline BBox operator/(const Transform &t, const BBox &b)
{
	return Union(Union(Union(Union(Union(Union(BBox(t / b.pMin, t / b.pMax),
		t / Point(b.pMax.x, b.pMin.y, b.pMin.z)),
		t / Point(b.pMin.x, b.pMax.y, b.pMin.z)),
		t / Point(b.pMin.x, b.pMin.y, b.pMax.z)),
		t / Point(b.pMax.x, b.pMax.y, b.pMin.z)),
		t / Point(b.pMax.x, b.pMin.y, b.pMax.z)),
		t / Point(b.pMin.x, b.pMax.y, b.pMax.z));
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

#endif // LUX_TRANSFORM_H
