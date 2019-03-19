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

#ifndef _SLG_PHOTONGICACHE_H
#define	_SLG_PHOTONGICACHE_H

#include <vector>
#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>

#include "luxrays/utils/properties.h"
#include "luxrays/utils/utils.h"

#include "slg/slg.h"
#include "slg/samplers/sobol.h"
#include "slg/bsdf/bsdf.h"
#include "slg/scene/scene.h"
#include "slg/engines/caches/photongi/pgicbvh.h"
#include "slg/engines/caches/photongi/pgickdtree.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/engines/caches/photongi/pgic_types.cl"
}

//------------------------------------------------------------------------------
// Photon Mapping based GI cache
//------------------------------------------------------------------------------

struct GenericPhoton {
	GenericPhoton(const luxrays::Point &pt, const bool isVol) : p(pt), isVolume(isVol) {
	}

	luxrays::Point p;
	bool isVolume;
};

struct VisibilityParticle : GenericPhoton {
	VisibilityParticle(const luxrays::Point &pt, const luxrays::Normal &nm,
		const luxrays::Spectrum& bsdfEvalTotal, const bool isVol) :
			GenericPhoton(pt, isVol), n(nm),
			bsdfEvaluateTotal(bsdfEvalTotal), hitsAccumulatedDistance(0.f),
			hitsCount(0) {
	}

	luxrays::Spectrum ComputeRadiance(const float radius2, const float photonTraced) const {
		if (hitsCount > 0) {
			// The estimated area covered by the entry (if I have enough hits)
			const float area = (hitsCount < 16) ?  (radius2 * M_PI) :
				(luxrays::Sqr(2.f * hitsAccumulatedDistance / hitsCount) * M_PI);

			return (bsdfEvaluateTotal * INV_PI) * alphaAccumulated / (photonTraced * area);
		} else
			return luxrays::Spectrum();
	}

	luxrays::Normal n;
	luxrays::Spectrum bsdfEvaluateTotal;
	luxrays::Spectrum alphaAccumulated;

	// The following counters are used to estimate the surface covered by
	// this entry.
	float hitsAccumulatedDistance;
	u_int hitsCount;
};

struct Photon : GenericPhoton {
	Photon(const luxrays::Point &pt, const luxrays::Vector &dir,
		const luxrays::Spectrum &a, const luxrays::Normal &n, const bool isVol) :
			GenericPhoton(pt, isVol), d(dir),
			alpha(a), landingSurfaceNormal(n) {
	}

	luxrays::Vector d;
	luxrays::Spectrum alpha;
	luxrays::Normal landingSurfaceNormal;
};

struct RadiancePhoton : GenericPhoton {
	RadiancePhoton(const luxrays::Point &pt, const luxrays::Normal &nm,
		const luxrays::Spectrum &rad, const bool isVol) :
				GenericPhoton(pt, isVol), n(nm), outgoingRadiance(rad) {
	}

	luxrays::Normal n;
	luxrays::Spectrum outgoingRadiance;
};

struct NearPhoton {
    NearPhoton(const u_int index = NULL_INDEX, const float d2 = INFINITY) : photonIndex(index),
			distance2(d2) {
	}

    bool operator<(const NearPhoton &p2) const {
        return (distance2 == p2.distance2) ?
            (photonIndex < p2.photonIndex) : (distance2 < p2.distance2);
    }

    u_int photonIndex;
    float distance2;
};

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

typedef enum {
	PGIC_DEBUG_NONE, PGIC_DEBUG_SHOWINDIRECT, PGIC_DEBUG_SHOWCAUSTIC,
	PGIC_DEBUG_SHOWINDIRECTPATHMIX
} PhotonGIDebugType;

typedef enum {
	PGIC_SAMPLER_RANDOM, PGIC_SAMPLER_METROPOLIS
} PhotonGISamplerType;

typedef struct {
	PhotonGISamplerType samplerType;

	struct {
		u_int maxTracedCount, maxPathDepth;
	} photon;

	struct {
		float targetHitRate;
		u_int maxSampleCount;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle, lookUpNormalCosAngle;
	} visibility;

	struct {
		bool enabled;
		u_int maxSize;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle,
				glossinessUsageThreshold, usageThresholdScale,
				filterRadiusScale, haltThreshold;
	} indirect;

	struct {
		bool enabled;
		u_int maxSize;
		u_int lookUpMaxCount;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle,
				mergeRadiusScale;
	} caustic;

	PhotonGIDebugType debugType;
} PhotonGICacheParams;

class TracePhotonsThread;
class TraceVisibilityThread;

class PhotonGICache {
public:
	PhotonGICache(const Scene *scn, const PhotonGICacheParams &params);
	virtual ~PhotonGICache();

	PhotonGIDebugType GetDebugType() const { return params.debugType; }
	
	bool IsIndirectEnabled() const { return params.indirect.enabled; }
	bool IsCausticEnabled() const { return params.caustic.enabled; }
	bool IsPhotonGIEnabled(const BSDF &bsdf) const;
	float GetIndirectUsageThreshold(const BSDFEvent lastBSDFEvent,
			const float lastGlossiness, const float u0) const;
	bool IsDirectLightHitVisible(const bool causticCacheAlreadyUsed,
			const BSDFEvent lastBSDFEvent, const PathDepthInfo &depthInfo) const;
	
	const PhotonGICacheParams &GetParams() const { return params; }

	void Preprocess();

	luxrays::Spectrum GetIndirectRadiance(const BSDF &bsdf) const;
	luxrays::Spectrum GetCausticRadiance(const BSDF &bsdf) const;
	
	const std::vector<RadiancePhoton> &GetRadiancePhotons() const { return radiancePhotons; }
	const PGICRadiancePhotonBvh *GetRadiancePhotonsBVH() const { return radiancePhotonsBVH; }
	const u_int GetRadiancePhotonTracedCount() const { return indirectPhotonTracedCount; }

	const std::vector<Photon> &GetCausticPhotons() const { return causticPhotons; }
	const PGICPhotonBvh *GetCausticPhotonsBVH() const { return causticPhotonsBVH; }
	const u_int GetCausticPhotonTracedCount() const { return causticPhotonTracedCount; }

	static PhotonGISamplerType String2SamplerType(const std::string &type);
	static std::string SamplerType2String(const PhotonGISamplerType type);
	static PhotonGIDebugType String2DebugType(const std::string &type);
	static std::string DebugType2String(const PhotonGIDebugType type);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();
	static PhotonGICache *FromProperties(const Scene *scn, const luxrays::Properties &cfg);

	friend class TracePhotonsThread;
	friend class TraceVisibilityThread;

private:
	float EvaluateBestRadius();
	void EvaluateBestRadiusImpl(const u_int threadIndex, const u_int workSize,
			float &accumulatedRadiusSize, u_int &radiusSizeCount) const;
	void TraceVisibilityParticles();
	void TracePhotons(const u_int photonTracedCount,
		boost::atomic<u_int> &globalIndirectPhotonsTraced,
		boost::atomic<u_int> &globalCausticPhotonsTraced,
		boost::atomic<u_int> &globalIndirectSize,
		boost::atomic<u_int> &globalCausticSize);
	void TracePhotons();
	void FilterVisibilityParticlesRadiance(const std::vector<luxrays::Spectrum> &radianceValues,
			std::vector<luxrays::Spectrum> &filteredRadianceValues) const;
	void CreateRadiancePhotons();
	void MergeCausticPhotons();
	luxrays::Spectrum ProcessCacheEntries(const std::vector<NearPhoton> &entries,
			const float maxDistance2,
			const std::vector<Photon> &photons, const u_int photonTracedCount,
			const BSDF &bsdf) const;

	const Scene *scene;
	PhotonGICacheParams params;

	// Visibility map
	std::vector<VisibilityParticle> visibilityParticles;
	PGICKdTree *visibilityParticlesKdTree;

	// Radiance photon map
	std::vector<RadiancePhoton> radiancePhotons;
	PGICRadiancePhotonBvh *radiancePhotonsBVH;
	u_int indirectPhotonTracedCount;

	// Caustic photon maps
	std::vector<Photon> causticPhotons;
	PGICPhotonBvh *causticPhotonsBVH;
	u_int causticPhotonTracedCount;
};

}

#endif	/* _SLG_PHOTONGICACHE_H */
