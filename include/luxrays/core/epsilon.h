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


#ifndef _LUXRAYS_EPSILON_H
#define	_LUXRAYS_EPSILON_H

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/bbox.h"

#include <algorithm>

namespace luxrays {

// This class assume IEEE 754-2008 binary32 format for floating point numbers
// check http://en.wikipedia.org/wiki/Single_precision_floating-point_format for
// reference. Most method of this class should be thread-safe.

// OpenCL data types
namespace ocl {
#include "luxrays/core/epsilon_types.cl"
}

class MachineEpsilon {
public:
	// Not thread-safe method
	static void SetMin(const float min) { minEpsilon = min; }
	static float GetMin() { return minEpsilon; }
	// Not thread-safe method
	static void SetMax(const float max) { maxEpsilon = max; }
	static float GetMax() { return maxEpsilon; }

	// Thread-safe method
	static float E(const float value) {
		const float epsilon = fabsf(FloatAdvance(value) - value);

		return Clamp(epsilon, minEpsilon, maxEpsilon);
	}

	// Thread-safe method
	static float E(const Vector &v) {
		return std::max(E(v.x), std::max(E(v.y), E(v.z)));
	}

	// Thread-safe method
	static float E(const Point &p) {
		return std::max(E(p.x), std::max(E(p.y), E(p.z)));
	}

	// Thread-safe method
	static float E(const BBox &bb) {
		return std::max(E(bb.pMin), E(bb.pMax));
	}

private:
	union MachineFloat {
		float f;
		u_int i;
	};

	static float minEpsilon;
	static float maxEpsilon;

	// This method doesn't handle NaN, INFINITY, etc.
	static float FloatAdvance(const float value) {
		MachineFloat mf;
		mf.f = value;

		mf.i += DEFAULT_EPSILON_DISTANCE_FROM_VALUE;

		return mf.f;
	}
};

}

#endif	/* _LUXRAYS_EPSILON_H */

