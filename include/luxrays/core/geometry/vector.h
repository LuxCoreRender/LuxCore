/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_VECTOR_H
#define _LUXRAYS_VECTOR_H

#include <cmath>
#include <iostream>
#include <functional>
#include <limits>
#include <algorithm>

#include "luxrays/utils/utils.h"
#include "luxrays/utils/serializationutils.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/vector_types.cl"
}

class Point;
class Normal;

class Vector {
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
		return x * x + y * y + z * z;
	}

	float Length() const {
		return sqrtf(LengthSquared());
	}
	explicit Vector(const Normal &n);

	bool IsNaN() const {
		return isnan(x) || isnan(y) || isnan(z);
	}

	bool IsInf() const {
		return isinf(x) || isinf(y) || isinf(z);
	}

	friend class boost::serialization::access;

	// Vector Public Data
	float x, y, z;

private:
	template<class Archive> void serialize(Archive &ar, const unsigned int version) {
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
	float len = sqrtf (v1.y * v1.y + v1.z * v1.z);
	if (len < 1e-5) { // it's pretty-much along x-axis
		*v2 = Vector (0.f, 0.f, 1.f);
	} else {
		*v2 = Vector(0.f, v1.z / len, -v1.y / len);
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
	const float p = atan2f(v.y, v.x);
	return (p < 0.f) ? p + 2.f * M_PI : p;
}

inline float CosTheta(const Vector &w) {
	return w.z;
}

inline float SinTheta2(const Vector &w) {
	return Max(0.f, 1.f - CosTheta(w) * CosTheta(w));
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

// Eliminate serialization overhead at the cost of
// never being able to increase the version.
BOOST_CLASS_IMPLEMENTATION(luxrays::Vector, boost::serialization::object_serializable)
BOOST_CLASS_EXPORT_KEY(luxrays::Vector)

#endif	/* _LUXRAYS_VECTOR_H */
