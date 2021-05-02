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

#include <math.h>

#include "slg/samplers/sobolsequence.h"

using namespace std;
using namespace luxrays;
using namespace slg;

constexpr float S = float(1.0/(4294967296));

inline u_int hash_combine(u_int seed, u_int v)
{
  return seed ^ (v + (seed << 6) + (seed >> 2));
}

inline u_int reverse_bits(u_int x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
	return ((x >> 16) | (x << 16));
}

inline u_int laine_karras_permutation(u_int x, u_int seed) {
	x += seed;
	x ^= x * 0x6c50b47cu;
	x ^= x * 0xb82f1e52u;
	x ^= x * 0xc7afe638u;
	x ^= x * 0x8d22f6e6u;
	return x;
}

inline u_int nested_uniform_scramble_base2(u_int x, u_int seed) {
	x = reverse_bits(x);
	x = laine_karras_permutation(x, seed);
	x = reverse_bits(x);
	return x;
}

inline u_int cmj_hash_simple(u_int i, u_int p) {
	i = (i ^ 61) ^ p;
	i += i << 3;
	i ^= i >> 4;
	i *= 0x27d4eb2d;
	return i;
}

//------------------------------------------------------------------------------
// SobolSequence
//------------------------------------------------------------------------------

SobolSequence::SobolSequence() : directions(NULL) {
	rngPass = 0;
	rng0 = 0.f;
	rng1 = 0.f;
}

SobolSequence::~SobolSequence() {
	delete[] directions;
}

void SobolSequence::RequestSamples(const u_int size) {
	directions = new u_int[size * SOBOL_BITS];
	GenerateDirectionVectors(directions, size);
}

u_int SobolSequence::SobolDimension(const u_int index, const u_int dimension) const {
	const u_int offset = dimension * SOBOL_BITS;
	u_int result = 0;
	u_int i = index;

	for (u_int j = 0; i; i >>= 1, j++) {
		if (i & 1)
			result ^= directions[offset + j];
	}

	return result;
}

float SobolSequence::GetSample(const u_int pass, const u_int index) {
	// I scramble pass too in order avoid correlations visible with LIGHTCPU and BIDIRCPU
	const u_int iResult = SobolDimension(pass + rngPass, index);
	const float fResult = iResult * (1.f / 0xffffffffu);
	
	// Cranley-Patterson rotation to reduce visible regular patterns
	const float shift = (index & 1) ? rng0 : rng1;
	const float val = fResult + shift;

	return val - floorf(val);
}

float SobolSequence::GetSampleOwen(const u_int pass, const u_int index) {
	//Get a per-pixel fixed seed for every 4 dimensions
	u_int seed = cmj_hash_simple(index/4, rngPass);
	
	// Scramble the pass in the sobol sequence
	u_int owenIndex = nested_uniform_scramble_base2(pass, seed);

	// Get sobol sample using the scrambled pass. Using only four dimensions.
	// The scrambling allows to pad them to make for higher-dimension samples
	u_int X = GetSobolSample(owenIndex, index%4);

	// Get Owen-scrambled sobol sample using the laine-karras permutation and some bit inversion
    float sample = nested_uniform_scramble_base2(X, hash_combine(seed, index%4)) * S;
	
	// if (index == 0 || index == 1 || index == 12 || index == 13) {
	// 	SLG_LOG("pass: " << pass  << " hash: " << hash << " rngPass:" << rngPass << " index: " << index  << " sample: " << sample << " sample2: " << sample2);
	// 	SLG_LOG("X " << X << " X2 " << X2);
	// }
	return sample;
}