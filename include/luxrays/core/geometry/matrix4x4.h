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

#ifndef _LUXRAYS_MATRIX4X4_H
#define _LUXRAYS_MATRIX4X4_H

#include <ostream>

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
#include "luxrays/core/geometry/matrix4x4_types.cl"
}

class Matrix4x4 {
public:
	// Matrix4x4 Public Methods
	Matrix4x4() {
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				if (i == j)
					m[i][j] = 1.f;
				else
					m[i][j] = 0.f;
	}
	Matrix4x4(float mat[4][4]);
	Matrix4x4(float t00, float t01, float t02, float t03,
			float t10, float t11, float t12, float t13,
			float t20, float t21, float t22, float t23,
			float t30, float t31, float t32, float t33);

	Matrix4x4 Transpose() const;
	float Determinant() const;

	void Print(std::ostream &os) const {
		os << "Matrix4x4[ ";
		for (int i = 0; i < 4; ++i) {
			os << "[ ";
			for (int j = 0; j < 4; ++j) {
				os << m[i][j];
				if (j != 3) os << ", ";
			}
			os << " ] ";
		}
		os << " ] ";
	}

	Matrix4x4 operator*(const Matrix4x4 &mat) const {
		float r[4][4];
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				r[i][j] = m[i][0] * mat.m[0][j] +
					m[i][1] * mat.m[1][j] +
					m[i][2] * mat.m[2][j] +
					m[i][3] * mat.m[3][j];

		return Matrix4x4(r);
	}

	Matrix4x4 Inverse() const;

	friend std::ostream &operator<<(std::ostream &, const Matrix4x4 &);

	float m[4][4];
};

inline std::ostream & operator<<(std::ostream &os, const Matrix4x4 &m) {
	m.Print(os);
	return os;
}

}

#endif	/* _LUXRAYS_MATRIX4X4_H */
