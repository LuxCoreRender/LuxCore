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

#ifndef _SLG_LIGHTSTRATEGY_DLSCACHE_H
#define	_SLG_LIGHTSTRATEGY_DLSCACHE_H

#include "slg/slg.h"
#include "slg/lights/strategies/lightstrategy.h"
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
#include "slg/lights/strategies/logpower.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/lights/strategies/dlsc_types.cl"
}

//------------------------------------------------------------------------------
// LightStrategyDLSCache
//------------------------------------------------------------------------------

class LightStrategyDLSCache : public LightStrategy {
public:
	LightStrategyDLSCache(const DLSCParams &params);
	virtual ~LightStrategyDLSCache();

	virtual void Preprocess(SceneConstRef scene, const LightStrategyTask taskType,
			const bool useRTMode);
	
	// Used for direct light sampling
	virtual LightSourcePtr SampleLights(
			SceneConstRef scene,
			const float u,
			const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume,
			float *pdf) const;

	virtual float SampleLightPdf(LightSourceConstRef light,
			const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;

	// Used for light emission
	virtual LightSourcePtr SampleLights(
		SceneConstRef scene, const float u, float *pdf
	) const;

	virtual LightStrategyType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual luxrays::PropertiesUPtr ToProperties() const;

	// Used for OpenCL data translation
	const luxrays::Distribution1DRPtr GetLightsDistribution() const { return distributionStrategy->GetLightsDistribution(); }
	DLSCBvhConstPtr GetBVH() const { return DLSCache.GetBVH(); }
	bool UseRTMode() const { return useRTMode; }
	float GetEntryRadius() const { return DLSCache.GetParams().visibility.lookUpRadius; }
	float GetEntryNormalAngle() const { return DLSCache.GetParams().visibility.lookUpNormalAngle; }

	//--------------------------------------------------------------------------
	// Static methods used by LightStrategyRegistry
	//--------------------------------------------------------------------------

	static LightStrategyType GetObjectType() { return TYPE_DLS_CACHE; }
	static std::string GetObjectTag() { return "DLS_CACHE"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static LightStrategyUPtr FromProperties(const luxrays::Properties &cfg);

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();

	LightStrategyTask taskType;
	LightStrategyLogPowerUPtr distributionStrategy = std::make_unique<LightStrategyLogPower>();
	DirectLightSamplingCache DLSCache;

	bool useRTMode;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_DLSCACHE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
