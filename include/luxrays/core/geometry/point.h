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

#ifndef _LUXRAYS_POINT_H
#define _LUXRAYS_POINT_H

#include <iostream>

#include "luxrays/core/geometry/vector.h"
#include "luxrays/utils/serializationutils.h"

namespace luxrays {

	// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/point_types.cl"
}

class Point {
public:
	// Point Methods

	Point(float _x = 0.f, float _y = 0.f, float _z = 0.f)
	: x(_x), y(_y), z(_z) {
	}

	Point(const float v[3]) : x(v[0]), y(v[1]), z(v[2]) {
	}

	Point operator+(const Vector &v) const {
		return Point(x + v.x, y + v.y, z + v.z);
	}

	Point & operator+=(const Vector &v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	Vector operator-(const Point &p) const {
		return Vector(x - p.x, y - p.y, z - p.z);
	}

	Point operator-(const Vector &v) const {
		return Point(x - v.x, y - v.y, z - v.z);
	}

	Point & operator-=(const Vector &v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	Point & operator+=(const Point &p) {
		x += p.x;
		y += p.y;
		z += p.z;
		return *this;
	}

	Point & operator-=(const Point &p) {
		x -= p.x;
		y -= p.y;
		z -= p.z;
		return *this;
	}

	Point operator+(const Point &p) const {
		return Point(x + p.x, y + p.y, z + p.z);
	}

	Point operator*(float f) const {
		return Point(f*x, f*y, f * z);
	}

	Point & operator*=(float f) {
		x *= f;
		y *= f;
		z *= f;
		return *this;
	}

	Point operator/(float f) const {
		float inv = 1.f / f;
		return Point(inv*x, inv*y, inv * z);
	}

	Point & operator/=(float f) {
		float inv = 1.f / f;
		x *= inv;
		y *= inv;
		z *= inv;
		return *this;
	}

	bool operator==(const Point &p) const {
		return x == p.x && y == p.y && z == p.z;
	}

	bool operator!=(const Point &p) const {
		return x != p.x || y != p.y || z != p.z;
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

	//--------------------------------------------------------------------------
	// Required by OpenSubdiv interface
	//--------------------------------------------------------------------------

	void Clear(void * = 0) {
        x = 0.f;
		y = 0.f;
		z = 0.f;
    }
	
	void AddWithWeight(Point const &src, float weight) {
        x += weight * src.x;
        y += weight * src.y;
        z += weight * src.z;
    }
	
	//--------------------------------------------------------------------------
	
	friend class boost::serialization::access;

	// Point Public Data
	float x, y, z;
#define _LUXRAYS_POINT_OCLDEFINE "typedef struct { float x, y, z; } Point;\n"

private:
	template<class Archive>	void serialize(Archive & ar, const unsigned int version) {
		ar & x;
		ar & y;
		ar & z;
	}
};

inline Vector::Vector(const Point &p)
	: x(p.x), y(p.y), z(p.z) {
}

inline std::ostream & operator<<(std::ostream &os, const Point &v) {
	os << "Point[" << v.x << ", " << v.y << ", " << v.z << "]";
	return os;
}

inline Point operator*(float f, const Point &p) {
	return p*f;
}

inline float Distance(const Point &p1, const Point &p2) {
	return (p1 - p2).Length();
}

inline float DistanceSquared(const Point &p1, const Point &p2) {
	return (p1 - p2).LengthSquared();
}

}

// Eliminate serialization overhead at the cost of
// never being able to increase the version.
BOOST_CLASS_IMPLEMENTATION(luxrays::Point, boost::serialization::object_serializable)
BOOST_CLASS_EXPORT_KEY(luxrays::Point)

#endif	/* _LUXRAYS_POINT_H */
