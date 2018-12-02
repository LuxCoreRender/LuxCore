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

#include "slg/scene/scene.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/engines/caches/photongi/pcgibvh.h"

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

		delete renderThreads[i];
		renderThreads[i] = nullptr;
	}

	SLG_LOG("Photon GI total photon traced: " << photonCount);
}

void PhotonGICache::BuildPhotonsBVH() {
	SLG_LOG("Photon GI building BVH");
	photonBVH = new PGICBvh(photons, entryRadius);
}

void PhotonGICache::Preprocess() {
	TracePhotons();
	BuildPhotonsBVH();
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
