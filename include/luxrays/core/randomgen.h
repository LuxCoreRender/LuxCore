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

// Tausworthe (taus113) random numbergenerator by radiance
// Based on code from GSL (GNU Scientific Library)
// MASK assures 64bit safety

#ifndef _LUXRAYS_RANDOM_H
#define _LUXRAYS_RANDOM_H

#include <cstddef>

#include "luxrays/luxrays.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/randomgen_types.cl"
}

//------------------------------------------------------------------------------
// RandomGenerator
//------------------------------------------------------------------------------

#define MASK 0xffffffffUL
#define FLOATMASK 0x00ffffffUL

#define RAN_BUFFER_AMOUNT 2048

static const float invUI = (1.f / (FLOATMASK + 1UL));

class RandomGenerator {
public:
	RandomGenerator(const unsigned long seed) {
		buf = new unsigned long[RAN_BUFFER_AMOUNT];

		init(seed);
	}

	~RandomGenerator() {
		delete[] buf;
	}

	unsigned long uintValue() {
		// Repopulate buffer if necessary
		if (bufid == RAN_BUFFER_AMOUNT) {
			for (int i = 0; i < RAN_BUFFER_AMOUNT; ++i)
				buf[i] = nobuf_generateUInt();
			bufid = 0;
		}

		return buf[bufid++];
	}

	float floatValue() {
		return (uintValue() & FLOATMASK) * invUI;
	}

	void init(const unsigned long seed) {
		bufid = RAN_BUFFER_AMOUNT;

		taus113_set(seed);
	}

private:
	unsigned long LCG(const unsigned long n) {
		return 69069UL * n; // The result is clamped to 32 bits (long)
	}

	void taus113_set(unsigned long s) {
		if (!s)
			s = 1UL; // default seed is 1

		z1 = LCG(s);
		if (z1 < 2UL)
			z1 += 2UL;
		z2 = LCG(z1);
		if (z2 < 8UL)
			z2 += 8UL;
		z3 = LCG(z2);
		if (z3 < 16UL)
			z3 += 16UL;
		z4 = LCG(z3);
		if (z4 < 128UL)
			z4 += 128UL;

		// Calling RNG ten times to satisfy recurrence condition
		for (int i = 0; i < 10; ++i)
			nobuf_generateUInt();
	}

	unsigned long nobuf_generateUInt() {
		const unsigned long b1 = ((((z1 << 6UL) & MASK) ^ z1) >> 13UL);
		z1 = ((((z1 & 4294967294UL) << 18UL) & MASK) ^ b1);

		const unsigned long b2 = ((((z2 << 2UL) & MASK) ^ z2) >> 27UL);
		z2 = ((((z2 & 4294967288UL) << 2UL) & MASK) ^ b2);

		const unsigned long b3 = ((((z3 << 13UL) & MASK) ^ z3) >> 21UL);
		z3 = ((((z3 & 4294967280UL) << 7UL) & MASK) ^ b3);

		const unsigned long b4 = ((((z4 << 3UL) & MASK) ^ z4) >> 12UL);
		z4 = ((((z4 & 4294967168UL) << 13UL) & MASK) ^ b4);

		return (z1 ^ z2 ^ z3 ^ z4);
	}

	unsigned long z1, z2, z3, z4;
	unsigned long *buf;
	int bufid;
};

/*
   This is a maximally equidistributed combined Tausworthe generator
   based on code from GNU Scientific Library 1.5 (30 Jun 2004)

    x_n = (s1_n ^ s2_n ^ s3_n)

    s1_{n+1} = (((s1_n & 4294967294) <<12) ^ (((s1_n <<13) ^ s1_n) >>19))
    s2_{n+1} = (((s2_n & 4294967288) << 4) ^ (((s2_n << 2) ^ s2_n) >>25))
    s3_{n+1} = (((s3_n & 4294967280) <<17) ^ (((s3_n << 3) ^ s3_n) >>11))

    The period of this generator is about 2^88.
 */

//------------------------------------------------------------------------------
// TauswortheRandomGenerator
//------------------------------------------------------------------------------

class TauswortheRandomGenerator {
public:
	TauswortheRandomGenerator() {
		init(131u);
	}

	// This constructor is used to build a sequence of pseudo-random numbers
	// starting form a floating point seed (usually another pseudo-random
	// number)
	TauswortheRandomGenerator(const float floatSeed) {
		union bits {
			float f;
			uint32_t i;
		};
		
		bits s;
		s.f = floatSeed;

		init(s.i);
	}

	TauswortheRandomGenerator(const unsigned int seed) {
		init(seed);
	}
	~TauswortheRandomGenerator() { }

	void init(const unsigned int seed) {
		s1 = validSeed(LCG(seed), 1);
		s2 = validSeed(LCG(s1), 7);
		s3 = validSeed(LCG(s2), 15);		
	}
	
	unsigned long uintValue() {
		s1 = TAUSWORTHE(s1, 13, 19, 4294967294UL, 12);
		s2 = TAUSWORTHE(s2, 2, 25, 4294967288UL, 4);
		s3 = TAUSWORTHE(s3, 3, 11, 4294967280UL, 17);

		return (s1 ^ s2 ^ s3);
	}

	float floatValue() {
		return (uintValue() & FLOATMASK) * invUI;
	}

private:
	unsigned int LCG(const unsigned int x) const { return x * 69069; }

	unsigned int validSeed(const unsigned int x, const unsigned int m) const {
		return (x < m) ? (x + m) : x;
	}

	unsigned int TAUSWORTHE(const unsigned int s, const unsigned int a,
		const unsigned int b, const unsigned int c,
		const unsigned int d) const {
		return ((s&c)<<d) ^ (((s << a) ^ s) >> b);
	}

	unsigned int s1, s2, s3;
};

template <typename T> void Shuffle(T *samp, size_t count, size_t dims, RandomGenerator &rng) {
	for (size_t i = 0; i < count; ++i) {
		size_t other = i + (rng.uintValue() % (count - i));
		for (size_t j = 0; j < dims; ++j)
			Swap(samp[dims * i + j], samp[dims * other + j]);
	}
}

//------------------------------------------------------------------------------
// RadicalInverse
//------------------------------------------------------------------------------

inline double RadicalInverse(u_int n, u_int base) {
	double val = 0.;
	double invBase = 1. / base, invBi = invBase;
	while (n > 0) {
		// Compute next digit of radical inverse
		u_int d_i = (n % base);
		val += d_i * invBi;
		n /= base;
		invBi *= invBase;
	}
	return val;
}

}

#endif	/* _LUXRAYS_RANDOM_H */
