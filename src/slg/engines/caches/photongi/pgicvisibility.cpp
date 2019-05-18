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

#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/engines/caches/photongi/pgicoctree.h"
#include "slg/utils/scenevisibility.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TraceVisibilityParticles
//------------------------------------------------------------------------------

namespace slg {

class PGICSceneVisibility : public SceneVisibility<VisibilityParticle> {
public:
	PGICSceneVisibility(PhotonGICache &cache) :
		SceneVisibility(cache.scene, cache.visibilityParticles,
				cache.params.photon.maxPathDepth, cache.params.visibility.maxSampleCount,
				cache.params.visibility.targetHitRate,
				cache.params.visibility.lookUpRadius, cache.params.visibility.lookUpNormalAngle,
				cache.params.photon.timeStart, cache.params.photon.timeEnd),
		pgic(cache) {
	}
	virtual ~PGICSceneVisibility() { }
	
protected:
	virtual IndexOctree<VisibilityParticle> *AllocOctree() const {
		return new PGCIOctree(visibilityParticles, scene->dataSet->GetBBox(),
				lookUpRadius, lookUpNormalAngle);
	}

	virtual bool ProcessHitPoint(const BSDF &bsdf, vector<VisibilityParticle> &visibilityParticles) const {
		if (pgic.IsPhotonGIEnabled(bsdf)) {
			const Spectrum bsdfEvalTotal = bsdf.EvaluateTotal();
			assert (bsdfEvalTotal.IsValid());

			// Check if I have to flip the normal
			const Normal surfaceNormal = (bsdf.hitPoint.intoObject ?
				1.f : -1.f) * bsdf.hitPoint.geometryN;

			visibilityParticles.push_back(VisibilityParticle(bsdf.hitPoint.p,
					surfaceNormal, bsdfEvalTotal, bsdf.IsVolume()));
		}

		return true;
	}

	virtual bool ProcessVisibilityParticle(const VisibilityParticle &vp,
			vector<VisibilityParticle> &visibilityParticles,
			IndexOctree<VisibilityParticle> *octree, const float maxDistance2) const {
		PGCIOctree *particlesOctree = (PGCIOctree *)octree;

		// Check if a cache entry is available for this point
		const u_int entryIndex = particlesOctree->GetNearestEntry(vp.p, vp.n, vp.isVolume);

		if (entryIndex == NULL_INDEX) {
			// Add as a new entry
			visibilityParticles.push_back(vp);
			particlesOctree->Add(visibilityParticles.size() - 1);

			return false;
		} else {
			VisibilityParticle &entry = visibilityParticles[entryIndex];
			const float distance2 = DistanceSquared(vp.p, entry.p);

			if (distance2 > maxDistance2) {
				// Add as a new entry
				visibilityParticles.push_back(vp);
				particlesOctree->Add(visibilityParticles.size() - 1);
				
				return false;
			} else {
				// Update the statistics about the area covered by this entry
				entry.hitsAccumulatedDistance += sqrtf(distance2);
				entry.hitsCount += 1;
				
				return true;
			}
		}
	}
	
	PhotonGICache &pgic;
};

}

void PhotonGICache::TraceVisibilityParticles() {
	PGICSceneVisibility pgicVisibility(*this);
	
	pgicVisibility.Build();

	/*const size_t renderThreadCount = boost::thread::hardware_concurrency();
	vector<TraceVisibilityThread *> renderThreads(renderThreadCount, nullptr);
	SLG_LOG("PhotonGI trace visibility particles thread count: " << renderThreadCount);

	// Initialize the Octree where to store the visibility points
	//
	// I use an Octree because it can be built at runtime while I than switch
	// to a KdTree so I can lookup entries with any radius.
	PGCIOctree *particlesOctree = new PGCIOctree(visibilityParticles, scene->dataSet->GetBBox(),
			params.visibility.lookUpRadius, params.visibility.lookUpNormalAngle);
	boost::mutex particlesOctreeMutex;

	SobolSamplerSharedData visibilitySobolSharedData(131, nullptr);

	boost::atomic<u_int> globalVisibilityParticlesCount(0);
	u_int visibilityCacheLookUp = 0;
	u_int visibilityCacheHits = 0;
	bool visibilityWarmUp = true;

	// Create the visibility particles tracing threads
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i] = new TraceVisibilityThread(*this, i,
				visibilitySobolSharedData,
				particlesOctree, particlesOctreeMutex,
				globalVisibilityParticlesCount,
				visibilityCacheLookUp, visibilityCacheHits,
				visibilityWarmUp);
	}

	// Start visibility particles tracing threads
	for (size_t i = 0; i < renderThreadCount; ++i)
		renderThreads[i]->Start();
	
	// Wait for the end of visibility particles tracing threads
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i]->Join();

		delete renderThreads[i];
	}
	
	visibilityParticles.shrink_to_fit();
	SLG_LOG("PhotonGI visibility total entries: " << visibilityParticles.size());

	// Free the Octree
	delete particlesOctree;*/

	if (visibilityParticles.size() == 0) {
		// Something wrong, nothing in the scene is visible and/or cache enabled
		return;
	}

	// Build the KdTree
	SLG_LOG("PhotonGI building visibility particles KdTree");
	visibilityParticlesKdTree = new PGICKdTree(&visibilityParticles);
}
