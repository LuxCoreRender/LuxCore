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

class DLSCacheEntry {
public:
	DLSCacheEntry();
    // Move constructor, takes a rvalue reference &&
    DLSCacheEntry(DLSCacheEntry &&other);
	~DLSCacheEntry();

	// Move assignment, takes a rvalue reference &&
    DLSCacheEntry &operator=(DLSCacheEntry &&other);
	
	void Init(const BSDF &bsdf, const PathVolumeInfo &vi);
	
	bool IsDirectLightSamplingDisabled() const {
		return (lightsDistribution == nullptr);
	}

	void AddSamplingPoint(const BSDF &bsdf, const PathVolumeInfo &vi);

	// Point information
	luxrays::Point p;
	luxrays::Normal n;
	bool isVolume;
	
	// Cache information
	std::vector<u_int> distributionIndexToLightIndex;
	luxrays::Distribution1D *lightsDistribution;

	friend class DirectLightSamplingCache;
	
private:
	typedef struct {
		std::vector<BSDF> bsdfList;
		std::vector<PathVolumeInfo> volInfoList;

		std::vector<float> lightReceivedLuminance;
		std::vector<u_int> distributionIndexToLightIndex;
		
		std::vector<float> mergedLightReceivedLuminance;
		std::vector<u_int> mergedDistributionIndexToLightIndex;

		bool isTransparent;
	} TemporayInformation;

	// Prevent copy constructor to be used
	DLSCacheEntry(const DLSCacheEntry &);
	// Prevent copy assignment to be used
    DLSCacheEntry &operator=(const DLSCacheEntry &);

	void DeleteTmpInfo() {
		delete tmpInfo;
		tmpInfo = nullptr;
	}

	TemporayInformation *tmpInfo;
};

//------------------------------------------------------------------------------
// Direct light sampling cache
//------------------------------------------------------------------------------

typedef struct {
	struct {
		float radius, normalAngle, convergenceThreshold;
		u_int maxPasses, warmUpSamples, mergePasses;
		
		bool enabledOnVolumes;
	} entry;

	u_int maxSampleCount, maxDepth, maxEntryPasses;
	float targetCacheHitRate, lightThreshold;
} DirectLightSamplingCacheParams;

class DLSCOctree;
class DLSCBvh;

class DirectLightSamplingCache {
public:
	DirectLightSamplingCache();
	virtual ~DirectLightSamplingCache();

	void SetParams(const DirectLightSamplingCacheParams &p) { params = p; }
	const DirectLightSamplingCacheParams &GetParams() const { return params; }
	bool IsDLSCEnabled(const BSDF &bsdf) const;

	void Build(const Scene *scene);
	
	const DLSCacheEntry *GetEntry(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;

	// Used for OpenCL data translation
	const DLSCBvh *GetBVH() const { return bvh; }

private:
	void GenerateEyeRay(const Camera *camera, luxrays::Ray &eyeRay,
			PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const;
	float SampleLight(const Scene *scene, DLSCacheEntry *entry,
		const LightSource *light, const u_int pass) const;
	
	float EvaluateBestRadius(const Scene *scene);
	void BuildCacheEntries(const Scene *scene);
	void FillCacheEntry(const Scene *scene, DLSCacheEntry *entry);
	void FillCacheEntries(const Scene *scene);
	void MergeCacheEntry(const Scene *scene, const u_int entryIndex);
	void FinalizedMergeCacheEntry(const u_int entryIndex);
	void MergeCacheEntries(const Scene *scene);
	void InitDistributionEntry(const Scene *scene, DLSCacheEntry *entry);
	void InitDistributionEntries(const Scene *scene);
	void BuildBVH(const Scene *scene);

	void DebugExport(const std::string &fileName, const float sphereRadius) const;

	DirectLightSamplingCacheParams params;

	std::vector<DLSCacheEntry> allEntries;

	// Used only during the building phase
	DLSCOctree *octree;
	// Used during the rendering phase
	DLSCBvh *bvh;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_DLSCACHEIMPL_H */
