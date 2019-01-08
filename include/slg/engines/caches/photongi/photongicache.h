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
		const luxrays::Spectrum &d) : p(pt), d(dir), data(d) {
	}

	luxrays::Point p;
	luxrays::Vector d;
	luxrays::Spectrum data;
};

struct RadiancePhoton {
	RadiancePhoton(const luxrays::Point &pt, const luxrays::Normal &nm,
		const luxrays::Spectrum &d) : p(pt), n(nm), data(d) {
	}

	luxrays::Point p;
	luxrays::Normal n;
	luxrays::Spectrum data;
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

	std::vector<Photon> photons;
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

class PhotonGICache {
public:
	PhotonGICache(const Scene *scn, const u_int photonCount, const u_int maxPathDepth,
			const float entryRadius);
	virtual ~PhotonGICache();

	void Preprocess();

	luxrays::Spectrum GetRadiance(const luxrays::Point &p, const luxrays::Normal &n) const;
	
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProps();
	static PhotonGICache *FromProperties(const Scene *scn, const luxrays::Properties &cfg);

	friend class TracePhotonsThread;

private:
	void TracePhotons();
	void BuildPhotonsBVH();
	void FreePhotonsMap();
	void FillRadiancePhotonData(RadiancePhoton &radiacePhoton);
	void FillRadiancePhotonsData();
	void BuildRadiancePhotonsBVH();

	const Scene *scene;
	
	const u_int photonCount, maxPathDepth;
	const float entryRadius;
	
	boost::atomic<u_int> globalCounter;
	SobolSamplerSharedData samplerSharedData;

	// Photons map
	std::vector<Photon> photons;
	PGICBvh<Photon> *photonBVH;
	
	// Radiance map
	std::vector<RadiancePhoton> radiancePhotons;
	PGICBvh<RadiancePhoton> *radiancePhotonBVH;
};

}

#endif	/* _SLG_PHOTONGICACHE_H */
