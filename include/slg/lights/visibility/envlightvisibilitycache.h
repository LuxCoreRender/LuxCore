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

#ifndef _SLG_LIGHTVISIBILITYCACHE_H
#define	_SLG_LIGHTVISIBILITYCACHE_H

#include <boost/atomic.hpp>

#include "luxrays/utils/mcdistribution.h"
#include "luxrays/utils/serializationutils.h"

#include "slg/slg.h"
#include "slg/core/indexoctree.h"
#include "slg/core/indexbvh.h"
#include "slg/scene/scene.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/metropolis.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/lights/visibility/elvc_types.cl"
}

//------------------------------------------------------------------------------
// Env. Light visibility cache
//------------------------------------------------------------------------------

struct ELVCVisibilityParticle {
	ELVCVisibilityParticle(const BSDF &bsdf, const PathVolumeInfo &vi) {
		p = bsdf.hitPoint.p;

		Add(bsdf, vi);
	}

	void Add(const BSDF &bsdf, const PathVolumeInfo &vi) {
		bsdfList.push_back(bsdf);
		volInfoList.push_back(vi);
	}

	void Add(const ELVCVisibilityParticle &part) {
		bsdfList.insert(bsdfList.end(), part.bsdfList.begin(), part.bsdfList.end());
		volInfoList.insert(volInfoList.end(), part.volInfoList.begin(), part.volInfoList.end());
	}

	// Field required by IndexOctree<T> class
	luxrays::Point p;

	std::vector<BSDF> bsdfList;
	std::vector<PathVolumeInfo> volInfoList;
};

class ELVCOctree : public IndexOctree<ELVCVisibilityParticle> {
public:
	ELVCOctree(const std::vector<ELVCVisibilityParticle> &allEntries, const luxrays::BBox &bbox,
			const float radius, const float normAngle, const u_int md = 24);
	virtual ~ELVCOctree();

	u_int GetNearestEntry(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;

private:
	void GetNearestEntryImpl(const IndexOctreeNode *node, const luxrays::BBox &nodeBBox,
			const luxrays::Point &p, const luxrays::Normal &n, const bool isVolume,
			u_int &nearestEntryIndex, float &nearestDistance2) const;
};

class ELVCacheEntry {
public:
	ELVCacheEntry() : visibilityMap(nullptr) {
	}
	ELVCacheEntry(const luxrays::Point &pt, const luxrays::Normal &nm,
		const bool isVol, luxrays::Distribution2D *vm) :
			p(pt), n(nm), isVolume(isVol), visibilityMap(vm) {
	}
	
	~ELVCacheEntry() {
		delete visibilityMap;
	}

	// Point information
	luxrays::Point p;
	luxrays::Normal n;
	bool isVolume;

	// Cache information
	luxrays::Distribution2D *visibilityMap;
	
	friend class boost::serialization::access;
	
protected:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & p;
		ar & n;
		ar & isVolume;
		ar & visibilityMap;
	}
};

class ELVCBvh : public IndexBvh<ELVCacheEntry> {
public:
	ELVCBvh(const std::vector<ELVCacheEntry> *entries,
			const float radius, const float normalAngle);
	virtual ~ELVCBvh();

	const ELVCacheEntry *GetNearestEntry(const luxrays::Point &p,
			const luxrays::Normal &n, const bool isVolume) const;

	// Used for OpenCL data translation
	const std::vector<ELVCacheEntry> *GetAllEntries() const { return allEntries; }

	friend class boost::serialization::access;

private:
	// Used by serialization
	ELVCBvh() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(IndexBvh);
		ar & normalCosAngle;
	}

	float normalCosAngle;
};

struct ELVCParams {
	ELVCParams() {
		map.quality = 0.5;
		map.tileWidth = 0;
		map.tileHeight = 0;
		map.tileSampleCount = 0;
		map.sampleUpperHemisphereOnly = false;

		visibility.maxSampleCount = 1024 * 1024;
		visibility.maxPathDepth = 4;
		visibility.targetHitRate = .99f;
		visibility.lookUpRadius = 0.f;
		visibility.lookUpNormalAngle = 25.f;

		persistent.fileName = "";
		persistent.safeSave = true;
	}

	struct {
		float quality; // A value between 0.0 and 1.0
		u_int tileWidth, tileHeight;
		u_int tileSampleCount;

		bool sampleUpperHemisphereOnly;
	} map;

	struct {
		u_int maxSampleCount, maxPathDepth;

		float targetHitRate, lookUpRadius, lookUpNormalAngle;
	} visibility;

	struct {
		std::string fileName;
		bool safeSave;
	} persistent;

	friend class boost::serialization::access;
	
protected:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & map.quality;
		ar & map.tileWidth;
		ar & map.tileHeight;
		ar & map.tileSampleCount;
		ar & map.sampleUpperHemisphereOnly;

		ar & visibility.maxSampleCount;
		ar & visibility.maxPathDepth;
		ar & visibility.targetHitRate;
		ar & visibility.lookUpRadius;
		ar & visibility.lookUpNormalAngle;

		ar & persistent.fileName;
		ar & persistent.safeSave;
	}
};

class ELVCSceneVisibility;

class EnvLightVisibilityCache {
public:
	EnvLightVisibilityCache(const Scene *scene, const EnvLightSource *envLight,
			ImageMap *luminanceMapImage,
			const ELVCParams &params);
	EnvLightVisibilityCache(const Scene *scene, const EnvLightSource *envLight,
			const u_int mapWidth, const u_int mapHeight, const ELVCParams &params);
	virtual ~EnvLightVisibilityCache();

	bool IsCacheEnabled(const BSDF &bsdf) const;
	const ELVCParams &GetParams() const { return params; }
	const ELVCBvh *GetBVH() const { return cacheEntriesBVH; }
	const u_int GetXTileCount() const { return tilesXCount; }
	const u_int GetYTileCount() const { return tilesYCount; }
	bool HasTileDistributions() const { return (tileDistributions.size() > 0);}
	const luxrays::Distribution2D *GetTileDistribution(const u_int index) const {
		return tileDistributions[index];
	}

	void Build();

	void Sample(const BSDF &bsdf, const float u0, const float u1,
			float uv[2], float *pdf) const;
	float Pdf(const BSDF &bsdf, const float u, const float v) const;

	static ELVCParams Properties2Params(const std::string &prefix, const luxrays::Properties props);
	static luxrays::Properties Params2Props(const std::string &prefix, const ELVCParams &params);

	static const u_int defaultLuminanceMapWidth, defaultLuminanceMapHeight;
	
	friend class ELVCSceneVisibility;

private:
	void ParamsEvaluation();

	float EvaluateBestRadius();
	void TraceVisibilityParticles();
	void BuildCacheEntry(const u_int entryIndex, const ImageMap *luminanceMapImageScaled);
	void BuildCacheEntries();
	void BuildTileDistributions();

	const luxrays::Distribution2D *GetVisibilityMap(const BSDF &bsdf) const;

	void LoadPersistentCache(const std::string &fileName);
	void SavePersistentCache(const std::string &fileName);

	const Scene *scene;
	const EnvLightSource *envLight;
	const ImageMap *luminanceMapImage;

	ELVCParams params;

	// Used only during the building phase
	std::vector<ELVCVisibilityParticle> visibilityParticles;

	// Used during the rendering phase
	std::vector<ELVCacheEntry> cacheEntries;
	ELVCBvh *cacheEntriesBVH;
	u_int mapWidth, mapHeight;
	u_int tilesXCount, tilesYCount;
	std::vector<luxrays::Distribution2D *> tileDistributions;
};

}

BOOST_CLASS_VERSION(slg::ELVCacheEntry, 1)
BOOST_CLASS_VERSION(slg::ELVCBvh, 1)
BOOST_CLASS_VERSION(slg::ELVCParams, 4)

BOOST_CLASS_EXPORT_KEY(slg::ELVCacheEntry)
BOOST_CLASS_EXPORT_KEY(slg::ELVCBvh)
BOOST_CLASS_EXPORT_KEY(slg::ELVCParams)

#endif	/* _SLG_LIGHTVISIBILITYCACHE_H */
