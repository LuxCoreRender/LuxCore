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

#ifndef _LUXRAYS_VECTOR_H
#define _LUXRAYS_VECTOR_H

#include <cmath>
#include <iostream>
#include <functional>
#include <limits>
#include <algorithm>
#include <boost/serialization/access.hpp>

#include "luxrays/core/utils.h"

namespace luxrays {

class Point;
class Normal;

class Vector {
	friend class boost::serialization::access;
public:
	// Vector Public Methods

	Vector(float _x = 0.f, float _y = 0.f, float _z = 0.f) :
		x(_x), y(_y), z(_z) {
	}
	explicit Vector(const Point &p);

	Vector operator+(const Vector &v) const {
		return Vector(x + v.x, y + v.y, z + v.z);
	}

	Vector & operator+=(const Vector &v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	Vector operator-(const Vector &v) const {
		return Vector(x - v.x, y - v.y, z - v.z);
	}

	Vector & operator-=(const Vector &v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	bool operator==(const Vector &v) const {
		return x == v.x && y == v.y && z == v.z;
	}

	Vector operator*(float f) const {
		return Vector(f*x, f*y, f * z);
	}

	Vector & operator*=(float f) {
		x *= f;
		y *= f;
		z *= f;
		return *this;
	}

	Vector operator/(float f) const {
		float inv = 1.f / f;
		return Vector(x * inv, y * inv, z * inv);
	}

	Vector & operator/=(float f) {
		float inv = 1.f / f;
		x *= inv;
		y *= inv;
		z *= inv;
		return *this;
	}

	Vector operator-() const {
		return Vector(-x, -y, -z);
	}

	float operator[](int i) const {
		return (&x)[i];
	}

	float &operator[](int i) {
		return (&x)[i];
	}

	float LengthSquared() const {
		return x * x + y * y + z*z;
	}

	float Length() const {
		return sqrtf(LengthSquared());
	}
	explicit Vector(const Normal &n);

	// Vector Public Data
	float x, y, z;
#define _LUXRAYS_VECTOR_OCLDEFINE "typedef struct { float x, y, z; } Vector;\n"

private:
	template<class Archive>
			void serialize(Archive & ar, const unsigned int version)
			{
				ar & x;
				ar & y;
				ar & z;
			}
};

inline std::ostream &operator<<(std::ostream &os, const Vector &v) {
	os << "Vector[" << v.x << ", " << v.y << ", " << v.z << "]";
	return os;
}

inline Vector operator*(float f, const Vector &v) {
	return v*f;
}

inline float Dot(const Vector &v1, const Vector &v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

inline float AbsDot(const Vector &v1, const Vector &v2) {
	return fabsf(Dot(v1, v2));
}

inline Vector Cross(const Vector &v1, const Vector &v2) {
	return Vector((v1.y * v2.z) - (v1.z * v2.y),
			(v1.z * v2.x) - (v1.x * v2.z),
			(v1.x * v2.y) - (v1.y * v2.x));
}

inline Vector Normalize(const Vector &v) {
	return v / v.Length();
}

inline void CoordinateSystem(const Vector &v1, Vector *v2, Vector *v3) {
	if (fabsf(v1.x) > fabsf(v1.y)) {
		float invLen = 1.f / sqrtf(v1.x * v1.x + v1.z * v1.z);
		*v2 = Vector(-v1.z * invLen, 0.f, v1.x * invLen);
	} else {
		float invLen = 1.f / sqrtf(v1.y * v1.y + v1.z * v1.z);
		*v2 = Vector(0.f, v1.z * invLen, -v1.y * invLen);
	}
	*v3 = Cross(v1, *v2);
}

inline Vector SphericalDirection(float sintheta, float costheta, float phi) {
	return Vector(sintheta * cosf(phi), sintheta * sinf(phi), costheta);
}

inline Vector SphericalDirection(float sintheta, float costheta, float phi,
	const Vector &x, const Vector &y, const Vector &z) {
	return sintheta * cosf(phi) * x + sintheta * sinf(phi) * y +
		costheta * z;
}

inline float SphericalTheta(const Vector &v) {
	return acosf(Clamp(v.z, -1.f, 1.f));
}

inline float SphericalPhi(const Vector &v) {
	float p = atan2f(v.y, v.x);
	return (p < 0.f) ? p + 2.f * M_PI : p;
}

inline float CosTheta(const Vector &w) {
	return w.z;
}

inline float SinTheta2(const Vector &w) {
	return std::max(0.f, 1.f - CosTheta(w) * CosTheta(w));
}

inline float SinTheta(const Vector &w) {
	return sqrtf(SinTheta2(w));
}

inline float CosPhi(const Vector &w) {
	const float sinTheta = SinTheta(w);
	return sinTheta > 0.f ? Clamp(w.x / sinTheta, -1.f, 1.f) : 1.f;
}

inline float SinPhi(const Vector &w) {
	const float sinTheta = SinTheta(w);
	return sinTheta > 0.f ? Clamp(w.y / sinTheta, -1.f, 1.f) : 0.f;
}

inline bool SameHemisphere(const Vector &w, const Vector &wp) {
	return w.z * wp.z > 0.f;
}

}

#ifdef _LUXRAYS_NORMAL_H
#include "luxrays/core/geometry/vector_normal.h"
#endif

#endif	/* _LUXRAYS_VECTOR_H */
