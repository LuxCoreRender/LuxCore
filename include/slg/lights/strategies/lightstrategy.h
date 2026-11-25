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

#ifndef _SLG_LIGHTSTRATEGY_H
#define	_SLG_LIGHTSTRATEGY_H

#include "slg/usings.h"
#include "slg/lights/light.h"
#include "slg/scene/scene.h"

namespace slg {

//------------------------------------------------------------------------------
// LightStrategy
//------------------------------------------------------------------------------

typedef enum {
	TASK_EMIT, TASK_ILLUMINATE, TASK_INFINITE_ONLY,
	LIGHT_STRATEGY_TASK_COUNT
} LightStrategyTask;

typedef enum {
	TYPE_UNIFORM, TYPE_POWER, TYPE_LOG_POWER, TYPE_DLS_CACHE,
	LIGHT_STRATEGY_TYPE_COUNT
} LightStrategyType;


class LightStrategy {
public:
	virtual ~LightStrategy() { }

	virtual LightStrategyType GetType() const = 0;
	virtual std::string GetTag() const = 0;

	virtual void Preprocess(SceneConstRef scn, const LightStrategyTask taskType,
			const bool useRTMode) = 0;

	// Used for direct light sampling
	virtual LightSourcePtr SampleLights(
			SceneConstRef scene,
			const float u,
			const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume,
			float *pdf) const = 0;

	virtual float SampleLightPdf(
			LightSourceConstRef light,
			const luxrays::Point &p,
			const luxrays::Normal &n,
			const bool isVolume) const = 0;

	// Used for light emission
	virtual LightSourcePtr SampleLights(
		SceneConstRef, const float u, float *pdf
	) const = 0;

	// Transform the current object in Properties
	virtual luxrays::PropertiesUPtr ToProperties() const = 0;

	static LightStrategyType GetType(const luxrays::Properties &cfg);

	//--------------------------------------------------------------------------
	// Static methods used by LightStrategyRegistry
	//--------------------------------------------------------------------------

	// This method is not used at the moment
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	// Allocate a Object based on the cfg definition
	static LightStrategyUPtr FromProperties(const luxrays::Properties &cfg);
	// This method is not used at the moment
	static std::string FromPropertiesOCL(const luxrays::Properties &cfg);

	static LightStrategyType String2LightStrategyType(const std::string &type);
	static std::string LightStrategyType2String(const LightStrategyType type);

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();

	LightStrategy(const LightStrategyType t) : type(t) { }

	SceneConstPtr scene;  // I think this could be a (mandatory) reference but
									 // for now, I keep it as a (optional) pointer

private:
	const LightStrategyType type;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
