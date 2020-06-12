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

#ifndef _SLG_DISTRIBUTION_LIGHTSTRATEGY_H
#define	_SLG_DISTRIBUTION_LIGHTSTRATEGY_H

#include "slg/lights/strategies/lightstrategy.h"

namespace slg {

//------------------------------------------------------------------------------
// DistributionLightStrategy
//------------------------------------------------------------------------------

class DistributionLightStrategy : public LightStrategy {
public:
	virtual ~DistributionLightStrategy() { delete lightsDistribution; }

	virtual void Preprocess(const Scene *scn, const LightStrategyTask taskType) { scene = scn; }

	// Used for direct light sampling
	virtual LightSource *SampleLights(const float u,
			const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume,
			float *pdf) const;
	virtual float SampleLightPdf(const LightSource *light,
			const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;

	// Used for light emission
	virtual LightSource *SampleLights(const float u, float *pdf) const;
	
	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;
	
	const luxrays::Distribution1D *GetLightsDistribution() const { return lightsDistribution; }
	
protected:
	DistributionLightStrategy(const LightStrategyType t) : LightStrategy(t), lightsDistribution(nullptr) { }

	luxrays::Distribution1D *lightsDistribution;
};

}

#endif	/* _SLG_DISTRIBUTION_LIGHTSTRATEGY_H */
