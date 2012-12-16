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

// Tausworthe (taus113) random numbergenerator by radiance
// Based on code from GSL (GNU Scientific Library)
// MASK assures 64bit safety

#ifndef _LUXRAYS_RANDOM_H
#define _LUXRAYS_RANDOM_H

namespace luxrays {

#define MASK 0xffffffffUL
#define FLOATMASK 0x00ffffffUL

#define RAN_BUFFER_AMOUNT 2048

static const float invUI = (1.f / (FLOATMASK + 1UL));

class RandomGenerator {
public:
	RandomGenerator(const unsigned long seed) {
		buf = new unsigned long[RAN_BUFFER_AMOUNT];
		bufid = RAN_BUFFER_AMOUNT;

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

	void init(const unsigned long tn) {
		taus113_set(tn);
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

		// Calling RNG ten times to satify recurrence condition
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

class TauswortheRandomGenerator {
public:
	TauswortheRandomGenerator(const unsigned int seed) {
		s1 = validSeed(LCG(seed), 1);
		s2 = validSeed(LCG(s1), 7);
		s3 = validSeed(LCG(s2), 15);

	}
	~TauswortheRandomGenerator() { }

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

}

#endif	/* _LUXRAYS_RANDOM_H */
