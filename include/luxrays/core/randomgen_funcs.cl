#line 2 "randomgen_funcs.cl"

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

//------------------------------------------------------------------------------
// Random number generator
// maximally equidistributed combined Tausworthe generator
//------------------------------------------------------------------------------

#define FLOATMASK 0x00ffffffu

OPENCL_FORCE_INLINE uint TAUSWORTHE(const uint s, const uint a,
	const uint b, const uint c,
	const uint d) {
	return ((s & c) << d) ^ (((s << a) ^ s) >> b);
}

OPENCL_FORCE_INLINE uint LCG(const uint x) { return x * 69069; }

OPENCL_FORCE_INLINE uint ValidSeed(const uint x, const uint m) {
	return (x < m) ? (x + m) : x;
}

OPENCL_FORCE_INLINE void Rnd_Init(uint seed, Seed *s) {
	s->s1 = ValidSeed(LCG(seed), 1);
	s->s2 = ValidSeed(LCG(s->s1), 7);
	s->s3 = ValidSeed(LCG(s->s2), 15);
}

// This constructor is used to build a sequence of pseudo-random numbers
// starting form a floating point seed (usually another pseudo-random
// number)
OPENCL_FORCE_INLINE void Rnd_InitFloat(const float floatSeed, Seed *s) {
	union {
		float f;
		uint i;
	} bits;

	bits.f = floatSeed;

	Rnd_Init(bits.i, s);
}

OPENCL_FORCE_INLINE unsigned long Rnd_UintValue(Seed *s) {
	s->s1 = TAUSWORTHE(s->s1, 13, 19, 4294967294UL, 12);
	s->s2 = TAUSWORTHE(s->s2, 2, 25, 4294967288UL, 4);
	s->s3 = TAUSWORTHE(s->s3, 3, 11, 4294967280UL, 17);

	return ((s->s1) ^ (s->s2) ^ (s->s3));
}

OPENCL_FORCE_INLINE float Rnd_FloatValue(Seed *s) {
	return (Rnd_UintValue(s) & FLOATMASK) * (1.f / (FLOATMASK + 1UL));
}
