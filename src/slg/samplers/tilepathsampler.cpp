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
#include "slg/samplers/tilepathsampler.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathSamplerSharedData
//------------------------------------------------------------------------------

SamplerSharedData *TilePathSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new TilePathSamplerSharedData();
}

//------------------------------------------------------------------------------
// TilePath sampler
//------------------------------------------------------------------------------

TilePathSampler::TilePathSampler(luxrays::RandomGenerator *rnd, Film *flm,
		const FilmSampleSplatter *flmSplatter) : Sampler(rnd, flm, flmSplatter),
		sobolSequence(), rngGenerator(131) {
	aaSamples = 1;
}

TilePathSampler::~TilePathSampler() {
}

void TilePathSampler::SetAASamples(const u_int aaSamp) {
	aaSamples = aaSamp;
}

void TilePathSampler::RequestSamples(const u_int size) {
	sobolSequence.RequestSamples(size);
}

void TilePathSampler::InitNewSample() {
	// Initialize rng0, rng1 and rngPass

	sobolSequence.rng0 = rngGenerator.floatValue();
	sobolSequence.rng1 = rngGenerator.floatValue();
	// Limit the number of pass skipped
	sobolSequence.rngPass = rngGenerator.uintValue() % 512;

	// Initialize sample0 and sample1

	const u_int *subRegion = film->GetSubRegion();
	sample0 = (tile->coord.x - subRegion[0] + tileX + sobolSequence.GetSample(tilePass, 0)) / (subRegion[1] - subRegion[0] + 1);
	sample1 = (tile->coord.y - subRegion[2] + tileY + sobolSequence.GetSample(tilePass, 1)) / (subRegion[3] - subRegion[2] + 1);	
}

float TilePathSampler::GetSample(const u_int index) {
	switch (index) {
		case 0:
			return sample0;
		case 1:
			return sample1;
		default:
			return sobolSequence.GetSample(tilePass, index);
	}
}

void TilePathSampler::NextSample(const vector<SampleResult> &sampleResults) {
	tileFilm->AddSampleCount(1.0);
	tileFilm->AddSample(tileX, tileY, sampleResults[0]);

	++tileX;
	if (tileX >= tile->coord.width) {
		tileX = 0;
		++tileY;

		if (tileY >= tile->coord.height) {
			// Restart
			tileY = 0;
			++tilePass;
		}
	}
			
	InitNewSample();
}

void TilePathSampler::Init(TileRepository::Tile *t, Film *tFilm) {
	tile = t;
	tileFilm = tFilm;

	// To have always the same sequence for each tile
	const u_int seed = t->coord.x + (t->coord.y << 16);
	rngGenerator.init(seed);

	tileX = 0;
	tileY = 0;
	tilePass = t->pass * aaSamples * aaSamples;
	
	InitNewSample();
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties TilePathSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type"));
}

Sampler *TilePathSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	return new TilePathSampler(rndGen, film, flmSplatter);
}

slg::ocl::Sampler *TilePathSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::TILEPATHSAMPLER;

	return oclSampler;
}

const Properties &TilePathSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag());

	return props;
}
