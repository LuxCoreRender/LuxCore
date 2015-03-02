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

#ifndef _LUXRAYS_RAY_H
#define _LUXRAYS_RAY_H

#include "luxrays/core/epsilon.h"
#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"

#include <boost/math/special_functions/sign.hpp>

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/ray_types.cl"
}

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
		const float start, const float end = std::numeric_limits<float>::infinity(),
		const float t = 0.f)
		: o(origin), d(direction), mint(start), maxt(end), time(t) { }

	Point operator()(float t) const { return o + d * t; }
	void GetDirectionSigns(int signs[3]) const {
		signs[0] = boost::math::signbit(d.x);
		signs[1] = boost::math::signbit(d.y);
		signs[2] = boost::math::signbit(d.z);
	}
	
	void Update(const Point &origin, const Vector &direction) {
		o = origin;
		d = direction;
		mint = MachineEpsilon::E(origin);
		maxt = std::numeric_limits<float>::infinity();
		// Keep the same time value
	}

	// Ray Public Data
	Point o;
	Vector d;
	mutable float mint, maxt;
	float time;
	float pad[3]; // Add padding to avoid size discrepancies with OpenCL
};

inline std::ostream &operator<<(std::ostream &os, const Ray &r) {
	os << "Ray[" << r.o << ", " << r.d << ", " << r.mint << "," << r.maxt << "]";
	return os;
}

class RayHit {
public:
	float t;
	float b1, b2; // Barycentric coordinates of the hit point
	unsigned int meshIndex, triangleIndex;

	void SetMiss() { meshIndex = 0xffffffffu; };
	bool Miss() const { return (meshIndex == 0xffffffffu); };
};

inline std::ostream &operator<<(std::ostream &os, const RayHit &rh) {
	os << "RayHit[" << rh.t << ", (" << rh.b1 << ", " << rh.b2 << "), " <<
			rh.meshIndex << ", " << rh.triangleIndex << "]";
	return os;
}

}

#endif	/* _LUXRAYS_RAY_H */
