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

#ifndef _SLG_LIGHTVISIBILITYCACHE_H
#define	_SLG_LIGHTVISIBILITYCACHE_H

#include <boost/atomic.hpp>

#include "luxrays/utils/mcdistribution.h"
#include "slg/slg.h"
#include "slg/core/indexoctree.h"
#include "slg/core/indexbvh.h"
#include "slg/scene/scene.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/metropolis.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// Env. Light visibility cache
//------------------------------------------------------------------------------

struct ELVCVisibilityParticle {
	ELVCVisibilityParticle(const luxrays::Point &pt, const PathVolumeInfo &vi) :
		p(pt), volInfo(vi) {
		Add(pt, vi);
	}

	void Add(const luxrays::Point &pt, const PathVolumeInfo &vi) {
		pList.push_back(pt);
		volInfoList.push_back(vi);
	}

	luxrays::Point p;
	PathVolumeInfo volInfo;
	
	std::vector<luxrays::Point> pList;
	std::vector<PathVolumeInfo> volInfoList;
};

class ELVCOctree : public IndexOctree<ELVCVisibilityParticle> {
public:
	ELVCOctree(const std::vector<ELVCVisibilityParticle> &allEntries, const luxrays::BBox &bbox,
			const float r, const u_int md = 24);
	virtual ~ELVCOctree();

	u_int GetNearestEntry(const luxrays::Point &p) const;

private:
	void GetNearestEntryImpl(const IndexOctreeNode *node, const luxrays::BBox &nodeBBox,
			const luxrays::Point &p, u_int &nearestEntryIndex, float &nearestDistance2) const;
};

struct ELVCCacheEntry {
	ELVCCacheEntry() : visibilityMap(nullptr) {
	}
	ELVCCacheEntry(const luxrays::Point &pt, const bool isVol, luxrays::Distribution2D *vm) :
			p(pt), visibilityMap(vm) {
	}
	
	~ELVCCacheEntry() {
		delete visibilityMap;
	}

	luxrays::Point p;
	luxrays::Distribution2D *visibilityMap;
};

class ELVCBvh : public IndexBvh<ELVCCacheEntry> {
public:
	ELVCBvh(const std::vector<ELVCCacheEntry> *entries,
			const float radius);
	virtual ~ELVCBvh();

	const ELVCCacheEntry *GetNearestEntry(const luxrays::Point &p) const;
};

typedef struct {
	u_int width, height;

	u_int maxSampleCount;
	u_int maxPathDepth;

	float targetHitRate;
	float lookUpRadius;

	float glossinessUsageThreshold;

	bool sampleUpperHemisphereOnly;
} ELVCParams;

class ELVCSceneVisibility;

class EnvLightVisibilityCache {
public:
	EnvLightVisibilityCache(const Scene *scene, const EnvLightSource *envLight,
			ImageMap *luminanceMapImage,
			const ELVCParams &params);
	virtual ~EnvLightVisibilityCache();

	bool IsCacheEnabled(const BSDF &bsdf) const;

	void Build();

	const luxrays::Distribution2D *GetVisibilityMap(const luxrays::Point &p) const;

	friend class ELVCSceneVisibility;

private:
	float EvaluateBestRadius();
	void TraceVisibilityParticles();
	void BuildCacheEntry(const u_int entryIndex);
	void BuildCacheEntries();

	const Scene *scene;
	const EnvLightSource *envLight;
	const ImageMap *luminanceMapImage;

	ELVCParams params;

	std::vector<ELVCVisibilityParticle> visibilityParticles;
	std::vector<ELVCCacheEntry> cacheEntries;
	ELVCBvh *cacheEntriesBVH;
};

}

#endif	/* _SLG_LIGHTVISIBILITYCACHE_H */
