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
#include "slg/samplers/samplerregistry.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SamplerSharedData
//------------------------------------------------------------------------------

SamplerSharedData *SamplerSharedData::FromProperties(const Properties &cfg, RandomGenerator *rndGen, Film *film) {
	const string type = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();

	SamplerSharedDataRegistry::FromProperties func;
	if (SamplerSharedDataRegistry::STATICTABLE_NAME(FromProperties).Get(type, func))
		return func(cfg, rndGen, film);
	else
		throw runtime_error("Unknown sampler type in SamplerSharedData::FromProperties(): " + type);
}

//------------------------------------------------------------------------------
// Sampler
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties Sampler::ToProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();

	SamplerRegistry::ToProperties func;

	if (SamplerRegistry::STATICTABLE_NAME(ToProperties).Get(type, func))
		return Properties() << func(cfg);
	else
		throw runtime_error("Unknown sampler type in Sampler::ToProperties(): " + type);
}

Sampler *Sampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const string type = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();

	SamplerRegistry::FromProperties func;
	if (SamplerRegistry::STATICTABLE_NAME(FromProperties).Get(type, func))
		return func(cfg, rndGen, film, flmSplatter, sharedData);
	else
		throw runtime_error("Unknown sampler type in Sampler::FromProperties(): " + type);
}

slg::ocl::Sampler *Sampler::FromPropertiesOCL(const Properties &cfg) {
	const string type = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();

	SamplerRegistry::FromPropertiesOCL func;
	if (SamplerRegistry::STATICTABLE_NAME(FromPropertiesOCL).Get(type, func))
		return func(cfg);
	else
		throw runtime_error("Unknown sampler type in Sampler::FromPropertiesOCL(): " + type);
}

SamplerType Sampler::String2SamplerType(const string &type) {
	SamplerRegistry::GetObjectType func;
	if (SamplerRegistry::STATICTABLE_NAME(GetObjectType).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown sampler type in Sampler::String2SamplerType(): " + type);
}

string Sampler::SamplerType2String(const SamplerType type) {
	SamplerRegistry::GetObjectTag func;
	if (SamplerRegistry::STATICTABLE_NAME(GetObjectTag).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown sampler type in Sampler::SamplerType2String(): " + boost::lexical_cast<string>(type));
}

const Properties &Sampler::GetDefaultProps() {
	static Properties props;

	return props;
}

//------------------------------------------------------------------------------
// SamplerSharedDataRegistry
//
// For the registration of each SamplerSharedData sub-class
// with SamplerSharedData StaticTable
//
// NOTE: you have to place all STATICTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the StaticTable) because otherwise
// static members initialization order is not defined.
//------------------------------------------------------------------------------

STATICTABLE_DECLARATION(SamplerSharedDataRegistry, string, FromProperties);

//------------------------------------------------------------------------------

SAMPLERSHAREDDATA_STATICTABLE_REGISTER(RandomSampler::GetObjectTag(), RandomSamplerSharedData);
SAMPLERSHAREDDATA_STATICTABLE_REGISTER(SobolSampler::GetObjectTag(), SobolSamplerSharedData);
SAMPLERSHAREDDATA_STATICTABLE_REGISTER(MetropolisSampler::GetObjectTag(), MetropolisSamplerSharedData);
SAMPLERSHAREDDATA_STATICTABLE_REGISTER(RTPathCPUSampler::GetObjectTag(), RTPathCPUSamplerSharedData);
// Just add here any new SamplerSharedData (don't forget in the .h too)

//------------------------------------------------------------------------------
// SamplerRegistry
//
// For the registration of each Sampler sub-class with Sampler StaticTables
//
// NOTE: you have to place all STATICTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the StaticTable) because otherwise
// static members initialization order is not defined.
//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_STATICFIELDS(SamplerRegistry);

//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_REGISTER(SamplerRegistry, RandomSampler);
OBJECTSTATICREGISTRY_REGISTER(SamplerRegistry, SobolSampler);
OBJECTSTATICREGISTRY_REGISTER(SamplerRegistry, MetropolisSampler);
OBJECTSTATICREGISTRY_REGISTER(SamplerRegistry, RTPathCPUSampler);
// Just add here any new Sampler (don't forget in the .h too)
