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
		const u_int pathDepth, const float radius) : scene(scn), photonCount(pCount),
		maxPathDepth(pathDepth), entryRadius(radius),
		samplerSharedData(131, nullptr), photonBVH(nullptr) {
}

PhotonGICache::~PhotonGICache() {
	delete photonBVH;
}

void PhotonGICache::TracePhotons() {
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
		photons.insert(photons.end(), renderThreads[i]->photons.begin(),
				renderThreads[i]->photons.end());

		// Copy all radiance photons
		radiancePhotons.insert(radiancePhotons.end(), renderThreads[i]->radiancePhotons.begin(),
				renderThreads[i]->radiancePhotons.end());

		delete renderThreads[i];
		renderThreads[i] = nullptr;
	}

	SLG_LOG("Photon GI total photon traced: " << photonCount);
	SLG_LOG("Photon GI total radiance photon stored: " << radiancePhotons.size());
}

void PhotonGICache::BuildPhotonsBVH() {
	SLG_LOG("Photon GI building photon BVH");
	photonBVH = new PGICBvh<Photon>(photons, entryRadius);
}

void PhotonGICache::FreePhotonsMap() {
	delete photonBVH;
	photonBVH = nullptr;
	photons.clear();
	photons.shrink_to_fit();
}

void PhotonGICache::FillRadiancePhotonData(RadiancePhoton &radiacePhoton) {
	vector<const Photon *> entries;

	photonBVH->GetAllNearEntries(entries, radiacePhoton.p);

	radiacePhoton.outgoingRadiance = Spectrum();
	for (auto photon : entries) {
		if (Dot(radiacePhoton.n, -photon->d) > DEFAULT_COS_EPSILON_STATIC)
			radiacePhoton.outgoingRadiance += photon->alpha;
	}
	
	radiacePhoton.outgoingRadiance /= photonCount * entryRadius * entryRadius * M_PI;
}

void PhotonGICache::FillRadiancePhotonsData() {
	SLG_LOG("Photon GI building radiance photon data");

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

void PhotonGICache::BuildRadiancePhotonsBVH() {
	SLG_LOG("Photon GI building radiance photon BVH");
	radiancePhotonBVH = new PGICBvh<RadiancePhoton>(radiancePhotons, entryRadius);
}

void PhotonGICache::Preprocess() {
	TracePhotons();
	BuildPhotonsBVH();
	FillRadiancePhotonsData();
	FreePhotonsMap();
	BuildRadiancePhotonsBVH();
}

Spectrum PhotonGICache::GetRadiance(const BSDF &bsdf) const {
	assert (bsdf.GetMaterialType() == MaterialType::MATTE);

	const RadiancePhoton *radiancePhoton = radiancePhotonBVH->GetNearEntry(bsdf.hitPoint.p, bsdf.hitPoint.shadeN);

	if (radiancePhoton)
		return radiancePhoton->outgoingRadiance * bsdf.EvaluateTotal();
	else
		return Spectrum();
}

Properties PhotonGICache::ToProperties(const Properties &cfg) {
	Properties props;
	
	props <<
			cfg.Get(GetDefaultProps().Get("path.photongi.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.count")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.radius"));

	return props;
}

const Properties &PhotonGICache::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.photongi.enabled")(false) <<
			Property("path.photongi.count")(100000) <<
			Property("path.photongi.depth")(4) <<
			Property("path.photongi.radius")(.15f);

	return props;
}

PhotonGICache *PhotonGICache::FromProperties(const Scene *scn, const Properties &cfg) {
	const bool enabled = cfg.Get(GetDefaultProps().Get("path.photongi.enabled")).Get<bool>();
	
	if (enabled) {
		const u_int count = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.count")).Get<u_int>());
		const u_int depth = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.depth")).Get<u_int>());
		const float radius = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.radius")).Get<float>());

		return new PhotonGICache(scn, count, depth, radius);
	} else
		return nullptr;
}
