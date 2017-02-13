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

#ifndef _LUXRAYS_NORMAL_H
#define _LUXRAYS_NORMAL_H

#include "luxrays/core/geometry/vector.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/normal_types.cl"
}

class Normal {
	friend class boost::serialization::access;
public:
	// Normal Methods

	Normal(float _x = 0, float _y = 0, float _z = 0)
	: x(_x), y(_y), z(_z) {
	}

	Normal operator-() const {
		return Normal(-x, -y, -z);
	}

	Normal operator+(const Normal &v) const {
		return Normal(x + v.x, y + v.y, z + v.z);
	}

	Normal & operator+=(const Normal &v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	Normal operator-(const Normal &v) const {
		return Normal(x - v.x, y - v.y, z - v.z);
	}

	Normal & operator-=(const Normal &v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	Normal operator*(float f) const {
		return Normal(f*x, f*y, f * z);
	}

	Normal & operator*=(float f) {
		x *= f;
		y *= f;
		z *= f;
		return *this;
	}

	Normal operator/(float f) const {
		float inv = 1.f / f;
		return Normal(x * inv, y * inv, z * inv);
	}

	Normal & operator/=(float f) {
		float inv = 1.f / f;
		x *= inv;
		y *= inv;
		z *= inv;
		return *this;
	}

	bool operator==(const Normal &n) const {
		return x == n.x && y == n.y && z == n.z;
	}

	bool operator!=(const Normal &n) const {
		return x != n.x || y != n.y || z != n.z;
	}

	float LengthSquared() const {
		return x * x + y * y + z*z;
	}

	float Length() const {
		return sqrtf(LengthSquared());
	}

	explicit Normal(const Vector &v) : x(v.x), y(v.y), z(v.z) {
	}

	float operator[](int i) const {
		return (&x)[i];
	}

	float &operator[](int i) {
		return (&x)[i];
	}

	bool IsNaN() const {
		return isnan(x) || isnan(y) || isnan(z);
	}

	bool IsInf() const {
		return isinf(x) || isinf(y) || isinf(z);
	}

	// Normal Public Data
	float x, y, z;

private:
	template<class Archive>
			void serialize(Archive & ar, const unsigned int version)
			{
				ar & x;
				ar & y;
				ar & z;
			}
};

inline Normal operator*(float f, const Normal &n) {
	return Normal(f * n.x, f * n.y, f * n.z);
}

inline Vector::Vector(const Normal &n)
	: x(n.x), y(n.y), z(n.z) {
}

inline std::ostream &operator<<(std::ostream &os, const Normal &v) {
	os << "Normal[" << v.x << ", " << v.y << ", " << v.z << "]";
	return os;
}

inline Normal Normalize(const Normal &n) {
	return n / n.Length();
}

inline float Dot(const Normal &n1, const Normal &n2) {
	return n1.x * n2.x + n1.y * n2.y + n1.z * n2.z;
}

inline float AbsDot(const Normal &n1, const Normal &n2) {
	return fabsf(n1.x * n2.x + n1.y * n2.y + n1.z * n2.z);
}

}

#ifdef _LUXRAYS_VECTOR_H
#include "luxrays/core/geometry/vector_normal.h"
#endif

#endif 	/* _LUXRAYS_NORMAL_H */
