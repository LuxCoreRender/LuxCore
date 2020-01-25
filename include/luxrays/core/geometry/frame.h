/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

// The code of this class is based on Tomas Davidovic's SmallVCM
// (http://www.davidovic.cz and http://www.smallvcm.com)

#ifndef _LUXRAYS_FRAME_H
#define	_LUXRAYS_FRAME_H

#include "luxrays/core/geometry/vector.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/frame_types.cl"
}

class Frame {
public:
	Frame() {
		X = Vector(1.f, 0.f, 0.f);
		Y = Vector(0.f, 1.f, 0.f);
		Z = Vector(0.f, 0.f, 1.f);
	};

	Frame(const Vector &x, const Vector &y, const Normal &z) : Z(Vector(z)) {
		Y = Normalize(Cross(Z, x));
		X = Cross(Y, Z);
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
		Z = z;
		CoordinateSystem(Z, &X, &Y);
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

