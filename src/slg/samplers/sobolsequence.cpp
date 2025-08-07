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
	u_int offset = dimension * SOBOL_BITS;
	u_int result = 0;
	u_int i = index;

	while (i) {
		result ^= ((i & 1) * directions[offset]);
		i >>= 1; offset++;
		result ^= ((i & 1) * directions[offset]);
		i >>= 1; offset++;
		result ^= ((i & 1) * directions[offset]);
		i >>= 1; offset++;
		result ^= ((i & 1) * directions[offset]);
		i >>= 1; offset++;
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
