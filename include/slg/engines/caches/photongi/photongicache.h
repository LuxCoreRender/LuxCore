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

#ifndef _SLG_PHOTONGICACHE_H
#define	_SLG_PHOTONGICACHE_H

#include <vector>
#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/function.hpp>

#include "luxrays/core/color/spectrumgroup.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/utils.h"
#include "luxrays/utils/serializationutils.h"

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

	friend class boost::serialization::access;
	
protected:
	// Used by serialization
	GenericPhoton() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & p;
		ar & isVolume;
	}
};

struct PGICVisibilityParticle : GenericPhoton {
	PGICVisibilityParticle(const luxrays::Point &pt, const luxrays::Normal &nm,
		const luxrays::Spectrum& bsdfEvalTotal, const bool isVol) :
			GenericPhoton(pt, isVol), n(nm),
			bsdfEvaluateTotal(bsdfEvalTotal), hitsAccumulatedDistance(0.f),
			hitsCount(0) {
	}

	luxrays::SpectrumGroup ComputeRadiance(const float radius2, const float photonTraced) const {
		if (hitsCount > 0) {
			// The estimated area covered by the entry (if I have enough hits)
			const float area = (hitsCount < 16) ?  (radius2 * M_PI) :
				(luxrays::Sqr(2.f * hitsAccumulatedDistance / hitsCount) * M_PI);

			luxrays::SpectrumGroup result = alphaAccumulated;
			result *= (bsdfEvaluateTotal * INV_PI) / (photonTraced * area);

			return result;
		} else
			return luxrays::SpectrumGroup();
	}

	luxrays::Normal n;
	luxrays::Spectrum bsdfEvaluateTotal;
	luxrays::SpectrumGroup alphaAccumulated;

	// The following counters are used to estimate the surface covered by
	// this entry.
	float hitsAccumulatedDistance;
	u_int hitsCount;

	friend class boost::serialization::access;
	
protected:
	// Used by serialization
	PGICVisibilityParticle() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(GenericPhoton);
		ar & n;
		ar & bsdfEvaluateTotal;
		ar & alphaAccumulated;
		ar & hitsAccumulatedDistance;
		ar & hitsCount;
	}
};

struct Photon : GenericPhoton {
	Photon(const luxrays::Point &pt, const luxrays::Vector &dir, const u_int id,
		const luxrays::Spectrum &a, const luxrays::Normal &n, const bool isVol) :
			GenericPhoton(pt, isVol), d(dir),
			lightID(id), alpha(a), landingSurfaceNormal(n) {
	}

	luxrays::Vector d;
	u_int lightID;
	luxrays::Spectrum alpha;
	luxrays::Normal landingSurfaceNormal;

	friend class boost::serialization::access;
	
protected:
	// Used by serialization
	Photon() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(GenericPhoton);
		ar & d;
		ar & lightID;
		ar & alpha;
		ar & landingSurfaceNormal;
	}
};

struct RadiancePhoton : GenericPhoton {
	RadiancePhoton(const luxrays::Point &pt, const luxrays::Normal &nm,
		const luxrays::SpectrumGroup &rad, const bool isVol) :
				GenericPhoton(pt, isVol), n(nm), outgoingRadiance(rad) {
	}

	luxrays::Normal n;
	luxrays::SpectrumGroup outgoingRadiance;

	friend class boost::serialization::access;
	
protected:
	// Used by serialization
	RadiancePhoton() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(GenericPhoton);
		ar & n;
		ar & outgoingRadiance;
	}
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

typedef struct PhotonGICacheParams_t {
	PhotonGISamplerType samplerType;

	struct {
		u_int maxTracedCount, maxPathDepth;
		float timeStart, timeEnd;
	} photon;

	struct {
		float targetHitRate;
		u_int maxSampleCount;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle, lookUpNormalCosAngle;
	} visibility;

	float glossinessUsageThreshold;

	struct {
		bool enabled;
		u_int maxSize;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle,
				usageThresholdScale,
				filterRadiusScale, haltThreshold;
	} indirect;

	struct {
		bool enabled;
		u_int maxSize;
		float lookUpRadius, lookUpRadius2, lookUpNormalAngle,
				radiusReduction, minLookUpRadius;
		u_int updateSpp;
	} caustic;

	PhotonGIDebugType debugType;

	struct {
		std::string fileName;
		bool safeSave;
	} persistent;

	friend class boost::serialization::access;
	
protected:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & photon.maxTracedCount;
		ar & photon.maxPathDepth;

		ar & visibility.targetHitRate;
		ar & visibility.maxSampleCount;
		ar & visibility.lookUpRadius;
		ar & visibility.lookUpRadius2;
		ar & visibility.lookUpNormalAngle;
		ar & visibility.lookUpNormalCosAngle;

		ar & glossinessUsageThreshold;

		ar & indirect.enabled;
		ar & indirect.maxSize;
		ar & indirect.lookUpRadius;
		ar & indirect.lookUpRadius2;
		ar & indirect.lookUpNormalAngle;
		ar & indirect.usageThresholdScale;
		ar & indirect.filterRadiusScale;
		ar & indirect.haltThreshold;

		ar & caustic.enabled;
		ar & caustic.maxSize;
		ar & caustic.lookUpRadius;
		ar & caustic.lookUpRadius2;
		ar & caustic.lookUpNormalAngle;
		ar & caustic.radiusReduction;
		ar & caustic.minLookUpRadius;
		ar & caustic.updateSpp;

		ar & debugType;
		
		ar & persistent.fileName;
		ar & persistent.safeSave;
	}
} PhotonGICacheParams;

class PGICSceneVisibility;
class TracePhotonsThread;
class EyePathInfo;

class PhotonGICache {
public:
	PhotonGICache(const Scene *scn, const PhotonGICacheParams &params);
	virtual ~PhotonGICache();

	void SetScene(const Scene *scn) { scene = scn; }
	PhotonGIDebugType GetDebugType() const { return params.debugType; }
	
	bool IsIndirectEnabled() const { return params.indirect.enabled; }
	bool IsCausticEnabled() const { return params.caustic.enabled; }
	bool IsPhotonGIEnabled(const BSDF &bsdf) const;
	float GetIndirectUsageThreshold(const BSDFEvent lastBSDFEvent,
			const float lastGlossiness, const float u0) const;
	bool IsDirectLightHitVisible(const EyePathInfo &pathInfo,
		const bool photonGICausticCacheUsed) const;
	
	const PhotonGICacheParams &GetParams() const { return params; }

	void Preprocess(const u_int threadCount);
	bool Update(const u_int threadIndex, const u_int filmSPP,
		const boost::function<void()> &threadZeroCallback);
	bool Update(const u_int threadIndex, const u_int filmSPP) {
		const boost::function<void()> noCallback;
		return Update(threadIndex, filmSPP, noCallback);
	}
	void FinishUpdate(const u_int threadIndex);

	const luxrays::SpectrumGroup *GetIndirectRadiance(const BSDF &bsdf) const;
	luxrays::SpectrumGroup ConnectWithCausticPaths(const BSDF &bsdf) const;
	
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

	friend class PGICSceneVisibility;
	friend class TracePhotonsThread;
	friend class boost::serialization::access;

private:
	// Used by serialization
	PhotonGICache();

	float EvaluateBestRadius();
	void EvaluateBestRadiusImpl(const u_int threadIndex, const u_int workSize,
			float &accumulatedRadiusSize, u_int &radiusSizeCount) const;
	void TraceVisibilityParticles();
	void TracePhotons(const u_int seedBase, const u_int photonTracedCount,
		const bool indirectCacheDone, const bool causticCacheDone,
		boost::atomic<u_int> &globalIndirectPhotonsTraced,
		boost::atomic<u_int> &globalCausticPhotonsTraced,
		boost::atomic<u_int> &globalIndirectSize,
		boost::atomic<u_int> &globalCausticSize);
	void TracePhotons(const bool indirectEnabled, const bool causticEnabled);
	void FilterVisibilityParticlesRadiance(const std::vector<luxrays::SpectrumGroup> &radianceValues,
			std::vector<luxrays::SpectrumGroup> &filteredRadianceValues) const;
	void CreateRadiancePhotons();

	void LoadPersistentCache(const std::string &fileName);
	void SavePersistentCache(const std::string &fileName);

	template<class Archive> void serialize(Archive &ar, const u_int version);

	const Scene *scene;
	PhotonGICacheParams params;

	u_int threadCount;
	std::unique_ptr<boost::barrier> threadsSyncBarrier;
	u_int lastUpdateSpp, updateSeedBase;
	bool finishUpdateFlag;

	// Visibility map
	std::vector<PGICVisibilityParticle> visibilityParticles;
	PGICKdTree *visibilityParticlesKdTree;

	// Radiance photon map
	std::vector<RadiancePhoton> radiancePhotons;
	PGICRadiancePhotonBvh *radiancePhotonsBVH;
	u_int indirectPhotonTracedCount;

	// Caustic photon maps
	std::vector<Photon> causticPhotons;
	PGICPhotonBvh *causticPhotonsBVH;
	u_int causticPhotonTracedCount, causticPhotonPass;
};

}

BOOST_CLASS_VERSION(slg::GenericPhoton, 1)
BOOST_CLASS_VERSION(slg::PGICVisibilityParticle, 2)
BOOST_CLASS_VERSION(slg::Photon, 2)
BOOST_CLASS_VERSION(slg::RadiancePhoton, 2)
BOOST_CLASS_VERSION(slg::PhotonGICacheParams, 6)
BOOST_CLASS_VERSION(slg::PhotonGICache, 3)

BOOST_CLASS_EXPORT_KEY(slg::GenericPhoton)
BOOST_CLASS_EXPORT_KEY(slg::PGICVisibilityParticle)
BOOST_CLASS_EXPORT_KEY(slg::Photon)
BOOST_CLASS_EXPORT_KEY(slg::RadiancePhoton)
BOOST_CLASS_EXPORT_KEY(slg::PhotonGICacheParams)
BOOST_CLASS_EXPORT_KEY(slg::PhotonGICache)

#endif	/* _SLG_PHOTONGICACHE_H */
