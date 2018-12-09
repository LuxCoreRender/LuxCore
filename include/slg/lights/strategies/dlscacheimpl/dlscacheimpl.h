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
#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"
#include "slg/scene/scene.h"
#include "slg/samplers/sampler.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// DLSCachePoint
//------------------------------------------------------------------------------

class DLSCachePoint {
public:
	DLSCachePoint(const luxrays::Point &pnt, const luxrays::Normal &nml,
			const PathVolumeInfo &vi) : p(pnt), n(nml) { }

	luxrays::Point p;
	luxrays::Normal n;

	PathVolumeInfo volInfo;
};

//------------------------------------------------------------------------------
// DLSCacheEntry
//------------------------------------------------------------------------------

class DLSCacheEntry {
public:
	DLSCacheEntry(const luxrays::Point &pnt, const luxrays::Normal &nml,
			const bool isVol, const bool isTrans, const PathVolumeInfo &vi);
	~DLSCacheEntry();
	
	bool IsDirectLightSamplingDisabled() const {
		return (lightsDistribution == NULL);
	}

	void AddSamplingPoint(const luxrays::Point &pnt, const luxrays::Normal &nml, 
			const bool isTrans, const PathVolumeInfo &vi);
	
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
		bool isTransparent;
		std::vector<DLSCachePoint> samplingPoints;

		std::vector<float> lightReceivedLuminance;
		std::vector<u_int> distributionIndexToLightIndex;
		
		std::vector<float> mergedLightReceivedLuminance;
		std::vector<u_int> mergedDistributionIndexToLightIndex;
	} TemporayInformation;

	void DeleteTmpInfo() {
		delete tmpInfo;
		tmpInfo = NULL;
	}

	
	TemporayInformation *tmpInfo;
};

//------------------------------------------------------------------------------
// Direct light sampling cache
//------------------------------------------------------------------------------

class Scene;
class DLSCOctree;
class DLSCBvh;

class DirectLightSamplingCache {
public:
	DirectLightSamplingCache();
	virtual ~DirectLightSamplingCache();

	void Build(const Scene *scene);
	
	const DLSCacheEntry *GetEntry(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;

	// Used for OpenCL data translation
	const DLSCBvh *GetBVH() const { return bvh; }

	u_int maxSampleCount, maxDepth, maxEntryPasses;
	float targetCacheHitRate, lightThreshold;
	float entryRadius, entryNormalAngle, entryConvergenceThreshold;
	u_int entryWarmUpSamples;
	
	bool entryOnVolumes;

private:
	void GenerateEyeRay(const Camera *camera, luxrays::Ray &eyeRay,
			PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const;
	float SampleLight(const Scene *scene, DLSCacheEntry *entry,
		const LightSource *light, const u_int pass) const;
	
	void BuildCacheEntries(const Scene *scene);
	void FillCacheEntry(const Scene *scene, DLSCacheEntry *entry);
	void FillCacheEntries(const Scene *scene);
	void MergeCacheEntry(const Scene *scene, DLSCacheEntry *entry);
	void MergeCacheEntries(const Scene *scene);
	void BuildBVH(const Scene *scene);

	void DebugExport(const std::string &fileName, const float sphereRadius) const;

	std::vector<DLSCacheEntry *> allEntries;

	// Used only during the building phase
	DLSCOctree *octree;
	// Used during the rendering phase
	DLSCBvh *bvh;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_DLSCACHEIMPL_H */
