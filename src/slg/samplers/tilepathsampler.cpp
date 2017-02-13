/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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
// TilePathOCL sampler
//------------------------------------------------------------------------------

// This code isn't really used
void TilePathSampler::NextSample(const vector<SampleResult> &sampleResults) {
	film->AddSampleCount(1.0);
	AddSamplesToFilm(sampleResults);
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
