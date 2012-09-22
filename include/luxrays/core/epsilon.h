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

#define DEFAULT_EPSILON_MIN 1e-9f
#define DEFAULT_EPSILON_MAX 1e-1f
#define DEFAULT_EPSILON_STATIC 1e-5f

// This corrisponde to about 1e-5f for values near 1.f
#define DEFAULT_EPSILON_DISTANCE_FROM_VALUE 0x80u

class MachineEpsilon {
public:
	// Not thread-safe method
	static void SetMin(const float min) { minEpsilon = min; }
	// Not thread-safe method
	static void SetMax(const float max) { maxEpsilon = max; }

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

