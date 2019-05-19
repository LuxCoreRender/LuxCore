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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>

#include <memory>

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#include "luxrays/utils/atomic.h"
#include "slg/samplers/metropolis.h"
#include "slg/lights/visibility/envlightvisibilitycache.h"
#include "slg/film/imagepipeline/plugins/gaussianblur3x3.h"
#include "slg/utils/film2sceneradius.h"
#include "slg/utils/scenevisibility.h"

using namespace std;
using namespace luxrays;
using namespace slg;
OIIO_NAMESPACE_USING

//------------------------------------------------------------------------------
// ELVCOctree
//------------------------------------------------------------------------------

ELVCOctree::ELVCOctree(const vector<ELVCVisibilityParticle> &entries,
		const BBox &bbox, const float r, const float normAngle, const u_int md) :
	IndexOctree(entries, bbox, r, normAngle, md) {
}

ELVCOctree::~ELVCOctree() {
}

u_int ELVCOctree::GetNearestEntry(const Point &p, const Normal &n,
		const bool isVolume) const {
	u_int nearestEntryIndex = NULL_INDEX;
	float nearestDistance2 = entryRadius2;

	GetNearestEntryImpl(&root, worldBBox, p, n, isVolume,
			nearestEntryIndex, nearestDistance2);
	
	return nearestEntryIndex;
}

void ELVCOctree::GetNearestEntryImpl(const IndexOctreeNode *node, const BBox &nodeBBox,
		const Point &p, const Normal &n, const bool isVolume,
		u_int &nearestEntryIndex, float &nearestDistance2) const {
	// Check if I'm inside the node bounding box
	if (!nodeBBox.Inside(p))
		return;

	// Check every entry in this node
	for (auto const &entryIndex : node->entriesIndex) {
		const ELVCVisibilityParticle &entry = allEntries[entryIndex];

		const float distance2 = DistanceSquared(p, entry.p);
		if ((distance2 < nearestDistance2) && (entry.isVolume == isVolume) &&
				(isVolume || (Dot(n, entry.n) >= entryNormalCosAngle))) {
			// I have found a valid nearer entry
			nearestEntryIndex = entryIndex;
			nearestDistance2 = distance2;
		}
	}

	// Check the children too
	const Point pMid = .5 * (nodeBBox.pMin + nodeBBox.pMax);
	for (u_int child = 0; child < 8; ++child) {
		if (node->children[child]) {
			const BBox childBBox = ChildNodeBBox(child, nodeBBox, pMid);

			GetNearestEntryImpl(node->children[child], childBBox,
					p, n, isVolume, nearestEntryIndex, nearestDistance2);
		}
	}
}

//------------------------------------------------------------------------------
// Env. light visibility cache builder
//------------------------------------------------------------------------------

EnvLightVisibilityCache::EnvLightVisibilityCache(const Scene *scn, const EnvLightSource *envl,
		ImageMap *li, const ELVCParams &p) :
		scene(scn), envLight(envl), luminanceMapImage(li), params(p) {
}

EnvLightVisibilityCache::~EnvLightVisibilityCache() {
}

bool EnvLightVisibilityCache::IsCacheEnabled(const BSDF &bsdf) const {
	const BSDFEvent eventTypes = bsdf.GetEventTypes();

	if ((eventTypes & TRANSMIT) || (eventTypes & SPECULAR) ||
			((eventTypes & GLOSSY) && (bsdf.GetGlossiness() < params.glossinessUsageThreshold)))
		return false;
	else
		return true;
}

//------------------------------------------------------------------------------
// EvaluateBestRadius
//------------------------------------------------------------------------------

namespace slg {

class ELVCFilm2SceneRadiusValidator : public Film2SceneRadiusValidator {
public:
	ELVCFilm2SceneRadiusValidator(const EnvLightVisibilityCache &c) : elvc(c) { }
	virtual ~ELVCFilm2SceneRadiusValidator() { }
	
	virtual bool IsValid(const BSDF &bsdf) const {
		return elvc.IsCacheEnabled(bsdf);
	}

private:
	const EnvLightVisibilityCache &elvc;
};

}

float EnvLightVisibilityCache::EvaluateBestRadius() {
	SLG_LOG("EnvLightVisibilityCache evaluating best radius");

	// The percentage of image plane to cover with the radius
	const float imagePlaneRadius = .04f;

	// The old default radius: 15cm
	const float defaultRadius = .15f;
	
	ELVCFilm2SceneRadiusValidator validator(*this);

	return Film2SceneRadius(scene, imagePlaneRadius, defaultRadius,
			params.maxPathDepth,
			// 0.0/-1.0 disables time evaluation
			0.f, -1.f,
			&validator);
}

//------------------------------------------------------------------------------
// Build
//------------------------------------------------------------------------------

namespace slg {

class ELVCSceneVisibility : public SceneVisibility<ELVCVisibilityParticle> {
public:
	ELVCSceneVisibility(EnvLightVisibilityCache &cache) :
		SceneVisibility(cache.scene, cache.visibilityParticles,
				cache.params.maxPathDepth, cache.params.maxSampleCount,
				cache.params.targetHitRate,
				cache.params.lookUpRadius, cache.params.lookUpNormalAngle,
				// To disable time evaluation
				0.f, -1.f),
		elvc(cache) {
	}
	virtual ~ELVCSceneVisibility() { }
	
protected:
	virtual IndexOctree<ELVCVisibilityParticle> *AllocOctree() const {
		return new ELVCOctree(visibilityParticles, scene->dataSet->GetBBox(),
				lookUpRadius, lookUpNormalAngle);
	}

	virtual bool ProcessHitPoint(const BSDF &bsdf, vector<ELVCVisibilityParticle> &visibilityParticles) const {
		if (elvc.IsCacheEnabled(bsdf)) {

			// Check if I have to flip the normal
			const Normal surfaceNormal = (bsdf.hitPoint.intoObject ?
				1.f : -1.f) * bsdf.hitPoint.geometryN;

			visibilityParticles.push_back(ELVCVisibilityParticle(bsdf.hitPoint.p,
					surfaceNormal, bsdf.IsVolume()));
			
			return false;
		} else
			return true;
	}

	virtual bool ProcessVisibilityParticle(const ELVCVisibilityParticle &vp,
			vector<ELVCVisibilityParticle> &visibilityParticles,
			IndexOctree<ELVCVisibilityParticle> *octree, const float maxDistance2) const {
		ELVCOctree *particlesOctree = (ELVCOctree *)octree;

		// Check if a cache entry is available for this point
		const u_int entryIndex = particlesOctree->GetNearestEntry(vp.p, vp.n, vp.isVolume);

		if (entryIndex == NULL_INDEX) {
			// Add as a new entry
			visibilityParticles.push_back(vp);
			particlesOctree->Add(visibilityParticles.size() - 1);

			return false;
		} else {
			ELVCVisibilityParticle &entry = visibilityParticles[entryIndex];
			const float distance2 = DistanceSquared(vp.p, entry.p);

			if (distance2 > maxDistance2) {
				// Add as a new entry
				visibilityParticles.push_back(vp);
				particlesOctree->Add(visibilityParticles.size() - 1);
				
				return false;
			} else {
				entry.Add(vp.p, vp.n);
				
				return true;
			}
		}
	}
	
	EnvLightVisibilityCache &elvc;
};

}

void EnvLightVisibilityCache::TraceVisibilityParticles() {
	ELVCSceneVisibility elvcVisibility(*this);
	
	elvcVisibility.Build();

	if (visibilityParticles.size() == 0) {
		// Something wrong, nothing in the scene is visible and/or cache enabled
		return;
	}
}

//------------------------------------------------------------------------------
// Build
//------------------------------------------------------------------------------

void EnvLightVisibilityCache::Build() {
	params.lookUpNormalCosAngle = cosf(Radians(params.lookUpNormalAngle));

	//--------------------------------------------------------------------------
	// Evaluate best radius if required
	//--------------------------------------------------------------------------

	if (params.lookUpRadius == 0.f) {
		params.lookUpRadius = EvaluateBestRadius();
		SLG_LOG("EnvLightVisibilityCache best cache radius: " << params.lookUpRadius);
	}

	params.lookUpRadius2 = Sqr(params.lookUpRadius);
	
	//--------------------------------------------------------------------------
	// Build the list of visible points (i.e. the cache points)
	//--------------------------------------------------------------------------
	
	TraceVisibilityParticles();
}
