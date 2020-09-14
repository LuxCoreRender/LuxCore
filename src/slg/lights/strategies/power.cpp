/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include "slg/lights/strategies/power.h"
#include "slg/scene/scene.h"
#include "slg/samplers/random.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightStrategyPower
//------------------------------------------------------------------------------

void LightStrategyPower::Preprocess(const Scene *scn, const LightStrategyTask taskType,
			const bool useRTMode) {
	DistributionLightStrategy::Preprocess(scn, taskType);

	const u_int lightCount = scene->lightDefs.GetSize();
	if (lightCount == 0)
		return;

	const float envRadius = InfiniteLightSource::GetEnvRadius(*scene);
	const float invEnvRadius2 = 1.f / (envRadius * envRadius);

	vector<float> lightPower;
	lightPower.reserve(lightCount);

	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();
	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = lights[i];
		float power = l->GetPower(*scene) * l->GetImportance();
		// In order to avoid over-sampling of distant lights
		if (l->IsInfinite())
			power *= invEnvRadius2;

		switch (taskType) {
			case TASK_EMIT: {
				lightPower.push_back(power);
				break;
			}
			case TASK_ILLUMINATE: {
				if (l->IsDirectLightSamplingEnabled())
					lightPower.push_back(power);
				else
					lightPower.push_back(0.f);
				break;
			}
			case TASK_INFINITE_ONLY: {
				if (l->IsInfinite())
					lightPower.push_back(power);
				else
					lightPower.push_back(0.f);
				break;
			}
			default:
				throw runtime_error("Unknown task in LightStrategyPower::Preprocess(): " + ToString(taskType));
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
