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

#ifndef _LUXRAYS_RAY_H
#define _LUXRAYS_RAY_H

#include "luxrays/core/epsilon.h"
#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"

namespace luxrays {

class  Ray {
public:
	// Ray Public Methods
	Ray() : maxt(std::numeric_limits<float>::infinity()), time(0.f) {
		mint = MachineEpsilon::E(1.f);
	}

	Ray(const Point &origin, const Vector &direction) : o(origin),
		d(direction), maxt(std::numeric_limits<float>::infinity()),
		time(0.f) {
		mint = MachineEpsilon::E(origin);
	}

	Ray(const Point &origin, const Vector &direction,
		float start, float end = std::numeric_limits<float>::infinity(),
		float t = 0.f)
		: o(origin), d(direction), mint(start), maxt(end), time(t) { }

	Point operator()(float t) const { return o + d * t; }
	void GetDirectionSigns(int signs[3]) const {
		signs[0] = d.x < 0.f;
		signs[1] = d.y < 0.f;
		signs[2] = d.z < 0.f;
	}

	// Ray Public Data
	Point o;
	Vector d;
	mutable float mint, maxt;
	float time;
#define _LUXRAYS_RAY_OCLDEFINE "typedef struct { Point o; Vector d; float mint, maxt, time; } Ray;\n"
};

inline std::ostream &operator<<(std::ostream &os, const Ray &r) {
	os << "Ray[" << r.o << ", " << r.d << ", " << r.mint << "," << r.maxt << "]";
	return os;
}

class RayHit {
public:
	float t;
	float b1, b2; // Barycentric coordinates of the hit point
	unsigned int index;
#define _LUXRAYS_RAYHIT_OCLDEFINE "typedef struct { float t, b1, b2; unsigned int index; } RayHit;\n"

	void SetMiss() { index = 0xffffffffu; };
	bool Miss() const { return (index == 0xffffffffu); };
};

}

#endif	/* _LUXRAYS_RAY_H */
