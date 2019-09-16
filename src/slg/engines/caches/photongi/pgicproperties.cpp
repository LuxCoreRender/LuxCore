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

#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PhotonGICache properties related methods
//------------------------------------------------------------------------------

Properties PhotonGICache::ToProperties(const Properties &cfg) {
	Properties props;

	props <<
			cfg.Get(GetDefaultProps().Get("path.photongi.sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxcount")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.photon.time.start")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.photon.time.end")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.visibility.lookup.radius")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.visibility.lookup.normalangle")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.visibility.targethitrate")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.visibility.maxsamplecount")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.glossinessusagethreshold")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.haltthreshold")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.lookup.radius")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.lookup.normalangle")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.usagethresholdscale")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.filter.radiusscale")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.updatespp")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.lookup.maxcount")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.lookup.radius")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.lookup.normalangle")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.merge.radiusscale")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.debug.type")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.persistent.file")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.persistent.safesave"));

	return props;
}

const Properties &PhotonGICache::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.photongi.sampler.type")("METROPOLIS") <<
			Property("path.photongi.photon.maxcount")(100000000) <<
			Property("path.photongi.photon.maxdepth")(4) <<
			// Time start/end are used only if time.end > time.start
			Property("path.photongi.photon.time.start")(0.0) <<
			Property("path.photongi.photon.time.end")(-1.0) <<
			Property("path.photongi.visibility.lookup.radius")(0.f) <<
			Property("path.photongi.visibility.lookup.normalangle")(10.f) <<
			Property("path.photongi.visibility.targethitrate")(.99f) <<
			Property("path.photongi.visibility.maxsamplecount")(1024 * 1024) <<
			Property("path.photongi.glossinessusagethreshold")(.05f) <<
			Property("path.photongi.indirect.enabled")(false) <<
			Property("path.photongi.indirect.maxsize")(0) <<
			Property("path.photongi.indirect.haltthreshold")(.05f) <<
			Property("path.photongi.indirect.lookup.radius")(0.f) <<
			Property("path.photongi.indirect.lookup.normalangle")(10.f) <<
			Property("path.photongi.indirect.usagethresholdscale")(8.f) <<
			Property("path.photongi.indirect.filter.radiusscale")(3.f) <<
			Property("path.photongi.caustic.enabled")(false) <<
			Property("path.photongi.caustic.maxsize")(1000000) <<
			Property("path.photongi.caustic.updatespp")(32) <<
			Property("path.photongi.caustic.lookup.maxcount")(128) <<
			Property("path.photongi.caustic.lookup.radius")(.15f) <<
			Property("path.photongi.caustic.lookup.normalangle")(10.f) <<
			Property("path.photongi.caustic.merge.radiusscale")(.25f) <<
			Property("path.photongi.debug.type")("none") <<
			Property("path.photongi.persistent.file")("") <<
			Property("path.photongi.persistent.safesave")(true);

	return props;
}

PhotonGICache *PhotonGICache::FromProperties(const Scene *scn, const Properties &cfg) {
	PhotonGICacheParams params;

	params.indirect.enabled = cfg.Get(GetDefaultProps().Get("path.photongi.indirect.enabled")).Get<bool>();
	params.caustic.enabled = cfg.Get(GetDefaultProps().Get("path.photongi.caustic.enabled")).Get<bool>();
	
	if (params.indirect.enabled || params.caustic.enabled) {
		params.samplerType = String2SamplerType(cfg.Get(GetDefaultProps().Get("path.photongi.sampler.type")).Get<string>());

		params.photon.maxTracedCount = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxcount")).Get<u_int>());
		params.photon.maxPathDepth = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxdepth")).Get<u_int>());
		params.photon.timeStart = cfg.Get(GetDefaultProps().Get("path.photongi.photon.time.start")).Get<float>();
		params.photon.timeEnd = cfg.Get(GetDefaultProps().Get("path.photongi.photon.time.end")).Get<float>();

		params.visibility.lookUpRadius = Max(0.f, cfg.Get(GetDefaultProps().Get("path.photongi.visibility.lookup.radius")).Get<float>());
		params.visibility.lookUpNormalAngle = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.visibility.lookup.normalangle")).Get<float>());
		params.visibility.targetHitRate = cfg.Get(GetDefaultProps().Get("path.photongi.visibility.targethitrate")).Get<float>();
		params.visibility.maxSampleCount = cfg.Get(GetDefaultProps().Get("path.photongi.visibility.maxsamplecount")).Get<u_int>();

		params.glossinessUsageThreshold = Max(0.f, cfg.Get(GetDefaultProps().Get("path.photongi.glossinessusagethreshold")).Get<float>());

		if (params.indirect.enabled) {
			params.indirect.maxSize = Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.maxsize")).Get<u_int>());
			params.indirect.haltThreshold = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.haltthreshold")).Get<float>());

			params.indirect.lookUpRadius = Max(0.f, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.lookup.radius")).Get<float>());
			params.indirect.lookUpNormalAngle = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.lookup.normalangle")).Get<float>());

			params.indirect.usageThresholdScale = Max(0.f, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.usagethresholdscale")).Get<float>());

			params.indirect.filterRadiusScale = Max(1.f, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.filter.radiusscale")).Get<float>());
		}

		if (params.caustic.enabled) {
			params.caustic.maxSize = Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.maxsize")).Get<u_int>());
			params.caustic.updateSpp = Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.updatespp")).Get<u_int>());

			params.caustic.lookUpMaxCount = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.lookup.maxcount")).Get<u_int>());
			params.caustic.lookUpRadius = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.lookup.radius")).Get<float>());
			params.caustic.lookUpNormalAngle = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.lookup.normalangle")).Get<float>());

			params.caustic.mergeRadiusScale = Max(0.f, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.merge.radiusscale")).Get<float>());
		}

		params.debugType = String2DebugType(cfg.Get(GetDefaultProps().Get("path.photongi.debug.type")).Get<string>());

		params.persistent.fileName = cfg.Get(GetDefaultProps().Get("path.photongi.persistent.file")).Get<string>();
		params.persistent.safeSave = cfg.Get(GetDefaultProps().Get("path.photongi.persistent.safesave")).Get<bool>();

		return new PhotonGICache(scn, params);
	} else
		return nullptr;
}
