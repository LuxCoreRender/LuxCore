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

#ifndef _LUXRAYS_UV_H
#define _LUXRAYS_UV_H

#include <ostream>
#include "luxrays/core/utils.h"

using namespace std;

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/uv_types.cl"
}

class UV {
public:
	// UV Methods

	UV(float _u = 0.f, float _v = 0.f)
	: u(_u), v(_v) {
	}

	UV(const float v[2]) : u(v[0]), v(v[1]) {
	}


	UV & operator+=(const UV &p) {
		u += p.u;
		v += p.v;
		return *this;
	}

	UV & operator-=(const UV &p) {
		u -= p.u;
		v -= p.v;
		return *this;
	}

	UV operator+(const UV &p) const {
		return UV(u + p.u, v + p.v);
	}

	UV operator-(const UV &p) const {
		return UV(u - p.u, v - p.v);
	}

	UV operator*(float f) const {
		return UV(f*u, f*v);
	}

	UV & operator*=(float f) {
		u *= f;
		v *= f;
		return *this;
	}

	UV operator/(float f) const {
		float inv = 1.f / f;
		return UV(inv*u, inv*v);
	}

	UV & operator/=(float f) {
		float inv = 1.f / f;
		u *= inv;
		v *= inv;
		return *this;
	}

	float operator[](int i) const {
		return (&u)[i];
	}

	float &operator[](int i) {
		return (&u)[i];
	}

	bool IsNaN() const {
		return isnan(u) || isnan(v);
	}

	bool IsInf() const {
		return isinf(u) || isinf(v);
	}

	// UV Public Data
	float u, v;
};

inline std::ostream & operator<<(std::ostream &os, const UV &v) {
	os << "UV[" << v.u << ", " << v.v << "]";
	return os;
}

inline UV operator*(float f, const UV &p) {
	return p*f;
}

}

#endif	/* _LUXRAYS_UV_H */
