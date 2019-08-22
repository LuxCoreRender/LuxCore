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

#ifndef _SLG_LIGHTSTRATEGY_DLSCACHEIMPL_H
#define	_SLG_LIGHTSTRATEGY_DLSCACHEIMPL_H

#include <vector>

#include "luxrays/utils/mcdistribution.h"
#include "luxrays/utils/serializationutils.h"

#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"
#include "slg/scene/scene.h"
#include "slg/samplers/sampler.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// DLSCacheEntry
//------------------------------------------------------------------------------

struct DLSCVisibilityParticle {
	DLSCVisibilityParticle(const BSDF &bsdf, const PathVolumeInfo &vi) {
		p = bsdf.hitPoint.p;

		Add(bsdf, vi);
	}

	void Add(const BSDF &bsdf, const PathVolumeInfo &vi) {
		bsdfList.push_back(bsdf);
		volInfoList.push_back(vi);
	}

	void Add(const DLSCVisibilityParticle &part) {
		bsdfList.insert(bsdfList.end(), part.bsdfList.begin(), part.bsdfList.end());
		volInfoList.insert(volInfoList.end(), part.volInfoList.begin(), part.volInfoList.end());
	}

	// Field required by IndexOctree<T> class
	luxrays::Point p;

	std::vector<BSDF> bsdfList;
	std::vector<PathVolumeInfo> volInfoList;
};

//------------------------------------------------------------------------------
// DLSCacheEntry
//------------------------------------------------------------------------------

class DLSCacheEntry {
public:
	DLSCacheEntry() : lightsDistribution(nullptr) {
	}
	DLSCacheEntry(const BSDF &bsdf) {
		p = bsdf.hitPoint.p;
		n = bsdf.hitPoint.GetLandingShadeN();
		isVolume = bsdf.IsVolume();

		delete lightsDistribution;
		lightsDistribution = nullptr;
	}

	~DLSCacheEntry() {
		delete lightsDistribution;
	}

	// Point information
	luxrays::Point p;
	luxrays::Normal n;
	bool isVolume;

	// Cache information
	luxrays::Distribution1D *lightsDistribution;
};

//------------------------------------------------------------------------------
// Direct light sampling cache
//------------------------------------------------------------------------------

struct DLSCParams {
	DLSCParams() {
		entry.maxPasses = 1024;
		entry.warmUpSamples = 128;
		entry.convergenceThreshold = .01f;

		visibility.maxSampleCount = 1024 * 1024;
		visibility.maxPathDepth = 4;
		visibility.targetHitRate = .99f;
		visibility.lookUpRadius = 0.f;
		visibility.lookUpNormalAngle = 25.f;
		visibility.glossinessUsageThreshold = .05f;
	}

	struct {
		u_int maxPasses, warmUpSamples;
		float convergenceThreshold;
	} entry;

	struct {
		u_int maxSampleCount, maxPathDepth;

		float targetHitRate, lookUpRadius, lookUpNormalAngle, glossinessUsageThreshold;
	} visibility;
};

class DLSCBvh;

class DirectLightSamplingCache {
public:
	DirectLightSamplingCache(const DLSCParams &params);
	virtual ~DirectLightSamplingCache();

	bool IsCacheEnabled(const BSDF &bsdf) const;
	const DLSCParams &GetParams() const { return params; }
	const DLSCBvh *GetBVH() const { return cacheEntriesBVH; }

	void Build(const Scene *scene);
	
	const luxrays::Distribution1D *GetLightDistribution(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;

	friend class DLSCSceneVisibility;

private:
	float SampleLight(const DLSCVisibilityParticle &visibilityParticle,
		const LightSource *light, const u_int pass) const;
	
	float EvaluateBestRadius();
	void TraceVisibilityParticles();
	void BuildCacheEntry(const u_int entryIndex);
	void BuildCacheEntries();

	void DebugExport(const std::string &fileName, const float sphereRadius) const;

	DLSCParams params;

	// Used only during the building phase
	const Scene *scene;
	std::vector<DLSCVisibilityParticle> visibilityParticles;

	// Used during the rendering phase
	std::vector<DLSCacheEntry> cacheEntries;
	DLSCBvh *cacheEntriesBVH;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_DLSCACHEIMPL_H */
