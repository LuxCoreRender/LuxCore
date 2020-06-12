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

#include "slg/lights/strategies/logpower.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightStrategyLogPower
//------------------------------------------------------------------------------

void LightStrategyLogPower::Preprocess(const Scene *scn, const LightStrategyTask taskType,
			const bool useRTMode) {
	DistributionLightStrategy::Preprocess(scn, taskType);

	const u_int lightCount = scene->lightDefs.GetSize();
	if (lightCount == 0)
		return;

	vector<float> lightPower;
	lightPower.reserve(lightCount);

	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();
	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = lights[i];
		const float power = logf(1.f + l->GetPower(*scene)) * l->GetImportance();

		switch (taskType) {
			case TASK_EMIT: {
				lightPower.push_back(power);
				break;
			}
			case TASK_ILLUMINATE: {
				if (l->IsDirectLightSamplingEnabled()){
					lightPower.push_back(power);
				} else
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
				throw runtime_error("Unknown task in LightStrategyLogPower::Preprocess(): " + ToString(taskType));
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
	return new LightStrategyLogPower();
}

const Properties &LightStrategyLogPower::GetDefaultProps() {
	static Properties props = Properties() <<
			LightStrategy::GetDefaultProps() <<
			Property("lightstrategy.type")(GetObjectTag());

	return props;
}
