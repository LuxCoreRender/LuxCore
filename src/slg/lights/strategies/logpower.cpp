/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/utils/properties.h"
#include "slg/scene/scene.h"
#include <memory>

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightStrategyLogPower
//------------------------------------------------------------------------------

void LightStrategyLogPower::Preprocess(SceneConstRef scene, const LightStrategyTask taskType,
			const bool useRTMode) {
	// Delete old lightsDistribution
	lightsDistribution = nullptr;

	DistributionLightStrategy::Preprocess(scene, taskType);

	const u_int lightCount = scene.GetLightSources().GetSize();
	if (lightCount == 0)
		return;

	vector<float> lightPower;
	lightPower.reserve(lightCount);

	for (u_int i = 0; i < lightCount; ++i) {
		auto& l = scene.GetLightSources().GetLightSource(i);
		const float power = logf(1.f + l.GetPower(scene)) * l.GetImportance();

		switch (taskType) {
			case TASK_EMIT: {
				lightPower.push_back(power);
				break;
			}
			case TASK_ILLUMINATE: {
				if (l.IsDirectLightSamplingEnabled()){
					lightPower.push_back(power);
				} else
					lightPower.push_back(0.f);
				break;
			}
			case TASK_INFINITE_ONLY: {
				if (l.IsInfinite())
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
	lightsDistribution = std::make_unique<Distribution1D>(lightPower);
}

// Static methods used by LightStrategyRegistry

PropertiesUPtr LightStrategyLogPower::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = std::make_unique<Properties>();
	
	*props <<
				cfg.Get(GetDefaultProps()->Get("lightstrategy.type"));
	
	return props;
}

LightStrategyUPtr LightStrategyLogPower::FromProperties(const Properties &cfg) {
	return std::make_unique<LightStrategyLogPower>();
}

PropertiesUPtr LightStrategyLogPower::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			LightStrategy::GetDefaultProps() <<
			Property("lightstrategy.type")(GetObjectTag());

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
