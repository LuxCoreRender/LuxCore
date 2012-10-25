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

#ifndef _LUXRAYS_POINT_H
#define _LUXRAYS_POINT_H

#include "luxrays/core/geometry/vector.h"
#include <iostream>
using std::ostream;
#include <boost/serialization/access.hpp>

namespace luxrays {

class Point {
	friend class boost::serialization::access;
public:
	// Point Methods

	Point(float _x = 0.f, float _y = 0.f, float _z = 0.f)
	: x(_x), y(_y), z(_z) {
	}

	Point(float v[3]) : x(v[0]), y(v[1]), z(v[2]) {
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

	// Point Public Data
	float x, y, z;
#define _LUXRAYS_POINT_OCLDEFINE "typedef struct { float x, y, z; } Point;\n"

private:
	template<class Archive>
		void serialize(Archive & ar, const unsigned int version)
		{
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

#endif	/* _LUXRAYS_POINT_H */
