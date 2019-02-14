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
#include "slg/engines/caches/photongi/pgicoctree.h"

namespace slg {

//------------------------------------------------------------------------------
// Photon Mapping Based GI cache
//------------------------------------------------------------------------------

struct GenericPhoton {
	GenericPhoton(const luxrays::Point &pt) : p(pt) {
	}

	luxrays::Point p;
};

struct VisibilityParticle : GenericPhoton {
	VisibilityParticle(const luxrays::Point &pt, const luxrays::Normal &nm,
			const luxrays::Spectrum& bsdfEvalTotal) : GenericPhoton(pt), n(nm),
			bsdfEvaluateTotal(bsdfEvalTotal) {
	}

	luxrays::Normal n;
	luxrays::Spectrum bsdfEvaluateTotal;
	luxrays::Spectrum alphaAccumulated;
};

struct Photon : GenericPhoton {
	Photon(const luxrays::Point &pt, const luxrays::Vector &dir,
		const luxrays::Spectrum &a, const luxrays::Normal &n) : GenericPhoton(pt), d(dir),
		alpha(a), landingSurfaceNormal(n) {
	}

	luxrays::Vector d;
	luxrays::Spectrum alpha;
	luxrays::Normal landingSurfaceNormal;
};

struct RadiancePhoton : GenericPhoton {
	RadiancePhoton(const luxrays::Point &pt, const luxrays::Normal &nm,
		const luxrays::Spectrum &rad) : GenericPhoton(pt), n(nm), outgoingRadiance(rad) {
	}

	luxrays::Normal n;
	luxrays::Spectrum outgoingRadiance;
};

struct NearPhoton {
    NearPhoton(const GenericPhoton *p = nullptr, const float d2 = INFINITY) : photon(p),
			distance2(d2) {
	}

    bool operator<(const NearPhoton &p2) const {
        return (distance2 == p2.distance2) ?
            (photon < p2.photon) : (distance2 < p2.distance2);
    }

    const GenericPhoton *photon;
    float distance2;
};

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

typedef enum {
	PGIC_DEBUG_SHOWINDIRECT, PGIC_DEBUG_SHOWCAUSTIC, PGIC_DEBUG_NONE
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
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle;
	} visibility;

	struct {
		bool enabled;
		u_int maxSize;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle,
				glossinessUsageThreshold, usageThresholdScale;
	} indirect;

	struct {
		bool enabled;
		u_int maxSize;
		u_int lookUpMaxCount;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle;
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
			const float lastGlossiness) const;
	bool IsDirectLightHitVisible(const bool causticCacheAlreadyUsed,
			const BSDFEvent lastBSDFEvent) const;
	
	const PhotonGICacheParams &GetParams() const { return params; }

	void Preprocess();

	luxrays::Spectrum GetIndirectRadiance(const BSDF &bsdf) const;
	luxrays::Spectrum GetCausticRadiance(const BSDF &bsdf) const;

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
	void TraceVisibilityParticles();
	void TracePhotons();
	void AddOutgoingRadiance(RadiancePhoton &radiacePhoton, const PGICPhotonBvh *photonsBVH,
			const u_int photonTracedCount) const;
	void FillRadiancePhotonData(RadiancePhoton &radiacePhoton);
	void CreateRadiancePhotons();
	luxrays::Spectrum ProcessCacheEntries(const std::vector<NearPhoton> &entries,
			const u_int photonTracedCount, const float maxDistance2, const BSDF &bsdf) const;

	const Scene *scene;
	PhotonGICacheParams params;

	boost::atomic<u_int> globalPhotonsCounter,
		globalIndirectPhotonsTraced, globalCausticPhotonsTraced,
		globalIndirectSize, globalCausticSize;
	SamplerSharedData *samplerSharedData;

	u_int indirectPhotonTracedCount, causticPhotonTracedCount;

	// Visibility map
	SobolSamplerSharedData visibilitySobolSharedData;
	boost::mutex visibilityParticlesOctreeMutex;
	boost::atomic<u_int> globalVisibilityParticlesCount;
	u_int visibilityCacheLookUp, visibilityCacheHits;
	bool visibilityWarmUp;

	std::vector<VisibilityParticle> visibilityParticles;
	PGCIOctree *visibilityParticlesOctree;

	// Caustic photon maps
	std::vector<Photon> causticPhotons;
	PGICPhotonBvh *causticPhotonsBVH;
	
	// Radiance photon map
	std::vector<RadiancePhoton> radiancePhotons;
	PGICRadiancePhotonBvh *radiancePhotonsBVH;
};

}

#endif	/* _SLG_PHOTONGICACHE_H */
