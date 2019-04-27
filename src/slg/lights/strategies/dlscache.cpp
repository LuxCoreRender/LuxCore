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
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightStrategyDLSCache
//------------------------------------------------------------------------------

LightStrategyDLSCache::LightStrategyDLSCache(const DirectLightSamplingCacheParams &params) : LightStrategy(TYPE_DLS_CACHE) {
	DLSCache.SetParams(params);
}

LightStrategyDLSCache::~LightStrategyDLSCache() {
}

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
	const DirectLightSamplingCacheParams &params = DLSCache.GetParams();

	return Properties() <<
			Property("lightstrategy.type")(LightStrategyType2String(GetType())) <<
			Property("lightstrategy.entry.radius")(params.entry.radius) <<
			Property("lightstrategy.entry.normalangle")(params.entry.normalAngle) <<
			Property("lightstrategy.entry.maxpasses")(params.entry.maxPasses) <<
			Property("lightstrategy.entry.convergencethreshold")(params.entry.convergenceThreshold) <<
			Property("lightstrategy.entry.warmupsamples")(params.entry.warmUpSamples) <<
			Property("lightstrategy.entry.volumes.enable")(params.entry.enabledOnVolumes) <<
			Property("lightstrategy.lightthreshold")(params.lightThreshold) <<
			Property("lightstrategy.targetcachehitratio")(params.targetCacheHitRate) <<
			Property("lightstrategy.maxdepth")(params.maxDepth) <<
			Property("lightstrategy.maxsamplescount")(params.maxSampleCount);
}

// Static methods used by LightStrategyRegistry

Properties LightStrategyDLSCache::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.type")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.radius")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.normalangle")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.maxpasses")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.convergencethreshold")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.warmupsamples")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.entry.volumes.enable")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.lightthreshold")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.targetcachehitratio")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("lightstrategy.maxsamplescount"));
}

LightStrategy *LightStrategyDLSCache::FromProperties(const Properties &cfg) {
	DirectLightSamplingCacheParams params;

	params.entry.radius = Max(0.f, cfg.Get(GetDefaultProps().Get("lightstrategy.entry.radius")).Get<float>());
	params.entry.normalAngle = Max(0.f, cfg.Get(GetDefaultProps().Get("lightstrategy.entry.normalangle")).Get<float>());
	params.entry.maxPasses = cfg.Get(GetDefaultProps().Get("lightstrategy.entry.maxpasses")).Get<u_int>();
	params.entry.convergenceThreshold = Clamp(cfg.Get(GetDefaultProps().Get("lightstrategy.entry.convergencethreshold")).Get<float>(), 0.f, 1.f);
	params.entry.warmUpSamples = Max<u_int>(1, cfg.Get(GetDefaultProps().Get("lightstrategy.entry.warmupsamples")).Get<u_int>());
	params.entry.mergePasses = Max<u_int>(1, cfg.Get(GetDefaultProps().Get("lightstrategy.entry.mergepasses")).Get<u_int>());
	params.entry.enabledOnVolumes = cfg.Get(GetDefaultProps().Get("lightstrategy.entry.volumes.enable")).Get<bool>();
	params.lightThreshold = Clamp(cfg.Get(GetDefaultProps().Get("lightstrategy.lightthreshold")).Get<float>(), 0.f, 1.f);
	params.targetCacheHitRate = Clamp(cfg.Get(GetDefaultProps().Get("lightstrategy.targetcachehitratio")).Get<float>(), 0.f, 1.f);
	params.maxDepth = cfg.Get(GetDefaultProps().Get("lightstrategy.maxdepth")).Get<u_int>();
	params.maxSampleCount = cfg.Get(GetDefaultProps().Get("lightstrategy.maxsamplescount")).Get<u_int>();

	return new LightStrategyDLSCache(params);
}

const Properties &LightStrategyDLSCache::GetDefaultProps() {
	static Properties props = Properties() <<
			LightStrategy::GetDefaultProps() <<
			Property("lightstrategy.type")(GetObjectTag()) <<
			Property("lightstrategy.entry.radius")(0.f) <<
			Property("lightstrategy.entry.normalangle")(10.f) <<
			Property("lightstrategy.entry.maxpasses")(1024) <<
			Property("lightstrategy.entry.convergencethreshold")(.01f) <<
			Property("lightstrategy.entry.warmupsamples")(12) <<
			Property("lightstrategy.entry.mergepasses")(1) <<
			Property("lightstrategy.entry.volumes.enable")(false) <<
			Property("lightstrategy.lightthreshold")(.01f) <<
			Property("lightstrategy.targetcachehitratio")(.995f) <<
			Property("lightstrategy.maxdepth")(4) <<
			Property("lightstrategy.maxsamplescount")(10000000);

	return props;
}
