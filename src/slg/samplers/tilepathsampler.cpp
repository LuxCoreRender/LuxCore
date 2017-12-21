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
		const FilmSampleSplatter *flmSplatter) : Sampler(rnd, flm, flmSplatter) {
	aaSamples = 1;
}

TilePathSampler::~TilePathSampler() {
}

void TilePathSampler::SetAASamples(const u_int aaSamp) {
	aaSamples = aaSamp;
}

void TilePathSampler::SampleGrid(const u_int ix, const u_int iy, float *u0, float *u1) const {
	*u0 = rndGen->floatValue();
	*u1 = rndGen->floatValue();

	if (aaSamples > 1) {
		const float idim = 1.f / aaSamples;
		*u0 = (ix + *u0) * idim;
		*u1 = (iy + *u1) * idim;
	}
}

void TilePathSampler::InitNewSample() {
	float u0, u1;
	SampleGrid(tileSampleX, tileSampleY, &u0, &u1);

	const u_int *subRegion = film->GetSubRegion();
	sample0 = (tile->coord.x - subRegion[0] + tileX + u0) / (subRegion[1] - subRegion[0] + 1);
	sample1 = (tile->coord.y - subRegion[2] + tileY + u1) / (subRegion[3] - subRegion[2] + 1);	
}

float TilePathSampler::GetSample(const u_int index) {
	switch (index) {
		case 0:
			return sample0;
		case 1:
			return sample1;
		default:
			return rndGen->floatValue();
	}
}

void TilePathSampler::NextSample(const vector<SampleResult> &sampleResults) {
	tileFilm->AddSampleCount(1.0);
	tileFilm->AddSample(tileX, tileY, sampleResults[0]);

	++tileSampleX;
	if (tileSampleX >= aaSamples) {
		tileSampleX = 0;
		++tileSampleY;

		if (tileSampleY >= aaSamples) {
			tileSampleY = 0;
			++tileX;

			if (tileX >= tile->coord.width) {
				tileX = 0;
				++tileY;

				if (tileY >= tile->coord.height) {
					// Restart

					tileY = 0;
				}
			}
			
		}
	}

	InitNewSample();
}

void TilePathSampler::Init(TileRepository::Tile *t, Film *tFilm) {
	tile = t;
	tileFilm = tFilm;

	tileX = 0;
	tileY = 0;
	tileSampleX = 0;
	tileSampleY = 0;
	
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
