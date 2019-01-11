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

#if defined(_OPENMP)
#include <omp.h>
#endif

#include "slg/bsdf/bsdf.h"
#include "slg/scene/scene.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

// TODO: serialization

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

PhotonGICache::PhotonGICache(const Scene *scn, const u_int pCount,
		const u_int pathDepth, const float radius,
		const bool direct, const bool indirect,
		const bool caustic) : scene(scn),
		directEnabled(direct), indirectEnabled(indirect), causticEnabled(caustic),
		photonCount(pCount),
		maxPathDepth(pathDepth), entryRadius(radius), entryRadius2(radius * radius),
		samplerSharedData(131, nullptr),
		directPhotonsBVH(nullptr),
		indirectPhotonsBVH(nullptr),
		causticPhotonsBVH(nullptr),
		radiancePhotonsBVH(nullptr) {
}

PhotonGICache::~PhotonGICache() {
	delete directPhotonsBVH;
	delete indirectPhotonsBVH;
	delete causticPhotonsBVH;
	delete radiancePhotonsBVH;
}

void PhotonGICache::TracePhotons(vector<Photon> &directPhotons, vector<Photon> &indirectPhotons,
		vector<Photon> &causticPhotons) {
	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	vector<TracePhotonsThread *> renderThreads(renderThreadCount, nullptr);
	SLG_LOG("Photon GI thread count: " << renderThreads.size());
	
	// Start the photon tracing threads
	globalCounter = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i] = new TracePhotonsThread(*this, i);

	// Start photon tracing threads
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
	
	// Wait for the end of photon tracing threads
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		renderThreads[i]->Join();

		// Copy all photons
		directPhotons.insert(directPhotons.end(), renderThreads[i]->directPhotons.begin(),
				renderThreads[i]->directPhotons.end());
		indirectPhotons.insert(indirectPhotons.end(), renderThreads[i]->indirectPhotons.begin(),
				renderThreads[i]->indirectPhotons.end());
		causticPhotons.insert(causticPhotons.end(), renderThreads[i]->causticPhotons.begin(),
				renderThreads[i]->causticPhotons.end());
		radiancePhotons.insert(radiancePhotons.end(), renderThreads[i]->radiancePhotons.begin(),
				renderThreads[i]->radiancePhotons.end());

		delete renderThreads[i];
		renderThreads[i] = nullptr;
	}

	SLG_LOG("Photon GI total photon traced: " << photonCount);
	SLG_LOG("Photon GI total direct photon stored: " << directPhotons.size());
	SLG_LOG("Photon GI total indirect photon stored: " << indirectPhotons.size());
	SLG_LOG("Photon GI total caustic photon stored: " << causticPhotons.size());
	SLG_LOG("Photon GI total radiance photon stored: " << radiancePhotons.size());
}

void PhotonGICache::FillRadiancePhotonData(RadiancePhoton &radiacePhoton) {
	vector<const Photon *> entries;

	if (directPhotonsBVH)
		directPhotonsBVH->GetAllNearEntries(entries, radiacePhoton.p);
	if (indirectPhotonsBVH)
		indirectPhotonsBVH->GetAllNearEntries(entries, radiacePhoton.p);
	if (causticPhotonsBVH)
		causticPhotonsBVH->GetAllNearEntries(entries, radiacePhoton.p);

	radiacePhoton.outgoingRadiance = Spectrum();
	for (auto photon : entries) {
		// Using a box filter here (i.e. multiply by 1.0)
		radiacePhoton.outgoingRadiance += photon->alpha;
	}
	
	radiacePhoton.outgoingRadiance /= photonCount * entryRadius2 * M_PI;
}

void PhotonGICache::FillRadiancePhotonsData() {
	double lastPrintTime = WallClockTime();
	atomic<u_int> counter(0);
	
	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < radiancePhotons.size(); ++i) {
		const int tid =
#if defined(_OPENMP)
			omp_get_thread_num()
#else
			0
#endif
			;

		if (tid == 0) {
			const double now = WallClockTime();
			if (now - lastPrintTime > 2.0) {
				SLG_LOG("Radiance photon filled entries: " << counter << "/" << radiancePhotons.size() <<" (" << (u_int)((100.0 * counter) / radiancePhotons.size()) << "%)");
				lastPrintTime = now;
			}
		}
		
		FillRadiancePhotonData(radiancePhotons[i]);
		
		++counter;
	}
}

void PhotonGICache::Preprocess() {

	//--------------------------------------------------------------------------
	// Fill all photon vectors
	//--------------------------------------------------------------------------

	TracePhotons(directPhotons, indirectPhotons, causticPhotons);

	//--------------------------------------------------------------------------
	// Direct light photon map
	//--------------------------------------------------------------------------

	if ((directPhotons.size() > 0) && (directEnabled || indirectEnabled)) {
		SLG_LOG("Photon GI building direct photons BVH");
		directPhotonsBVH = new PGICBvh<Photon>(directPhotons, entryRadius);
	}

	//--------------------------------------------------------------------------
	// Indirect light photon map
	//--------------------------------------------------------------------------

	if ((indirectPhotons.size() > 0) && indirectEnabled) {
		SLG_LOG("Photon GI building indirect photons BVH");
		indirectPhotonsBVH = new PGICBvh<Photon>(indirectPhotons, entryRadius);
	}

	//--------------------------------------------------------------------------
	// Caustic photon map
	//--------------------------------------------------------------------------

	if ((causticPhotons.size() > 0) && (causticEnabled || indirectEnabled)) {
		SLG_LOG("Photon GI building caustic photons BVH");
		causticPhotonsBVH = new PGICBvh<Photon>(causticPhotons, entryRadius);
	}

	//--------------------------------------------------------------------------
	// Radiance photon map
	//--------------------------------------------------------------------------

	if ((radiancePhotons.size() > 0) && indirectEnabled) {	
		SLG_LOG("Photon GI building radiance photon data");
		FillRadiancePhotonsData();

		SLG_LOG("Photon GI building radiance photons BVH");
		radiancePhotonsBVH = new PGICBvh<RadiancePhoton>(radiancePhotons, entryRadius);
	}
	
	//--------------------------------------------------------------------------
	// Check what I can free because it is not going to be used during
	// the rendering
	//--------------------------------------------------------------------------
	
	// I can always free indirect photon map because I'm going to use the
	// radiance map if enabled
	delete indirectPhotonsBVH;
	indirectPhotonsBVH = nullptr;
	indirectPhotons.clear();
	indirectPhotons.shrink_to_fit();

	if (!directEnabled) {
		delete directPhotonsBVH;
		directPhotonsBVH = nullptr;
		directPhotons.clear();
		directPhotons.shrink_to_fit();
	}

	if (!indirectEnabled) {
		delete indirectPhotonsBVH;
		indirectPhotonsBVH = nullptr;
		indirectPhotons.clear();
		indirectPhotons.shrink_to_fit();
	}

	if (!causticEnabled) {
		delete causticPhotonsBVH;
		causticPhotonsBVH = nullptr;
		causticPhotons.clear();
		causticPhotons.shrink_to_fit();
	}
}

// Simpson filter from PBRT v2. Filter the photons according their
// distance, giving more weight to the nearest.
static inline float SimpsonKernel(const Point &p1, const Point &p2,
		const float entryRadius2) {
	const float dist = DistanceSquared(p1, p2);

	// The distance between p1 and p2 is supposed to be < entryRadius2
	assert (dist <= entryRadius2);
    const float s = (1.f - dist / entryRadius2);

    return 3.f * INV_PI * s * s;
}

Spectrum PhotonGICache::GetDirectRadiance(const BSDF &bsdf) const {
	assert (bsdf.GetMaterialType() == MaterialType::MATTE);

	Spectrum result = Spectrum();
	if (directPhotonsBVH) {
		vector<const Photon *> entries;

		directPhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p);
		
		for (auto photon : entries) {
			// Using a Simpson filter here
			result += SimpsonKernel(bsdf.hitPoint.p, photon->p, entryRadius2) * photon->alpha;
		}

		result = (result * bsdf.EvaluateTotal() * INV_PI) / (photonCount * entryRadius2 * M_PI);
	}

	return result;
}

Spectrum PhotonGICache::GetIndirectRadiance(const BSDF &bsdf) const {
	assert (bsdf.GetMaterialType() == MaterialType::MATTE);

	const RadiancePhoton *radiancePhoton = radiancePhotonsBVH->GetNearEntry(bsdf.hitPoint.p, bsdf.hitPoint.shadeN);

	if (radiancePhoton)
		return radiancePhoton->outgoingRadiance * bsdf.EvaluateTotal() * INV_PI;
	else
		return Spectrum();
}

Spectrum PhotonGICache::GetCausticRadiance(const BSDF &bsdf) const {
	assert (bsdf.GetMaterialType() == MaterialType::MATTE);

	Spectrum result = Spectrum();
	if (causticPhotonsBVH) {
		vector<const Photon *> entries;

		causticPhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p);

		for (auto photon : entries) {
			// Using a Simpson filter here
			result += SimpsonKernel(bsdf.hitPoint.p, photon->p, entryRadius2) * photon->alpha;
		}

		result = (result * bsdf.EvaluateTotal() * INV_PI) / (photonCount * entryRadius2 * M_PI);
	}
	
	return result;
}

Properties PhotonGICache::ToProperties(const Properties &cfg) {
	Properties props;

	props <<
			cfg.Get(GetDefaultProps().Get("path.photongi.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.enabled.direct")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.enabled.indirect")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.enabled.caustic")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.count")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.radius"));

	return props;
}

const Properties &PhotonGICache::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.photongi.enabled")(false) <<
			Property("path.photongi.enabled.direct")(false) <<
			Property("path.photongi.enabled.indirect")(true) <<
			Property("path.photongi.enabled.caustic")(true) <<
			Property("path.photongi.count")(100000) <<
			Property("path.photongi.depth")(4) <<
			Property("path.photongi.radius")(.15f);

	return props;
}

PhotonGICache *PhotonGICache::FromProperties(const Scene *scn, const Properties &cfg) {
	const bool enabled = cfg.Get(GetDefaultProps().Get("path.photongi.enabled")).Get<bool>();
	
	if (enabled) {
		const bool directEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.enabled.direct")).Get<bool>();
		const bool indirectEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.enabled.indirect")).Get<bool>();
		const bool causticEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.enabled.caustic")).Get<bool>();

		const u_int count = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.count")).Get<u_int>());
		const u_int depth = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.depth")).Get<u_int>());
		const float radius = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.radius")).Get<float>());

		return new PhotonGICache(scn, count, depth, radius,
				directEnabled, indirectEnabled, causticEnabled);
	} else
		return nullptr;
}
