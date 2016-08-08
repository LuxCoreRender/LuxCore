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

#include "slg/lights/lightstrategy.h"
#include "slg/lights/lightstrategyregistry.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightStrategy
//------------------------------------------------------------------------------

LightSource *LightStrategy::SampleLights(const float u, float *pdf) const {
		const u_int lightIndex = lightsDistribution->SampleDiscrete(u, pdf);
		assert ((lightIndex >= 0) && (lightIndex < scene->lightDefs.GetSize()));

		return scene->lightDefs.GetLightSources()[lightIndex];
	}

float LightStrategy::SampleLightPdf(const LightSource *light) const {
	return lightsDistribution->Pdf(light->lightSceneIndex);
}

Properties LightStrategy::ToProperties() const {
	return Properties() <<
			Property("lightstrategy.type")(LightStrategyType2String(GetType()));
}

LightStrategyType LightStrategy::GetType(const luxrays::Properties &cfg) {
	const string type = cfg.Get(Property("lightstrategy.type")(LightStrategyLogPower::GetObjectTag())).Get<string>();
	
	return String2LightStrategyType(type);
}

//------------------------------------------------------------------------------
// Static methods used by LightStrategyRegistry
//------------------------------------------------------------------------------

Properties LightStrategy::ToProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("lightstrategy.type")(LightStrategyLogPower::GetObjectTag())).Get<string>();

	LightStrategyRegistry::ToProperties func;

	if (LightStrategyRegistry::STATICTABLE_NAME(ToProperties).Get(type, func)) {
		return func(cfg);
	} else
		throw runtime_error("Unknown light strategy type in LightStrategy::ToProperties(): " + type);
}

LightStrategy *LightStrategy::FromProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("lightstrategy.type")(LightStrategyLogPower::GetObjectTag())).Get<string>();

	LightStrategyRegistry::FromProperties func;
	if (LightStrategyRegistry::STATICTABLE_NAME(FromProperties).Get(type, func))
		return func(cfg);
	else
		throw runtime_error("Unknown filter type in LightStrategy::FromProperties(): " + type);
}

string LightStrategy::FromPropertiesOCL(const Properties &cfg) {
	throw runtime_error("Called LightStrategy::FromPropertiesOCL()");
}

LightStrategyType LightStrategy::String2LightStrategyType(const string &type) {
	LightStrategyRegistry::GetObjectType func;
	if (LightStrategyRegistry::STATICTABLE_NAME(GetObjectType).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown light strategy type in LightStrategy::String2LightStrategyType(): " + type);
}

string LightStrategy::LightStrategyType2String(const LightStrategyType type) {
	LightStrategyRegistry::GetObjectTag func;
	if (LightStrategyRegistry::STATICTABLE_NAME(GetObjectTag).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown light strategy type in LightStrategy::LightStrategyType2String(): " + boost::lexical_cast<string>(type));
}

const Properties &LightStrategy::GetDefaultProps() {
	static Properties props;

	return props;
}

//------------------------------------------------------------------------------
// LightStrategyRegistry
//
// For the registration of each LightStrategy sub-class with LightStrategy StaticTables
//
// NOTE: you have to place all STATICTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the StaticTable) because otherwise
// static members initialization order is not defined.
//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_STATICFIELDS(LightStrategyRegistry);

//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_REGISTER(LightStrategyRegistry, LightStrategyUniform);
OBJECTSTATICREGISTRY_REGISTER(LightStrategyRegistry, LightStrategyPower);
OBJECTSTATICREGISTRY_REGISTER(LightStrategyRegistry, LightStrategyLogPower);
// Just add here any new LightStrategy (don't forget in the .h too)

//------------------------------------------------------------------------------
// LightStrategyUniform
//------------------------------------------------------------------------------

void LightStrategyUniform::Preprocess(const Scene *scn, const bool onlyInfiniteLights) {
	LightStrategy::Preprocess(scn, onlyInfiniteLights);
	
	const u_int lightCount = scene->lightDefs.GetSize();
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();
	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = lights[i];

		if (onlyInfiniteLights && !l->IsInfinite())
			lightPower.push_back(0.f);
		else
			lightPower.push_back(l->GetImportance());
	}

	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

// Static methods used by LightStrategyRegistry

Properties LightStrategyUniform::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.type"));
}

LightStrategy *LightStrategyUniform::FromProperties(const Properties &cfg) {
	return new LightStrategyUniform();
}

const Properties &LightStrategyUniform::GetDefaultProps() {
	static Properties props = Properties() <<
			LightStrategy::GetDefaultProps() <<
			Property("lightstrategy.type")(GetObjectTag());

	return props;
}

//------------------------------------------------------------------------------
// LightStrategyPower
//------------------------------------------------------------------------------

void LightStrategyPower::Preprocess(const Scene *scn, const bool onlyInfiniteLights) {
	LightStrategy::Preprocess(scn, onlyInfiniteLights);

	const float envRadius = InfiniteLightSource::GetEnvRadius(*scene);
	const float invEnvRadius2 = 1.f / (envRadius * envRadius);

	const u_int lightCount = scene->lightDefs.GetSize();
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();
	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = lights[i];

		if (onlyInfiniteLights && !l->IsInfinite())
			lightPower.push_back(0.f);
		else {
			float power = l->GetPower(*scene);
			// In order to avoid over-sampling of distant lights
			if (l->IsInfinite())
				power *= invEnvRadius2;
			lightPower.push_back(power * l->GetImportance());
		}
	}

	// Build the data to power based light sampling
	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

// Static methods used by LightStrategyRegistry

Properties LightStrategyPower::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.type"));
}

LightStrategy *LightStrategyPower::FromProperties(const Properties &cfg) {
	return new LightStrategyPower();
}

const Properties &LightStrategyPower::GetDefaultProps() {
	static Properties props = Properties() <<
			LightStrategy::GetDefaultProps() <<
			Property("lightstrategy.type")(GetObjectTag());

	return props;
}

//------------------------------------------------------------------------------
// LightStrategyLogPower
//------------------------------------------------------------------------------

void LightStrategyLogPower::Preprocess(const Scene *scn, const bool onlyInfiniteLights) {
	LightStrategy::Preprocess(scn, onlyInfiniteLights);

	const u_int lightCount = scene->lightDefs.GetSize();
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();
	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = lights[i];

		if (onlyInfiniteLights && !l->IsInfinite())
			lightPower.push_back(0.f);
		else {
			const float power = logf(1.f + l->GetPower(*scene));
			lightPower.push_back(power * l->GetImportance());
		}
	}

	// Build the data to power based light sampling
	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

// Static methods used by LightStrategyRegistry

Properties LightStrategyLogPower::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.type"));
}

LightStrategy *LightStrategyLogPower::FromProperties(const Properties &cfg) {
	return new LightStrategyPower();
}

const Properties &LightStrategyLogPower::GetDefaultProps() {
	static Properties props = Properties() <<
			LightStrategy::GetDefaultProps() <<
			Property("lightstrategy.type")(GetObjectTag());

	return props;
}
