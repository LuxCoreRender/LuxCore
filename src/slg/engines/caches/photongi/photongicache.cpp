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
		samplerSharedData(131, nullptr),
		directRadiancePhotonsBVH(nullptr),
		indirectRadiancePhotonsBVH(nullptr),
		causticRadiancePhotonsBVH(nullptr) {
}

PhotonGICache::~PhotonGICache() {
	delete directRadiancePhotonsBVH;
	delete indirectRadiancePhotonsBVH;
	delete causticRadiancePhotonsBVH;
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

		// Copy all radiance photons
		directRadiancePhotons.insert(directRadiancePhotons.end(), renderThreads[i]->directRadiancePhotons.begin(),
				renderThreads[i]->directRadiancePhotons.end());
		indirectRadiancePhotons.insert(indirectRadiancePhotons.end(), renderThreads[i]->indirectRadiancePhotons.begin(),
				renderThreads[i]->indirectRadiancePhotons.end());
		causticRadiancePhotons.insert(causticRadiancePhotons.end(), renderThreads[i]->causticRadiancePhotons.begin(),
				renderThreads[i]->causticRadiancePhotons.end());

		delete renderThreads[i];
		renderThreads[i] = nullptr;
	}

	SLG_LOG("Photon GI total photon traced: " << photonCount);
	SLG_LOG("Photon GI total direct radiance photon stored: " << directRadiancePhotons.size());
	SLG_LOG("Photon GI total indirect radiance photon stored: " << indirectRadiancePhotons.size());
	SLG_LOG("Photon GI total caustic radiance photon stored: " << causticRadiancePhotons.size());
}

// Simpson filter from PBRT v2. Filter the photons according their
// distance, giving more weight to the nearest.
static inline float SimpsonKernel(const Photon *photon, const Point &p,
		const float entryRadius2) {
	const float dist = DistanceSquared(photon->p, p);

	// The distance between photon.p and p is supposed to be < entryRadius2
	assert (dist <= entryRadius2);
    const float s = (1.f - dist / entryRadius2);

    return 3.f * INV_PI * s * s;
}

void PhotonGICache::FillRadiancePhotonData(RadiancePhoton &radiacePhoton,
		const PGICBvh<Photon> *photonsBVH) {
	vector<const Photon *> entries;

	photonsBVH->GetAllNearEntries(entries, radiacePhoton.p);

	const float entryRadius2 = entryRadius * entryRadius;
	radiacePhoton.outgoingRadiance = Spectrum();
	for (auto photon : entries) {
		if (Dot(radiacePhoton.n, -photon->d) > DEFAULT_COS_EPSILON_STATIC)
			radiacePhoton.outgoingRadiance += SimpsonKernel(photon, radiacePhoton.p, entryRadius2) * photon->alpha;
	}
	
	radiacePhoton.outgoingRadiance /= photonCount * entryRadius2 * M_PI;
}

void PhotonGICache::FillRadiancePhotonsData(vector<RadiancePhoton> &radiancePhotons,
		const vector<Photon> &photons, const PGICBvh<Photon> *photonsBVH) {

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
		
		FillRadiancePhotonData(radiancePhotons[i], photonsBVH);
		
		++counter;
	}
}

void PhotonGICache::Preprocess() {
	// Photons maps (used only during the preprocessing)
	vector<Photon> directPhotons, indirectPhotons, causticPhotons;

	//--------------------------------------------------------------------------
	// Fill all photon vectors
	//--------------------------------------------------------------------------

	TracePhotons(directPhotons, indirectPhotons, causticPhotons);

	//--------------------------------------------------------------------------
	// Direct light radiance map
	//--------------------------------------------------------------------------

	if (directPhotons.size() > 0) {
		SLG_LOG("Photon GI building direct photons BVH");
		PGICBvh<Photon> *directPhotonsBVH = new PGICBvh<Photon>(directPhotons, entryRadius);

		SLG_LOG("Photon GI building direct radiance photon data");
		FillRadiancePhotonsData(directRadiancePhotons, directPhotons, directPhotonsBVH);

		delete directPhotonsBVH;
		directPhotonsBVH = nullptr;
		directPhotons.clear();
		directPhotons.shrink_to_fit();

		SLG_LOG("Photon GI building radiance direct photon BVH");
		directRadiancePhotonsBVH = new PGICBvh<RadiancePhoton>(directRadiancePhotons, entryRadius);
	}

	//--------------------------------------------------------------------------
	// Indirect light radiance map
	//--------------------------------------------------------------------------

	if (indirectPhotons.size() > 0) {
		SLG_LOG("Photon GI building indirect photons BVH");
		PGICBvh<Photon> *indirectPhotonsBVH = new PGICBvh<Photon>(indirectPhotons, entryRadius);

		SLG_LOG("Photon GI building indirect radiance photon data");
		FillRadiancePhotonsData(indirectRadiancePhotons, indirectPhotons, indirectPhotonsBVH);

		delete indirectPhotonsBVH;
		indirectPhotonsBVH = nullptr;
		indirectPhotons.clear();
		indirectPhotons.shrink_to_fit();

		SLG_LOG("Photon GI building radiance indirect photon BVH");
		indirectRadiancePhotonsBVH = new PGICBvh<RadiancePhoton>(indirectRadiancePhotons, entryRadius);
	}

	//--------------------------------------------------------------------------
	// Caustic radiance map
	//--------------------------------------------------------------------------

	if (causticPhotons.size() > 0) {
		SLG_LOG("Photon GI building caustic photons BVH");
		PGICBvh<Photon> *causticPhotonsBVH = new PGICBvh<Photon>(causticPhotons, entryRadius);

		SLG_LOG("Photon GI building caustic radiance photon data");
		FillRadiancePhotonsData(causticRadiancePhotons, causticPhotons, causticPhotonsBVH);

		delete causticPhotonsBVH;
		causticPhotonsBVH = nullptr;
		causticPhotons.clear();
		causticPhotons.shrink_to_fit();

		SLG_LOG("Photon GI building radiance caustic photon BVH");
		causticRadiancePhotonsBVH = new PGICBvh<RadiancePhoton>(causticRadiancePhotons, entryRadius);
	}
}

Spectrum PhotonGICache::GetRadiance(const BSDF &bsdf) const {
	assert (bsdf.GetMaterialType() == MaterialType::MATTE);

	const RadiancePhoton *radiancePhoton;
	Spectrum result;

	// Direct light
	if (directRadiancePhotonsBVH) {
		radiancePhoton = directRadiancePhotonsBVH->GetNearEntry(bsdf.hitPoint.p, bsdf.hitPoint.shadeN);
		if (radiancePhoton)
			result += radiancePhoton->outgoingRadiance * bsdf.EvaluateTotal();
	}

	// Indirect light
	if (indirectRadiancePhotonsBVH) {
		radiancePhoton = indirectRadiancePhotonsBVH->GetNearEntry(bsdf.hitPoint.p, bsdf.hitPoint.shadeN);
		if (radiancePhoton)
			result += radiancePhoton->outgoingRadiance * bsdf.EvaluateTotal();
	}

	// Caustic
	if (causticRadiancePhotonsBVH) {
		radiancePhoton = causticRadiancePhotonsBVH->GetNearEntry(bsdf.hitPoint.p, bsdf.hitPoint.shadeN);
		if (radiancePhoton)
			result += radiancePhoton->outgoingRadiance * bsdf.EvaluateTotal();
	}

	return result;
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
