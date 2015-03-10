/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
#include "slg/sdl/scene.h"

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

//------------------------------------------------------------------------------
// LightStrategyUniform
//------------------------------------------------------------------------------

void LightStrategyUniform::Preprocess(const Scene *scn) {
	LightStrategy::Preprocess(scn);
	
	const u_int lightCount = scene->lightDefs.GetSize();
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = scene->lightDefs.GetLightSource(i);

		lightPower.push_back(l->GetImportance());
	}

	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

//------------------------------------------------------------------------------
// LightStrategyPower
//------------------------------------------------------------------------------

void LightStrategyPower::Preprocess(const Scene *scn) {
	LightStrategy::Preprocess(scn);

	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene->dataSet->GetBSphere().rad * 1.01f;
	const float iWorldRadius2 = 1.f / (worldRadius * worldRadius);

	const u_int lightCount = scene->lightDefs.GetSize();
	float totalPower = 0.f;
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = scene->lightDefs.GetLightSource(i);

		float power = l->GetPower(*scene);
		// In order to avoid over-sampling of distant lights
		if (l->IsInfinite())
			power *= iWorldRadius2;			
		lightPower.push_back(power * l->GetImportance());
		totalPower += power;
	}

	// Build the data to power based light sampling
	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

//------------------------------------------------------------------------------
// LightStrategyLogPower
//------------------------------------------------------------------------------

void LightStrategyLogPower::Preprocess(const Scene *scn) {
	LightStrategy::Preprocess(scn);

	const u_int lightCount = scene->lightDefs.GetSize();
	float totalPower = 0.f;
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = scene->lightDefs.GetLightSource(i);

		float power = logf(1.f + l->GetPower(*scene));

		lightPower.push_back(power * l->GetImportance());
		totalPower += power;
	}

	// Build the data to power based light sampling
	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}
