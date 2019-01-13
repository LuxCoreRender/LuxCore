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
#include "slg/samplers/sobol.h"
#include "slg/bsdf/bsdf.h"
#include "slg/engines/caches/photongi/pcgibvh.h"

namespace slg {

//------------------------------------------------------------------------------
// Photon Mapping Based GI cache
//------------------------------------------------------------------------------

struct Photon {
	Photon(const luxrays::Point &pt, const luxrays::Vector &dir,
		const luxrays::Spectrum &a) : p(pt), d(dir), alpha(a) {
	}

	luxrays::Point p;
	luxrays::Vector d;
	luxrays::Spectrum alpha;
};

struct RadiancePhoton {
	RadiancePhoton(const luxrays::Point &pt, const luxrays::Normal &nm,
		const luxrays::Spectrum &rad) : p(pt), n(nm), outgoingRadiance(rad) {
	}

	luxrays::Point p;
	luxrays::Normal n;
	luxrays::Spectrum outgoingRadiance;
};

//------------------------------------------------------------------------------
// TracePhotonsThread
//------------------------------------------------------------------------------

class PhotonGICache;

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

	void RenderFunc();

	PhotonGICache &pgic;
	const u_int threadIndex;

	boost::thread *renderThread;
};

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

class BSDF;

class PhotonGICache {
public:
	PhotonGICache(const Scene *scn, const u_int maxPhotonTracedCount, const u_int maxPathDepth,
			const float entryRadius,
			const bool directEnabled, u_int const maxDirectSize,
			const bool indirectEnabled, u_int const maxIndirectSize,
			const bool causticEnabled, u_int const maxCausticSize);
	virtual ~PhotonGICache();

	bool IsDirectEnabled() const { return directEnabled; }
	bool IsIndirectEnabled() const { return indirectEnabled; }
	bool IsCausticEnabled() const { return causticEnabled; }

	void Preprocess();

	luxrays::Spectrum GetDirectRadiance(const BSDF &bsdf) const;
	luxrays::Spectrum GetIndirectRadiance(const BSDF &bsdf) const;
	luxrays::Spectrum GetCausticRadiance(const BSDF &bsdf) const;
	
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();
	static PhotonGICache *FromProperties(const Scene *scn, const luxrays::Properties &cfg);

	friend class TracePhotonsThread;

private:
	void TracePhotons(std::vector<Photon> &directPhotons, std::vector<Photon> &indirectPhotons,
			std::vector<Photon> &causticPhotons);
	luxrays::Spectrum GetOutgoingRadiance(PGICBvh<Photon> *photonsBVH,
			const u_int photonTracedCount, const luxrays::Point &p) const;
	void FillRadiancePhotonData(RadiancePhoton &radiacePhoton);
	void FillRadiancePhotonsData();

	const Scene *scene;
	
	const u_int maxPhotonTracedCount, maxPathDepth;
	const float entryRadius, entryRadius2;
	const bool directEnabled, indirectEnabled, causticEnabled;
	const u_int maxDirectSize, maxIndirectSize, maxCausticSize;
	
	boost::atomic<u_int> globalPhotonsCounter, globalDirectPhotonsTraced,
		globalIndirectPhotonsTraced, globalCausticPhotonsTraced,
		globalDirectSize, globalIndirectSize, globalCausticSize;
	SobolSamplerSharedData samplerSharedData;

	u_int directPhotonTracedCount, indirectPhotonTracedCount, causticPhotonTracedCount;

	// Photon maps
	std::vector<Photon> directPhotons, indirectPhotons, causticPhotons;
	PGICBvh<Photon> *directPhotonsBVH, *indirectPhotonsBVH, *causticPhotonsBVH;
	
	// Radiance photon map
	std::vector<RadiancePhoton> radiancePhotons;
	PGICBvh<RadiancePhoton> *radiancePhotonsBVH;
};

}

#endif	/* _SLG_PHOTONGICACHE_H */
