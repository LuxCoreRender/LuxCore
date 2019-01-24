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
#include <boost/thread.hpp>

#include "luxrays/utils/properties.h"
#include "luxrays/utils/utils.h"

#include "slg/slg.h"
#include "slg/samplers/sampler.h"
#include "slg/bsdf/bsdf.h"
#include "slg/engines/caches/photongi/pcgibvh.h"

namespace slg {

//------------------------------------------------------------------------------
// Photon Mapping Based GI cache
//------------------------------------------------------------------------------

struct GenericPhoton {
	GenericPhoton(const luxrays::Point &pt) : p(pt) {
	}

	luxrays::Point p;
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
// TracePhotonsThread
//------------------------------------------------------------------------------

class PhotonGICache;
class MetropolisSampler;

class TracePhotonsThread {
public:
	TracePhotonsThread(PhotonGICache &pgic, const u_int index);
	virtual ~TracePhotonsThread();

	void Start();
	void Join();

	std::vector<Photon> directPhotons, indirectPhotons, causticPhotons;
	std::vector<RadiancePhoton> radiancePhotons;

	friend class PhotonGICache;

private:
	void ConnectToEye(const float time, const float u0, const LightSource &light,
			const BSDF &bsdf, const luxrays::Point &lensPoint, const luxrays::Spectrum &flux,
			PathVolumeInfo volInfo, std::vector<SampleResult> &sampleResults);
	SampleResult &AddResult(std::vector<SampleResult> &sampleResults, const bool fromLight) const;

	void UpdatePhotonWeights(MetropolisSampler *sampler);

	void RenderFunc();

	PhotonGICache &pgic;
	const u_int threadIndex;

	boost::thread *renderThread;

	// Used with metropolis sampler to updated the photon weights after
	// the current sample has been accepted or rejected
	//
	// NOTE: I have to use indices instead of pointers because the photon vectors
	// can be be resized and photons moved
	std::vector<u_int> metropolisDirectPhotonListA, metropolisDirectPhotonListB;
	std::vector<u_int> metropolisIndirectPhotonListA, metropolisIndirectPhotonListB;
	std::vector<u_int> metropolisCausticPhotonListA, metropolisCausticPhotonListB;
	std::vector<u_int> metropolisRadiancePhotonListA, metropolisRadiancePhotonListB;

	std::vector<u_int> *metropolisDirectPhotonLastList, *metropolisDirectPhotonCurrentList;
	std::vector<u_int> *metropolisIndirectPhotonLastList, *metropolisIndirectPhotonCurrentList;
	std::vector<u_int> *metropolisCausticPhotonLastList, *metropolisCausticPhotonCurrentList;
	std::vector<u_int> *metropolisRadiancePhotonLastList, *metropolisRadiancePhotonCurrentList;
};

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

typedef enum {
	PGIC_DEBUG_SHOWDIRECT, PGIC_DEBUG_SHOWINDIRECT, PGIC_DEBUG_SHOWCAUSTIC, PGIC_DEBUG_NONE
} PhotonGIDebugType;

class PhotonGICache {
public:
	PhotonGICache(const SamplerType samplerType, const Scene *scn,
			const u_int maxPhotonTracedCount, const u_int maxPathDepth,
			const u_int entryMaxLookUpCount, const float entryRadius, const float entryNormalAngle,
			const bool directEnabled, u_int const maxDirectSize,
			const bool indirectEnabled, u_int const maxIndirectSize,
			const bool causticEnabled, u_int const maxCausticSize,
			const PhotonGIDebugType debugType);
	virtual ~PhotonGICache();

	PhotonGIDebugType GetDebugType() const { return debugType; }
	
	bool IsDirectEnabled() const { return directEnabled; }
	bool IsIndirectEnabled() const { return indirectEnabled; }
	bool IsCausticEnabled() const { return causticEnabled; }
	
	void Preprocess();

	luxrays::Spectrum GetAllRadiance(const BSDF &bsdf) const;
	luxrays::Spectrum GetDirectRadiance(const BSDF &bsdf) const;
	luxrays::Spectrum GetIndirectRadiance(const BSDF &bsdf) const;
	luxrays::Spectrum GetCausticRadiance(const BSDF &bsdf) const;

	static PhotonGIDebugType String2DebugType(const std::string &type);
	static std::string DebugType2String(const PhotonGIDebugType type);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();
	static PhotonGICache *FromProperties(const Scene *scn, const luxrays::Properties &cfg);

	friend class TracePhotonsThread;

private:
	void TracePhotons(std::vector<Photon> &directPhotons, std::vector<Photon> &indirectPhotons,
			std::vector<Photon> &causticPhotons);
	void AddOutgoingRadiance(RadiancePhoton &radiacePhoton, const PGICPhotonBvh *photonsBVH,
			const u_int photonTracedCount) const;
	void FillRadiancePhotonData(RadiancePhoton &radiacePhoton);
	void FillRadiancePhotonsData();
	luxrays::Spectrum ProcessCacheEntries(const std::vector<NearPhoton> &entries,
			const u_int photonTracedCount, const float maxDistance2, const BSDF &bsdf) const;
	void FilterPhoton(Photon &photon, const PGICPhotonBvh *photonsBVH) const;
	void FilterPhotons(std::vector<Photon> &photons, const PGICPhotonBvh *photonsBVH);

	const SamplerType samplerType;
	const Scene *scene;
	
	const u_int maxPhotonTracedCount, maxPathDepth;
	const u_int entryMaxLookUpCount;
	const float entryRadius, entryRadius2, entryNormalAngle;
	const bool directEnabled, indirectEnabled, causticEnabled;
	const u_int maxDirectSize, maxIndirectSize, maxCausticSize;
	const PhotonGIDebugType debugType;
	
	boost::atomic<u_int> globalPhotonsCounter, globalDirectPhotonsTraced,
		globalIndirectPhotonsTraced, globalCausticPhotonsTraced,
		globalDirectSize, globalIndirectSize, globalCausticSize;
	SamplerSharedData *samplerSharedData;

	u_int directPhotonTracedCount, indirectPhotonTracedCount, causticPhotonTracedCount;

	// Photon maps
	std::vector<Photon> directPhotons, indirectPhotons, causticPhotons;
	PGICPhotonBvh *directPhotonsBVH, *indirectPhotonsBVH, *causticPhotonsBVH;
	
	// Radiance photon map
	std::vector<RadiancePhoton> radiancePhotons;
	PGICRadiancePhotonBvh *radiancePhotonsBVH;
};

}

#endif	/* _SLG_PHOTONGICACHE_H */
