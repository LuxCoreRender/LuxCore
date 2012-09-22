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
};

inline std::ostream &operator<<(std::ostream &os, const BSphere &s) {
	os << "BSphere[" << s.center << ", " << s.rad << "]";
	return os;
}

}

#endif	/* _LUXRAYS_BSPHERE_H */
