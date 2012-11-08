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

// The code of this class is based on Tomas Davidovic's SmallVCM
// (http://www.davidovic.cz and http://www.smallvcm.com)

#ifndef _LUXRAYS_FRAME_H
#define	_LUXRAYS_FRAME_H

#include "luxrays/core/geometry/vector.h"

namespace luxrays {

class Frame {
public:
	Frame() {
		X = Vector(1.f, 0.f, 0.f);
		Y = Vector(0.f, 1.f, 0.f);
		Z = Vector(0.f, 0.f, 1.f);
	};

	Frame(const Vector &x, const Vector &y, const Vector &z) : X(x), Y(y), Z(z) {
	}

	Frame(const Vector &z) {
		SetFromZ(z);
	}

	Frame(const Normal &z) {
		SetFromZ(Vector(z));
	}

	void SetFromZ(const Normal &z) {
		SetFromZ(Vector(z)); 
	}

	void SetFromZ(const Vector &z) {
		Vector tmpZ = Z = Normalize(z);
		Vector tmpX = (std::abs(tmpZ.x) > 0.99f) ? Vector(0, 1, 0) : Vector(1, 0, 0);
		Y = Normalize(Cross(tmpZ, tmpX));
		X = Cross(Y, tmpZ);
	}

	Vector ToWorld(const Vector &a) const {
		return X * a.x + Y * a.y + Z * a.z;
	}

	Vector ToLocal(const Vector &a) const {
		return Vector(Dot(a, X), Dot(a, Y), Dot(a, Z));
	}

	const Vector &Binormal() const { return X; }
	const Vector &Tangent() const { return Y; }
	const Vector &Normal() const { return Z; }

	Vector X, Y, Z;
};

}

#endif	/* FRAME_H */

