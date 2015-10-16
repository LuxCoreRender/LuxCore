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
#include "slg/samplers/random.h"
#include "slg/samplers/sobol.h"
#include "slg/samplers/metropolis.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SamplerSharedData
//------------------------------------------------------------------------------

SamplerSharedData *SamplerSharedData::FromProperties(const Properties &cfg, RandomGenerator *rndGen) {
	const string type = cfg.Get(Property("sampler.type")("RANDOM")).Get<string>();

	const FromPropertiesFuncPtr func = FUNCTABLE_NAME(FromProperties).GetFunc(type);
	if (func)
		return func(cfg, rndGen);
	else
		throw runtime_error("Unknown sampler type in SamplerSharedData::FromProperties(): " + type);
}

FUNCTABLE_DECLARATION(SamplerSharedData, FromProperties);
// NOTE: you have to place all FUNCTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the FuncTable) or the compiler optimizer
// will remove the code.
FUNCTABLE_REGISTER(RandomSamplerSharedData, Sampler::SamplerType2String(RANDOM), FromProperties);
FUNCTABLE_REGISTER(SobolSamplerSharedData, Sampler::SamplerType2String(SOBOL), FromProperties);
FUNCTABLE_REGISTER(MetropolisSamplerSharedData, Sampler::SamplerType2String(METROPOLIS), FromProperties);

//------------------------------------------------------------------------------
// Sampler
//------------------------------------------------------------------------------

SamplerType Sampler::String2SamplerType(const string &type) {
	if ((type.compare("INLINED_RANDOM") == 0) ||
			(type.compare("RANDOM") == 0))
		return RANDOM;
	if (type.compare("METROPOLIS") == 0)
		return METROPOLIS;
	if (type.compare("SOBOL") == 0)
		return SOBOL;

	throw runtime_error("Unknown sampler type: " + type);
}

const string Sampler::SamplerType2String(const SamplerType type) {
	switch (type) {
		case RANDOM:
			return "RANDOM";
		case METROPOLIS:
			return "METROPOLIS";
		case SOBOL:
			return "SOBOL";
		default:
			throw runtime_error("Unknown sampler type: " + boost::lexical_cast<string>(type));
	}
}

void Sampler::AddSamplesToFilm(const vector<SampleResult> &sampleResults, const float weight) const {
	for (vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr < sampleResults.end(); ++sr) {
		if (sr->useFilmSplat)
			filmSplatter->SplatSample(*film, *sr, weight);
		else
			film->AddSample(sr->pixelX, sr->pixelY, *sr, weight);
	}
}

Properties Sampler::ToProperties() const {
	return Properties() <<
			Property("sampler.type")(SamplerType2String(GetType()));
}

Properties Sampler::ToProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("sampler.type")("RANDOM")).Get<string>();

	const ToPropertiesFuncPtr func = FUNCTABLE_NAME(ToProperties).GetFunc(type);
	if (func)
		return Properties() << func(cfg);
	else
		throw runtime_error("Unknown sampler type in Sampler::ToProperties(): " + type);
}

Sampler *Sampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const string type = cfg.Get(Property("sampler.type")("RANDOM")).Get<string>();

	const FromPropertiesFuncPtr func = FUNCTABLE_NAME(FromProperties).GetFunc(type);
	if (func)
		return func(cfg, rndGen, film, flmSplatter, sharedData);
	else
		throw runtime_error("Unknown sampler type in Sampler::FromProperties(): " + type);
}

FUNCTABLE_DECLARATION(Sampler, ToProperties);
// NOTE: you have to place all FUNCTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the FuncTable) or the compiler optimizer
// will remove the code.
FUNCTABLE_REGISTER(RandomSampler, Sampler::SamplerType2String(RANDOM), ToProperties);
FUNCTABLE_REGISTER(SobolSampler, Sampler::SamplerType2String(SOBOL), ToProperties);
FUNCTABLE_REGISTER(MetropolisSampler, Sampler::SamplerType2String(METROPOLIS), ToProperties);

FUNCTABLE_DECLARATION(Sampler, FromProperties);
// NOTE: you have to place all FUNCTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the FuncTable) or the compiler optimizer
// will remove the code.
FUNCTABLE_REGISTER(RandomSampler, Sampler::SamplerType2String(RANDOM), FromProperties);
FUNCTABLE_REGISTER(SobolSampler, Sampler::SamplerType2String(SOBOL), FromProperties);
FUNCTABLE_REGISTER(MetropolisSampler, Sampler::SamplerType2String(METROPOLIS), FromProperties);
