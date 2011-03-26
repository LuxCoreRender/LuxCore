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

#ifndef PERMUTEDHALTON_H
#define	PERMUTEDHALTON_H

#include "smalllux.h"

#include "luxrays/utils/core/randomgen.h"

// Directly from PBRT2
// smallest floating point value less than one; all canonical random samples
// should be <= this.
#ifdef WIN32
// sadly, MSVC2008 (at least) doesn't support hexidecimal fp constants...
static const float OneMinusEpsilon = 0.9999999403953552f;
#else
static const float OneMinusEpsilon = 0x1.fffffep-1;
#endif

// Directly from PBRT2
inline void GeneratePermutation(u_int *buf, u_int b, RandomGenerator &rng) {
	for (u_int i = 0; i < b; ++i)
		buf[i] = i;
	Shuffle(buf, b, 1, rng);
}

// Directly from PBRT2
inline double PermutedRadicalInverse(u_int n, u_int base, const u_int *p) {
	double val = 0;
	double invBase = 1. / base, invBi = invBase;

	while (n > 0) {
		u_int d_i = p[n % base];
		val += d_i * invBi;
		n *= invBase;
		invBi *= invBase;
	}
	return val;
}

// Directly from PBRT2
class PermutedHalton {
public:
	// PermutedHalton Public Methods
	PermutedHalton(u_int d, RandomGenerator &rng);

	~PermutedHalton() {
		delete[] b;
		delete[] permute;
	}

	void Sample(u_int n, float *out) const {
		u_int *p = permute;
		for (u_int i = 0; i < dims; ++i) {
			out[i] = min<float>(float(PermutedRadicalInverse(n, b[i], p)), OneMinusEpsilon);
			p += b[i];
		}
	}

	static u_int GetPermuteTableSize(const u_int dims);
	static void InitTables(const u_int dims, u_int *b, u_int *permute,
		RandomGenerator &rng);

private:
	// PermutedHalton Private Data
	u_int dims;
	u_int *b, *permute;

	PermutedHalton(const PermutedHalton &);
	PermutedHalton & operator=(const PermutedHalton &);
};


#endif	/* PERMUTEDHALTON_H */
