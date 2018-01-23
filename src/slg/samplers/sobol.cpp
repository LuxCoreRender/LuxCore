/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/color/color.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SobolSamplerSharedData
//------------------------------------------------------------------------------

SobolSamplerSharedData::SobolSamplerSharedData(RandomGenerator *rndGen, Film *film) : SamplerSharedData() {
	seedBase = rndGen->uintValue() % (0xFFFFFFFFu - 1u) + 1u;

	const u_int *subRegion = film->GetSubRegion();
	filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
	pixelIndex = 0;

	pass = SOBOL_STARTOFFSET;
}

void SobolSamplerSharedData::GetNewPixelIndex(u_int &index, u_int &sobolPass, u_int &seed) {
	SpinLocker spinLocker(spinLock);
	
	index = pixelIndex;
	sobolPass = pass;
	seed = (seedBase + pixelIndex) % (0xFFFFFFFFu - 1u) + 1u;
	
	pixelIndex += SOBOL_THREAD_WORK_SIZE;
	if (pixelIndex >= filmRegionPixelCount) {
		pixelIndex = 0;
		pass++;
	}
}

SamplerSharedData *SobolSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new SobolSamplerSharedData(rndGen, film);
}

//------------------------------------------------------------------------------
// Sobol sampler
//
// This sampler is based on Blender Cycles Sobol implementation.
//------------------------------------------------------------------------------

SobolSampler::SobolSampler(RandomGenerator *rnd, Film *flm,
		const FilmSampleSplatter *flmSplatter,
		SobolSamplerSharedData *samplerSharedData) : Sampler(rnd, flm, flmSplatter),
		sharedData(samplerSharedData), directions(NULL), rngGenerator(131) {
}

SobolSampler::~SobolSampler() {
	delete directions;
}

void SobolSampler::InitNewSample() {
	// Update pixelIndexOffset

	pixelIndexOffset++;
	if ((pixelIndexOffset > SOBOL_THREAD_WORK_SIZE) ||
			(pixelIndexBase + pixelIndexOffset >= sharedData->filmRegionPixelCount)) {
		// Ask for a new base
		u_int seed;
		sharedData->GetNewPixelIndex(pixelIndexBase, pass, seed);
		pixelIndexOffset = 0;

		// Initialize the rng0, rng1 and rngPass generator
		rngGenerator.init(seed);
	}

	// Initialize rng0 and rng1

	rng0 = rngGenerator.floatValue();
	rng1 = rngGenerator.floatValue();
	rngPass = rngGenerator.uintValue();
	
	// Initialize sample0 and sample 1

	const u_int *subRegion = film->GetSubRegion();

	const u_int pixelIndex = (pixelIndexBase + pixelIndexOffset) % sharedData->filmRegionPixelCount;
	const u_int subRegionWidth = subRegion[1] - subRegion[0] + 1;
	const u_int pixelX = subRegion[0] + (pixelIndex % subRegionWidth);
	const u_int pixelY = subRegion[2] + (pixelIndex / subRegionWidth);

	sample0 = (pixelX + GetSobolSample(0)) / film->GetWidth();
	sample1 = (pixelY + GetSobolSample(1)) / film->GetHeight();	
}

void SobolSampler::RequestSamples(const u_int size) {
	directions = new u_int[size * SOBOL_BITS];
	SobolGenerateDirectionVectors(directions, size);

	pixelIndexOffset = SOBOL_THREAD_WORK_SIZE;
	InitNewSample();
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
	switch (index) {
		case 0:
			return sample0;
		case 1:
			return sample1;
		default:
			return GetSobolSample(index);
	}
}

float SobolSampler::GetSobolSample(const u_int index) {
	// I scramble pass too in order avoid correlations visible with LIGHTCPU and PATHCPU
	const u_int iResult = SobolDimension(pass + rngPass, index);
	const float fResult = iResult * (1.f / 0xffffffffu);
	
	// Cranley-Patterson rotation to reduce visible regular patterns
	const float shift = (index & 1) ? rng0 : rng1;
	const float val = fResult + shift;

	return val - floorf(val);
}

void SobolSampler::NextSample(const vector<SampleResult> &sampleResults) {
	film->AddSampleCount(1.0);
	AddSamplesToFilm(sampleResults);

	InitNewSample();
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties SobolSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type"));
}

Sampler *SobolSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	return new SobolSampler(rndGen, film, flmSplatter, (SobolSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *SobolSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::SOBOL;

	return oclSampler;
}

const Properties &SobolSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag());

	return props;
}
