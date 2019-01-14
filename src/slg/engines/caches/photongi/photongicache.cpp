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

PhotonGICache::PhotonGICache(const Scene *scn, const u_int tracedCount,
		const u_int pathDepth, const float radius, const float normalAngle,
		const bool direct, const u_int maxDirect,
		const bool indirect, const u_int maxIndirect,
		const bool caustic, const u_int maxCaustic) : scene(scn),
		maxPhotonTracedCount(tracedCount), maxPathDepth(pathDepth),
		entryRadius(radius), entryRadius2(radius * radius), entryNormalAngle(normalAngle),
		directEnabled(direct), indirectEnabled(indirect), causticEnabled(caustic),
		maxDirectSize(maxDirect), maxIndirectSize(maxIndirect), maxCausticSize(maxCaustic),
		samplerSharedData(131, nullptr),
		directPhotonTracedCount(0),
		indirectPhotonTracedCount(0),
		causticPhotonTracedCount(0),
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
	
	globalPhotonsCounter = 0;
	globalDirectPhotonsTraced = 0;
	globalIndirectPhotonsTraced = 0;
	globalCausticPhotonsTraced = 0;
	globalDirectSize = 0;
	globalIndirectSize = 0;
	globalCausticSize = 0;

	// Create the photon tracing threads
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

	directPhotonTracedCount = globalDirectPhotonsTraced;
	indirectPhotonTracedCount = globalIndirectPhotonsTraced;
	causticPhotonTracedCount = globalCausticPhotonsTraced;

	// globalPhotonsCounter isn't exactly the number: there is an error due
	// last bucket of work likely being smaller than work bucket size
	SLG_LOG("Photon GI total photon traced: " << globalPhotonsCounter);
	SLG_LOG("Photon GI total direct photon stored: " << directPhotons.size() <<
			" (" << directPhotonTracedCount << " traced)");
	SLG_LOG("Photon GI total indirect photon stored: " << indirectPhotons.size() <<
			" (" << indirectPhotonTracedCount << " traced)");
	SLG_LOG("Photon GI total caustic photon stored: " << causticPhotons.size() <<
			" (" << causticPhotonTracedCount << " traced)");
	SLG_LOG("Photon GI total radiance photon stored: " << radiancePhotons.size());
}

void PhotonGICache::AddOutgoingRadiance(RadiancePhoton &radiacePhoton, PGICBvh<Photon> *photonsBVH,
			const u_int photonTracedCount) const {
	if (photonsBVH) {
		vector<const Photon *> entries;

		photonsBVH->GetAllNearEntries(entries, radiacePhoton.p, radiacePhoton.n);

		Spectrum result;
		for (auto photon : entries) {
			// Using a box filter here (i.e. multiply by 1.0)
			result += photon->alpha;
		}

		result /= photonTracedCount * entryRadius2 * M_PI;

		radiacePhoton.outgoingRadiance += result;
	}
}

void PhotonGICache::FillRadiancePhotonData(RadiancePhoton &radiacePhoton) {
	radiacePhoton.outgoingRadiance = Spectrum();

	AddOutgoingRadiance(radiacePhoton, directPhotonsBVH, directPhotonTracedCount);
	AddOutgoingRadiance(radiacePhoton, indirectPhotonsBVH, indirectPhotonTracedCount);
	AddOutgoingRadiance(radiacePhoton, causticPhotonsBVH, causticPhotonTracedCount);
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
		directPhotonsBVH = new PGICBvh<Photon>(directPhotons, entryRadius, entryNormalAngle);
	}

	//--------------------------------------------------------------------------
	// Indirect light photon map
	//--------------------------------------------------------------------------

	if ((indirectPhotons.size() > 0) && indirectEnabled) {
		SLG_LOG("Photon GI building indirect photons BVH");
		indirectPhotonsBVH = new PGICBvh<Photon>(indirectPhotons, entryRadius, entryNormalAngle);
	}

	//--------------------------------------------------------------------------
	// Caustic photon map
	//--------------------------------------------------------------------------

	if ((causticPhotons.size() > 0) && (causticEnabled || indirectEnabled)) {
		SLG_LOG("Photon GI building caustic photons BVH");
		causticPhotonsBVH = new PGICBvh<Photon>(causticPhotons, entryRadius, entryNormalAngle);
	}

	//--------------------------------------------------------------------------
	// Radiance photon map
	//--------------------------------------------------------------------------

	if ((radiancePhotons.size() > 0) && indirectEnabled) {	
		SLG_LOG("Photon GI building radiance photon data");
		FillRadiancePhotonsData();

		SLG_LOG("Photon GI building radiance photons BVH");
		radiancePhotonsBVH = new PGICBvh<RadiancePhoton>(radiancePhotons, entryRadius, entryNormalAngle);
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

Spectrum PhotonGICache::GetAllRadiance(const BSDF &bsdf) const {
	assert (IsCachedMaterial(bsdf.GetMaterialType()));

	Spectrum result = Spectrum();
	if (radiancePhotonsBVH) {
		vector<const RadiancePhoton *> entries;

		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		radiancePhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p, n);
		
		for (auto photon : entries) {
			// Using a Simpson filter here
			result += photon->outgoingRadiance;
		}

		result = (result * bsdf.EvaluateTotal() * INV_PI) / entries.size();
	}

	return result;
}

Spectrum PhotonGICache::GetDirectRadiance(const BSDF &bsdf) const {
	assert (IsCachedMaterial(bsdf.GetMaterialType()));

	Spectrum result = Spectrum();
	if (directPhotonsBVH) {
		vector<const Photon *> entries;

		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		directPhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p, n);
		
		for (auto photon : entries) {
			// Using a Simpson filter here
			result += SimpsonKernel(bsdf.hitPoint.p, photon->p, entryRadius2) * photon->alpha;
		}

		result = (result * bsdf.EvaluateTotal() * INV_PI) / (directPhotonTracedCount * entryRadius2);
	}

	return result;
}

Spectrum PhotonGICache::GetIndirectRadiance(const BSDF &bsdf) const {
	assert (IsCachedMaterial(bsdf.GetMaterialType()));

	Spectrum result = Spectrum();
	if (radiancePhotonsBVH) {
		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		const RadiancePhoton *radiancePhoton = radiancePhotonsBVH->GetNearestEntry(bsdf.hitPoint.p, n);

		if (radiancePhoton)
			result = radiancePhoton->outgoingRadiance * bsdf.EvaluateTotal() * INV_PI;
	}
	
	return result;
}

Spectrum PhotonGICache::GetCausticRadiance(const BSDF &bsdf) const {
	assert (IsCachedMaterial(bsdf.GetMaterialType()));

	Spectrum result = Spectrum();
	if (causticPhotonsBVH) {
		vector<const Photon *> entries;

		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		causticPhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p, n);

		for (auto photon : entries) {
			// Using a Simpson filter here
			result += SimpsonKernel(bsdf.hitPoint.p, photon->p, entryRadius2) * photon->alpha;
		}

		result = (result * bsdf.EvaluateTotal() * INV_PI) / (causticPhotonTracedCount * entryRadius2);
	}
	
	return result;
}

Properties PhotonGICache::ToProperties(const Properties &cfg) {
	Properties props;

	props <<
			cfg.Get(GetDefaultProps().Get("path.photongi.direct.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxcount")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.direct.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.radius")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.normalangle"));

	return props;
}

const Properties &PhotonGICache::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.photongi.direct.enabled")(false) <<
			Property("path.photongi.indirect.enabled")(false) <<
			Property("path.photongi.caustic.enabled")(false) <<
			Property("path.photongi.photon.maxcount")(100000) <<
			Property("path.photongi.direct.maxsize")(100000) <<
			Property("path.photongi.indirect.maxsize")(100000) <<
			Property("path.photongi.caustic.maxsize")(100000) <<
			Property("path.photongi.maxdepth")(4) <<
			Property("path.photongi.radius")(.15f) <<
			Property("path.photongi.normalangle")(10.f);

	return props;
}

PhotonGICache *PhotonGICache::FromProperties(const Scene *scn, const Properties &cfg) {
	const bool directEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.direct.enabled")).Get<bool>();
	const bool indirectEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.indirect.enabled")).Get<bool>();
	const bool causticEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.caustic.enabled")).Get<bool>();
	
	if (directEnabled || indirectEnabled || causticEnabled) {
		const u_int maxPhotonTracedCount = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxcount")).Get<u_int>());

		const u_int maxDirectSize = (directEnabled || indirectEnabled) ? Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.direct.maxsize")).Get<u_int>()) : 0;
		const u_int maxIndirectSize = indirectEnabled ? Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.maxsize")).Get<u_int>()) : 0;
		const u_int maxCausticSize = (causticEnabled || indirectEnabled) ? Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.maxsize")).Get<u_int>()) : 0;

		const u_int maxDepth = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.maxdepth")).Get<u_int>());
		const float radius = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.radius")).Get<float>());
		const float normalAngle = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.normalangle")).Get<float>());

		return new PhotonGICache(scn, maxPhotonTracedCount, maxDepth,
				radius, normalAngle,
				directEnabled, maxDirectSize,
				indirectEnabled, maxIndirectSize,
				causticEnabled, maxCausticSize);
	} else
		return nullptr;
}
