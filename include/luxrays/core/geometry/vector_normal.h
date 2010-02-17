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

#ifndef _LUXRAYS_VECTOR_NORMAL_H
#define _LUXRAYS_VECTOR_NORMAL_H

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/normal.h"

namespace luxrays {

// Geometry Inline Functions
inline Vector Cross(const Vector &v1, const Normal &v2) {
	return Vector((v1.y * v2.z) - (v1.z * v2.y),
                  (v1.z * v2.x) - (v1.x * v2.z),
                  (v1.x * v2.y) - (v1.y * v2.x));
}

inline Vector Cross(const Normal &v1, const Vector &v2) {
	return Vector((v1.y * v2.z) - (v1.z * v2.y),
                  (v1.z * v2.x) - (v1.x * v2.z),
                  (v1.x * v2.y) - (v1.y * v2.x));
}

inline float Dot(const Normal &n1, const Vector &v2) {
	return n1.x * v2.x + n1.y * v2.y + n1.z * v2.z;
}

inline float Dot(const Vector &v1, const Normal &n2) {
	return v1.x * n2.x + v1.y * n2.y + v1.z * n2.z;
}

inline float AbsDot(const Vector &v1, const Normal &n2) {
	return fabsf(v1.x * n2.x + v1.y * n2.y + v1.z * n2.z);
}

inline float AbsDot(const Normal &n1, const Vector &v2) {
	return fabsf(n1.x * v2.x + n1.y * v2.y + n1.z * v2.z);
}

}

#endif 	/* _LUXRAYS_VECTOR_NORMAL_H */
