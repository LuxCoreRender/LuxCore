/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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
 ***************************************************************************/

#ifndef _LUXRAYS_MATRIX3X3_H
#define _LUXRAYS_MATRIX3X3_H

#include <boost/limits.hpp>

#include "luxrays/core/geometry/vector.h"

namespace luxrays {

template <typename T>
static inline T Determinant3x3(const T matrix[3][3]) {
	double temp;
	temp  = matrix[0][0] * matrix[1][1] * matrix[2][2];
	temp -= matrix[0][0] * matrix[1][2] * matrix[2][1];
	temp -= matrix[1][0] * matrix[0][1] * matrix[2][2];
	temp += matrix[1][0] * matrix[0][2] * matrix[2][1];
	temp += matrix[2][0] * matrix[0][1] * matrix[1][2];
	temp -= matrix[2][0] * matrix[0][2] * matrix[1][1];
	return static_cast<T>(temp);
}

// square of the frobenius norm
template <typename T>
static inline T FrobNorm2(const T matrix[3][3]) {
	return 
		matrix[0][0]*matrix[0][0] + matrix[0][1]*matrix[0][1] + matrix[0][2]*matrix[0][2] +
		matrix[1][0]*matrix[1][0] + matrix[1][1]*matrix[1][1] + matrix[1][2]*matrix[1][2] +
		matrix[2][0]*matrix[2][0] + matrix[2][1]*matrix[2][1] + matrix[2][2]*matrix[2][2];
}

template <typename T>
static inline bool Invert3x3(const T matrix[3][3], T result[3][3]) {
	const T det = Determinant3x3(matrix);
	// ill-conditioned matrices will be caught by check below
	if (fabs(static_cast<double>(det)) == 0.0)
		return false;
	const T a = matrix[0][0], b = matrix[0][1], c = matrix[0][2],
		d = matrix[1][0], e = matrix[1][1], f = matrix[1][2],
		g = matrix[2][0], h = matrix[2][1], i = matrix[2][2];

	const T idet = static_cast<T>(1) / det;

	result[0][0] = (e * i - f * h) * idet;
	result[0][1] = (c * h - b * i) * idet;
	result[0][2] = (b * f - c * e) * idet;
	result[1][0] = (f * g - d * i) * idet;
	result[1][1] = (a * i - c * g) * idet;
	result[1][2] = (c * d - a * f) * idet;
	result[2][0] = (d * h - e * g) * idet;
	result[2][1] = (b * g - a * h) * idet;
	result[2][2] = (a * e - b * d) * idet;

	// check condition number of system
	const T normA = FrobNorm2(matrix);
	const T normAinv = FrobNorm2(result);

	// floats have 7 digits precision, demand at least 4
	// condition number * machine precision (2^-23) > 0.5e-4
	const T eps = std::numeric_limits<T>::epsilon();
	if ((normA * normAinv) > (0.25e-8f / (eps * eps)))
		return false;

	return true;
}

template <typename Tm, typename Tv>
static inline void Transform3x3(const Tm matrix[3][3], const Tv vector[3], Tv result[3]) {
	result[0] = static_cast<Tv>(matrix[0][0] * vector[0] + matrix[0][1] * vector[1] + matrix[0][2] * vector[2]);
	result[1] = static_cast<Tv>(matrix[1][0] * vector[0] + matrix[1][1] * vector[1] + matrix[1][2] * vector[2]);
	result[2] = static_cast<Tv>(matrix[2][0] * vector[0] + matrix[2][1] * vector[1] + matrix[2][2] * vector[2]);
}

template <typename T>
static inline Vector Transform3x3(const T matrix[3][3], const Vector &v) {
	return Vector(
		static_cast<float>(matrix[0][0] * v.x + matrix[0][1] * v.y + matrix[0][2] * v.z),
		static_cast<float>(matrix[1][0] * v.x + matrix[1][1] * v.y + matrix[1][2] * v.z),
		static_cast<float>(matrix[2][0] * v.x + matrix[2][1] * v.y + matrix[2][2] * v.z));
}

template <typename T>
static inline void Multiply3x3(const T a[3][3], const T b[3][3], T result[3][3]) {
	result[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0];
	result[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1];
	result[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2];
	result[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0];
	result[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1];
	result[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2];
	result[2][0] = a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0];
	result[2][1] = a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1];
	result[2][2] = a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2];
}

template <typename T>
static inline bool SolveLinearSystem3x3(const T A[3][3], const T B[3], T x[3]) {
	T invA[3][3];

	if (!Invert3x3(A, invA))
		return false;

	Transform3x3(invA, B, x);

	return true;
}

}

#endif //_LUXRAYS_MATRIX3X3_H
