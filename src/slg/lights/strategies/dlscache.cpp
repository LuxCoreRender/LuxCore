/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/lights/strategies/dlscache.h"
#include "slg/lights/strategies/dlscacheimpl.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightStrategyLogPower
//------------------------------------------------------------------------------

void LightStrategyDLSCache::Preprocess(const Scene *scn, const LightStrategyTask type,
			const bool rtMode) {
	scene = scn;
	taskType = type;
	useRTMode = rtMode;

	distributionStrategy.Preprocess(scn, taskType, rtMode);

	if ((taskType == TASK_ILLUMINATE) && !useRTMode)
		DLSCache.Build(scn);
}

LightSource *LightStrategyDLSCache::SampleLights(const float u,
			const Point &p, const Normal &n,
			const bool isVolume,
			float *pdf) const {
	if ((taskType == TASK_ILLUMINATE) && !useRTMode) {
		// Check if a cache entry is available for this point
		const DLSCacheEntry *cacheEntry = DLSCache.GetEntry(p, n, isVolume);

		if (cacheEntry) {
			if (cacheEntry->IsDirectLightSamplingDisabled())
				return NULL;
			else {
				const u_int distributionLightIndex = cacheEntry->lightsDistribution->SampleDiscrete(u, pdf);

				if (*pdf > 0.f)
					return scene->lightDefs.GetLightSources()[cacheEntry->distributionIndexToLightIndex[distributionLightIndex]];
				else
					return NULL;
			}
		} else
			return distributionStrategy.SampleLights(u, p, n, isVolume, pdf);
	} else
		return distributionStrategy.SampleLights(u, p, n, isVolume, pdf);
}

float LightStrategyDLSCache::SampleLightPdf(const LightSource *light,
		const Point &p, const Normal &n, const bool isVolume) const {
	if ((taskType == TASK_ILLUMINATE) && !useRTMode) {
		// Check if a cache entry is available for this point
		const DLSCacheEntry *cacheEntry = DLSCache.GetEntry(p, n, isVolume);

		if (cacheEntry) {
			if (cacheEntry->IsDirectLightSamplingDisabled())
				return 0.f;
			else {
				// Look for the distribution index
				// TODO: optimize the lookup
				for (u_int i = 0; i < cacheEntry->distributionIndexToLightIndex.size(); ++i) {
					if (cacheEntry->distributionIndexToLightIndex[i] == light->lightSceneIndex)
						return cacheEntry->lightsDistribution->Pdf(i);
				}
				
				return 0.f;
			}
		} else
			return distributionStrategy.SampleLightPdf(light, p, n, isVolume);
	} else
		return distributionStrategy.SampleLightPdf(light, p, n, isVolume);
}

LightSource *LightStrategyDLSCache::SampleLights(const float u,
			float *pdf) const {
	return distributionStrategy.SampleLights(u, pdf);
}

Properties LightStrategyDLSCache::ToProperties() const {
	return Properties() <<
			Property("lightstrategy.type")(LightStrategyType2String(GetType())) <<
			Property("lightstrategy.entry.radius")(DLSCache.entryRadius) <<
			Property("lightstrategy.entry.normalangle")(DLSCache.entryNormalAngle) <<
			Property("lightstrategy.entry.maxpasses")(DLSCache.maxEntryPasses) <<
			Property("lightstrategy.entry.convergencethreshold")(DLSCache.entryConvergenceThreshold) <<
			Property("lightstrategy.entry.volumes.enable")(DLSCache.entryOnVolumes) <<
			Property("lightstrategy.lightthreshold")(DLSCache.lightThreshold) <<
			Property("lightstrategy.targetcachehitratio")(DLSCache.targetCacheHitRate) <<
			Property("lightstrategy.maxdepth")(DLSCache.maxDepth) <<
			Property("lightstrategy.maxsamplescount")(DLSCache.maxSampleCount);
}

// Static methods used by LightStrategyRegistry

Properties LightStrategyDLSCache::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.type")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.radius")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.normalangle")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.maxpasses")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.convergencethreshold")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.volumes.enable")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.lightthreshold")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.targetcachehitratio")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.maxsamplescount"));
}

LightStrategy *LightStrategyDLSCache::FromProperties(const Properties &cfg) {
	LightStrategyDLSCache *ls = new LightStrategyDLSCache();

	ls->DLSCache.entryRadius = Max(0.f, cfg.Get(GetDefaultProps().Get("lightstrategy.entry.radius")).Get<float>());
	ls->DLSCache.entryNormalAngle = Max(0.f, cfg.Get(GetDefaultProps().Get("lightstrategy.entry.normalangle")).Get<float>());
	ls->DLSCache.maxEntryPasses = cfg.Get(GetDefaultProps().Get("lightstrategy.entry.maxpasses")).Get<u_int>();
	ls->DLSCache.entryConvergenceThreshold = cfg.Get(GetDefaultProps().Get("lightstrategy.entry.convergencethreshold")).Get<float>();
	ls->DLSCache.entryOnVolumes = cfg.Get(GetDefaultProps().Get("lightstrategy.entry.volumes.enable")).Get<bool>();
	ls->DLSCache.lightThreshold = cfg.Get(GetDefaultProps().Get("lightstrategy.lightthreshold")).Get<float>();
	ls->DLSCache.targetCacheHitRate = cfg.Get(GetDefaultProps().Get("lightstrategy.targetcachehitratio")).Get<float>();
	ls->DLSCache.maxDepth = cfg.Get(GetDefaultProps().Get("lightstrategy.maxdepth")).Get<u_int>();
	ls->DLSCache.maxSampleCount = cfg.Get(GetDefaultProps().Get("lightstrategy.maxsamplescount")).Get<u_int>();
	
	return ls;
}

const Properties &LightStrategyDLSCache::GetDefaultProps() {
	static Properties props = Properties() <<
			LightStrategy::GetDefaultProps() <<
			Property("lightstrategy.type")(GetObjectTag()) <<
			Property("lightstrategy.entry.radius")(.15f) <<
			Property("lightstrategy.entry.normalangle")(10.f) <<
			Property("lightstrategy.entry.maxpasses")(1024) <<
			Property("lightstrategy.entry.convergencethreshold")(.01f) <<
			Property("lightstrategy.entry.volumes.enable")(false) <<
			Property("lightstrategy.lightthreshold")(.01f) <<
			Property("lightstrategy.targetcachehitratio")(99.5f) <<
			Property("lightstrategy.maxdepth")(4) <<
			Property("lightstrategy.maxsamplescount")(10000000);

	return props;
}
