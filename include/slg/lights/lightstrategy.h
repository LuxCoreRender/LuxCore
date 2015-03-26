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

#ifndef _SLG_LIGHTSTRATEGY_H
#define	_SLG_LIGHTSTRATEGY_H

#include <boost/unordered_map.hpp>

#include "slg/lights/light.h"

namespace slg {

//------------------------------------------------------------------------------
// LightStrategy
//------------------------------------------------------------------------------

typedef enum {
	TYPE_UNIFORM, TYPE_POWER, TYPE_LOG_POWER,
	LIGHT_STRATEGY_TYPE_COUNT
} LightStrategyType;

class LightStrategy {
public:
	virtual ~LightStrategy() { delete lightsDistribution; }

	LightStrategyType GetType() const { return type; }

	virtual void Preprocess(const Scene *scn) { scene = scn; }

	LightSource *SampleLights(const float u, float *pdf) const;
	float SampleLightPdf(const LightSource *light) const;
	
	const luxrays::Distribution1D *GetLightsDistribution() const { return lightsDistribution; }

protected:
	LightStrategy(const LightStrategyType t) : scene(NULL), lightsDistribution(NULL), type(t) { }

	const Scene *scene;
	luxrays::Distribution1D *lightsDistribution;

private:
	const LightStrategyType type;
};

class LightStrategyUniform : public LightStrategy {
public:
	LightStrategyUniform() : LightStrategy(TYPE_UNIFORM) { }

	virtual void Preprocess(const Scene *scene);
};

class LightStrategyPower : public LightStrategy {
public:
	LightStrategyPower() : LightStrategy(TYPE_POWER) { }

	virtual void Preprocess(const Scene *scene);
};

class LightStrategyLogPower : public LightStrategy {
public:
	LightStrategyLogPower() : LightStrategy(TYPE_LOG_POWER) { }

	virtual void Preprocess(const Scene *scene);
};

}

#endif	/* _SLG_LIGHTSTRATEGY_H */
