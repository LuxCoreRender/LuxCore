/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/color/color.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/sobol.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SobolSamplerSharedData
//------------------------------------------------------------------------------

SobolSamplerSharedData::SobolSamplerSharedData(luxrays::RandomGenerator *rnd) : SamplerSharedData() {
	rng0 = rnd->floatValue();
	rng1 = rnd->floatValue();
	pass = SOBOL_STARTOFFSET;
}

//------------------------------------------------------------------------------
// Sobol sampler
//
// This sampler is based on Blender Cycles Sobol implementation.
//------------------------------------------------------------------------------

SobolSampler::SobolSampler(luxrays::RandomGenerator *rnd, Film *flm,
		SobolSamplerSharedData *samplerSharedData) : Sampler(rnd, flm),
		sharedData(samplerSharedData), directions(NULL) {
}

SobolSampler::~SobolSampler() {
	delete directions;
}

void SobolSampler::RequestSamples(const u_int size) {
	directions = new u_int[size * SOBOL_BITS];
	SobolGenerateDirectionVectors(directions, size);

	passBase = sharedData->pass.fetch_add(SOBOL_THREAD_WORK_SIZE);
	passOffset = 0;
}

u_int SobolSampler::SobolDimension(const u_int index, const u_int dimension) const {
	const u_int offset = dimension * SOBOL_BITS;
	u_int result = 0;
	u_int i = index;

	for (u_int j = 0; i; i >>= 1, j++) {
		if (i & 1)
			result ^= directions[offset + j];
	}

	return result;
}

float SobolSampler::GetSample(const u_int index) {
	const u_int iResult = SobolDimension(passBase + passOffset, index);
	const float fResult = iResult * (1.f / 0xffffffffu);
	
	// Cranley-Patterson rotation to reduce visible regular patterns
	const float shift = (index & 1) ? sharedData->rng0 : sharedData->rng1;
	const float val = fResult + shift;

	return val - floorf(val);
}

void SobolSampler::NextSample(const std::vector<SampleResult> &sampleResults) {
	film->AddSampleCount(1.0);
	AddSamplesToFilm(sampleResults);

	++passOffset;
	if (passOffset >= SOBOL_THREAD_WORK_SIZE) {
		passBase = sharedData->pass.fetch_add(SOBOL_THREAD_WORK_SIZE);
		passOffset = 0;
	}
}

SobolSamplerSharedData *SobolSampler::AllocSharedData(luxrays::RandomGenerator *rnd) {
	return new SobolSamplerSharedData(rnd);
}
