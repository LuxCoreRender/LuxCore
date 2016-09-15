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

#ifndef _LUXRAYS_BSPHERE_H
#define _LUXRAYS_BSPHERE_H

#include "luxrays/core/geometry/point.h"

namespace luxrays {

class BSphere {
public:
	// BBox Public Methods

	BSphere() : center(0.f, 0.f, 0.f) {
		rad = 0.f;
	}

	BSphere(const Point &c, const float r) : center(c) {
		rad = r;
	}

	// BSphere Public Data
	Point center;
	float rad;
#define _LUXRAYS_BSPHERE_OCLDEFINE "typedef struct { Point center; float rad; } BSphere;\n"
};

inline std::ostream &operator<<(std::ostream &os, const BSphere &s) {
	os << "BSphere[" << s.center << ", " << s.rad << "]";
	return os;
}

}

#endif	/* _LUXRAYS_BSPHERE_H */
