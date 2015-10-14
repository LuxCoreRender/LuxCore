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

using namespace std;
using namespace luxrays;
using namespace slg;

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
	const ToPropertiesFuncPtr func = toPropertiesFuncTable.Get(type);
	if (func)
		return Properties() << func(cfg);
	else
		throw runtime_error("Unknown sampler type in Sampler::ToProperties(): " + type);
}

FuncTable<Sampler::ToPropertiesFuncPtr> Sampler::toPropertiesFuncTable;
