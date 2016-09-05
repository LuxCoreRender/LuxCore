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

	virtual LightStrategyType GetType() const = 0;
	virtual std::string GetTag() const = 0;

	virtual void Preprocess(const Scene *scn, const bool onlyInfiniteLights) { scene = scn; }

	LightSource *SampleLights(const float u, float *pdf) const;
	float SampleLightPdf(const LightSource *light) const;
	
	const luxrays::Distribution1D *GetLightsDistribution() const { return lightsDistribution; }

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	static LightStrategyType GetType(const luxrays::Properties &cfg);

	//--------------------------------------------------------------------------
	// Static methods used by LightStrategyRegistry
	//--------------------------------------------------------------------------

	// This method is not used at the moment
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	// Allocate a Object based on the cfg definition
	static LightStrategy *FromProperties(const luxrays::Properties &cfg);
	// This method is not used at the moment
	static std::string FromPropertiesOCL(const luxrays::Properties &cfg);

	static LightStrategyType String2LightStrategyType(const std::string &type);
	static std::string LightStrategyType2String(const LightStrategyType type);

protected:
	static const luxrays::Properties &GetDefaultProps();

	LightStrategy(const LightStrategyType t) : scene(NULL), lightsDistribution(NULL), type(t) { }

	const Scene *scene;
	luxrays::Distribution1D *lightsDistribution;

private:
	const LightStrategyType type;
};

//------------------------------------------------------------------------------
// LightStrategyUniform
//------------------------------------------------------------------------------

class LightStrategyUniform : public LightStrategy {
public:
	LightStrategyUniform() : LightStrategy(TYPE_UNIFORM) { }

	virtual void Preprocess(const Scene *scene, const bool onlyInfiniteLights);

	virtual LightStrategyType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	//--------------------------------------------------------------------------
	// Static methods used by LightStrategyRegistry
	//--------------------------------------------------------------------------

	static LightStrategyType GetObjectType() { return TYPE_UNIFORM; }
	static std::string GetObjectTag() { return "UNIFORM"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static LightStrategy *FromProperties(const luxrays::Properties &cfg);

protected:
	static const luxrays::Properties &GetDefaultProps();
};

//------------------------------------------------------------------------------
// LightStrategyPower
//------------------------------------------------------------------------------

class LightStrategyPower : public LightStrategy {
public:
	LightStrategyPower() : LightStrategy(TYPE_POWER) { }

	virtual void Preprocess(const Scene *scene, const bool onlyInfiniteLights);

	virtual LightStrategyType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	//--------------------------------------------------------------------------
	// Static methods used by LightStrategyRegistry
	//--------------------------------------------------------------------------

	static LightStrategyType GetObjectType() { return TYPE_POWER; }
	static std::string GetObjectTag() { return "POWER"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static LightStrategy *FromProperties(const luxrays::Properties &cfg);

protected:
	static const luxrays::Properties &GetDefaultProps();
};

//------------------------------------------------------------------------------
// LightStrategyLogPower
//------------------------------------------------------------------------------

class LightStrategyLogPower : public LightStrategy {
public:
	LightStrategyLogPower() : LightStrategy(TYPE_LOG_POWER) { }

	virtual void Preprocess(const Scene *scene, const bool onlyInfiniteLights);

	virtual LightStrategyType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	//--------------------------------------------------------------------------
	// Static methods used by LightStrategyRegistry
	//--------------------------------------------------------------------------

	static LightStrategyType GetObjectType() { return TYPE_LOG_POWER; }
	static std::string GetObjectTag() { return "LOG_POWER"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static LightStrategy *FromProperties(const luxrays::Properties &cfg);

protected:
	static const luxrays::Properties &GetDefaultProps();
};

}

#endif	/* _SLG_LIGHTSTRATEGY_H */
