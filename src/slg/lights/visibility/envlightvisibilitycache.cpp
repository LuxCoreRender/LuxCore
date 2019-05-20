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
#if defined(_OPENMP)
#include <omp.h>
#endif

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#include "luxrays/core/bvh/bvhbuild.h"
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
		const BBox &bbox, const float r, const u_int md) :
	IndexOctree(entries, bbox, r, 360.f, md) {
}

ELVCOctree::~ELVCOctree() {
}

u_int ELVCOctree::GetNearestEntry(const Point &p) const {
	u_int nearestEntryIndex = NULL_INDEX;
	float nearestDistance2 = entryRadius2;

	GetNearestEntryImpl(&root, worldBBox, p, nearestEntryIndex, nearestDistance2);
	
	return nearestEntryIndex;
}

void ELVCOctree::GetNearestEntryImpl(const IndexOctreeNode *node, const BBox &nodeBBox,
		const Point &p, u_int &nearestEntryIndex, float &nearestDistance2) const {
	// Check if I'm inside the node bounding box
	if (!nodeBBox.Inside(p))
		return;

	// Check every entry in this node
	for (auto const &entryIndex : node->entriesIndex) {
		const ELVCVisibilityParticle &entry = allEntries[entryIndex];

		const float distance2 = DistanceSquared(p, entry.p);
		if (distance2 < nearestDistance2) {
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
					p, nearestEntryIndex, nearestDistance2);
		}
	}
}

//------------------------------------------------------------------------------
// Env. light visibility cache builder
//------------------------------------------------------------------------------

EnvLightVisibilityCache::EnvLightVisibilityCache(const Scene *scn, const EnvLightSource *envl,
		ImageMap *li, const ELVCParams &p) :
		scene(scn), envLight(envl), luminanceMapImage(li), params(p),
		cacheEntriesBVH(nullptr) {
}

EnvLightVisibilityCache::~EnvLightVisibilityCache() {
	delete cacheEntriesBVH;
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
			0.f, 1.f,
			&validator);
}

//------------------------------------------------------------------------------
// Scene visibility
//------------------------------------------------------------------------------

namespace slg {

class ELVCSceneVisibility : public SceneVisibility<ELVCVisibilityParticle> {
public:
	ELVCSceneVisibility(EnvLightVisibilityCache &cache) :
		SceneVisibility(cache.scene, cache.visibilityParticles,
				cache.params.maxPathDepth, cache.params.maxSampleCount,
				cache.params.targetHitRate,
				cache.params.lookUpRadius, 360.f,
				0.f, 1.f),
		elvc(cache) {
	}
	virtual ~ELVCSceneVisibility() { }
	
protected:
	virtual IndexOctree<ELVCVisibilityParticle> *AllocOctree() const {
		return new ELVCOctree(visibilityParticles, scene->dataSet->GetBBox(),
				lookUpRadius, lookUpNormalAngle);
	}

	virtual bool ProcessHitPoint(const BSDF &bsdf, const PathVolumeInfo &volInfo,
			vector<ELVCVisibilityParticle> &visibilityParticles) const {
		if (elvc.IsCacheEnabled(bsdf)) {
			visibilityParticles.push_back(ELVCVisibilityParticle(bsdf.hitPoint.p, volInfo));
			
			return false;
		} else
			return true;
	}

	virtual bool ProcessVisibilityParticle(const ELVCVisibilityParticle &vp,
			vector<ELVCVisibilityParticle> &visibilityParticles,
			IndexOctree<ELVCVisibilityParticle> *octree, const float maxDistance2) const {
		ELVCOctree *particlesOctree = (ELVCOctree *)octree;

		// Check if a cache entry is available for this point
		const u_int entryIndex = particlesOctree->GetNearestEntry(vp.p);

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
				entry.Add(vp.p, vp.volInfo);
				
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
// Build cache entries
//------------------------------------------------------------------------------

void EnvLightVisibilityCache::BuildCacheEntry(const u_int entryIndex) {
	const ELVCVisibilityParticle &visibilityParticle = visibilityParticles[entryIndex];
	ELVCCacheEntry &cacheEntry = cacheEntries[entryIndex];

	// Set up the default cache entry
	cacheEntry.p = visibilityParticle.p;
	cacheEntry.visibilityMap = nullptr;
	
	// Allocate the map storage
	unique_ptr<ImageMap> visibilityMapImage(ImageMap::AllocImageMap<float>(1.f, 1,
			params.width, params.height, ImageMapStorage::REPEAT));
	float *visibilityMap = (float *)visibilityMapImage->GetStorage()->GetPixelsData();

	const u_int passCount = 16;

	// Trace all shadow rays
	for (u_int pass = 0; pass < passCount; ++pass) {
		const float u0 = RadicalInverse(pass + 1, 3);
		const float u3 = RadicalInverse(pass + 1, 11);
		const float u4 = RadicalInverse(pass + 1, 13);

		// Pick a sampling point index
		const u_int pointIndex = Min<u_int>(Floor2UInt(u0 * visibilityParticle.pList.size()), visibilityParticle.pList.size() - 1);

		for (u_int y = 0; y < params.height; ++y) {
			for (u_int x = 0; x < params.width; ++x) {
				// Using pass + 1 to avoid 0.0 value
				const float u1 =  (x  + RadicalInverse(pass + 1, 5)) / (float)params.width;
				const float u2 =  (y  + RadicalInverse(pass + 1, 7)) / (float)params.height;

				// Pick a sampling point
				const Point samplingPoint = visibilityParticle.pList[pointIndex];

				// Pick a sampling direction
				Vector samplingDir;
				EnvLightSource::FromLatLongMapping(u1, u2, &samplingDir);

				// Set up the shadow ray
				Ray shadowRay(samplingPoint, samplingDir);
				shadowRay.time = u3;

				// Check if the light source is visible
			
				RayHit shadowRayHit;
				BSDF shadowBsdf;
				Spectrum connectionThroughput;

				PathVolumeInfo volInfo = visibilityParticle.volInfoList[pointIndex];
				if (!scene->Intersect(NULL, false, false, &volInfo, u4, &shadowRay,
						&shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					// Nothing was hit, the light source is visible
					visibilityMap[x + y * params.width] += connectionThroughput.Y();
				}
			}
		}
	}

	// Filter the map
	const u_int mapPixelCount = params.width * params.height;
	vector<float> tmpBuffer(mapPixelCount);
	GaussianBlur3x3FilterPlugin::ApplyBlurFilter(params.width, params.height,
				&visibilityMap[0], &tmpBuffer[0],
				.5f, 1.f, .5f);

	// Check if I have set the lower hemisphere to 0.0
	if (params.sampleUpperHemisphereOnly) {
		for (u_int y = params.height / 2 + 1; y < params.height; ++y)
			for (u_int x = 0; x < params.width; ++x)
				visibilityMap[x + y * params.width] = 0.f;
	}

	// Normalize and multiply for normalized image luminance
	float visibilityMaxVal = 0.f;
	for (u_int i = 0; i < mapPixelCount; ++i)
		visibilityMaxVal = Max(visibilityMaxVal, visibilityMap[i]);

	if (visibilityMaxVal == 0.f) {
		// In this case I will use the normal map
		return;
	}

	// For some debug, save the map to a file
	if (entryIndex % 100 == 0) {
		ImageSpec spec(params.width, params.height, 3, TypeDesc::FLOAT);
		ImageBuf buffer(spec);
		for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
			u_int x = it.x();
			u_int y = it.y();
			float *pixel = (float *)buffer.pixeladdr(x, y, 0);
			const float v = visibilityMap[x + y * params.width];
			pixel[0] = v;
			pixel[1] = v;
			pixel[2] = v;
		}
		buffer.write("visibiliy-" + ToString(entryIndex) + ".exr");
	}

	const float invVisibilityMaxVal = 1.f / visibilityMaxVal;
	for (u_int i = 0; i < mapPixelCount; ++i) {
		const float normalizedVisVal = visibilityMap[i] * invVisibilityMaxVal;

		visibilityMap[i] = normalizedVisVal;
	}

	if (luminanceMapImage) {
		const ImageMapStorage *luminanceMapStorage = luminanceMapImage->GetStorage();

		// For some debug, save the map to a file
		if (entryIndex % 100 == 0) {
			ImageSpec spec(params.width, params.height, 3, TypeDesc::FLOAT);
			ImageBuf buffer(spec);
			for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
				u_int x = it.x();
				u_int y = it.y();
				float *pixel = (float *)buffer.pixeladdr(x, y, 0);
				const float v = luminanceMapStorage->GetFloat(x + y * params.width);
				pixel[0] = v;
				pixel[1] = v;
				pixel[2] = v;
			}
			buffer.write("luminance-" + ToString(entryIndex) + ".exr");
		}

		float luminanceMaxVal = 0.f;
		for (u_int i = 0; i < mapPixelCount; ++i)
			luminanceMaxVal = Max(visibilityMaxVal, luminanceMapStorage->GetFloat(i));

		const float invLuminanceMaxVal = 1.f / luminanceMaxVal;
		for (u_int i = 0; i < mapPixelCount; ++i) {
			const float normalizedLumiVal = luminanceMapStorage->GetFloat(i) * invLuminanceMaxVal;

			visibilityMap[i] *= normalizedLumiVal;
		}
	}

	// For some debug, save the map to a file
	if (entryIndex % 100 == 0) {
		ImageSpec spec(params.width, params.height, 3, TypeDesc::FLOAT);
		ImageBuf buffer(spec);
		for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
			u_int x = it.x();
			u_int y = it.y();
			float *pixel = (float *)buffer.pixeladdr(x, y, 0);
			const float v = visibilityMap[x + y * params.width];
			pixel[0] = v;
			pixel[1] = v;
			pixel[2] = v;
		}
		buffer.write("map-" + ToString(entryIndex) + ".exr");
	}

	cacheEntry.visibilityMap = new Distribution2D(&visibilityMap[0], params.width, params.height);
}

void EnvLightVisibilityCache::BuildCacheEntries() {
	SLG_LOG("EnvLightVisibilityCache building cache entries: " << visibilityParticles.size());

	double lastPrintTime = WallClockTime();
	atomic<u_int> counter(0);

	cacheEntries.resize(visibilityParticles.size());
	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < visibilityParticles.size(); ++i) {
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
				SLG_LOG("EnvLightVisibilityCache initializing distribution: " << counter << "/" << visibilityParticles.size() <<" (" << (u_int)((100.0 * counter) / visibilityParticles.size()) << "%)");
				lastPrintTime = now;
			}
		}

		BuildCacheEntry(i);

		++counter;
	}
}

//------------------------------------------------------------------------------
// ELVCBvh
//------------------------------------------------------------------------------

ELVCBvh::ELVCBvh(const vector<ELVCCacheEntry> *entries, const float radius) :
		IndexBvh(entries, radius) {
}

ELVCBvh::~ELVCBvh() {
}

const ELVCCacheEntry *ELVCBvh::GetNearestEntry(const Point &p) const {
	const ELVCCacheEntry *nearestEntry = nullptr;
	float nearestDistance2 = entryRadius2;

	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(arrayNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		const slg::ocl::IndexBVHArrayNode &node = arrayNodes[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const ELVCCacheEntry *entry = &((*allEntries)[node.entryLeaf.entryIndex]);

			const float distance2 = DistanceSquared(p, entry->p);
			if (distance2 < nearestDistance2) {
				// I have found a valid nearer entry
				nearestEntry = entry;
				nearestDistance2 = distance2;
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (p.x >= node.bvhNode.bboxMin[0] && p.x <= node.bvhNode.bboxMax[0] &&
					p.y >= node.bvhNode.bboxMin[1] && p.y <= node.bvhNode.bboxMax[1] &&
					p.z >= node.bvhNode.bboxMin[2] && p.z <= node.bvhNode.bboxMax[2])
				++currentNode;
			else {
				// I don't need to use IndexBVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}

	return nearestEntry;
}

//------------------------------------------------------------------------------
// Build
//------------------------------------------------------------------------------

void EnvLightVisibilityCache::Build() {
	//--------------------------------------------------------------------------
	// Evaluate best radius if required
	//--------------------------------------------------------------------------

	if (params.lookUpRadius == 0.f) {
		params.lookUpRadius = EvaluateBestRadius();
		SLG_LOG("EnvLightVisibilityCache best cache radius: " << params.lookUpRadius);
	}
	
	//--------------------------------------------------------------------------
	// Build the list of visible points (i.e. the cache points)
	//--------------------------------------------------------------------------
	
	TraceVisibilityParticles();
	
	//--------------------------------------------------------------------------
	// Build cache entries
	//--------------------------------------------------------------------------

	BuildCacheEntries();

	//--------------------------------------------------------------------------
	// Free memory
	//--------------------------------------------------------------------------

	visibilityParticles.clear();
	visibilityParticles.shrink_to_fit();

	//--------------------------------------------------------------------------
	// Build cache entries BVH
	//--------------------------------------------------------------------------

	SLG_LOG("EnvLightVisibilityCache building cache entries BVH");
	cacheEntriesBVH = new ELVCBvh(&cacheEntries, params.lookUpRadius);
}

//------------------------------------------------------------------------------
// GetVisibilityMap
//------------------------------------------------------------------------------

const Distribution2D *EnvLightVisibilityCache::GetVisibilityMap(const Point &p) const {
	const ELVCCacheEntry *entry = cacheEntriesBVH->GetNearestEntry(p);
	if (entry)
		return entry->visibilityMap;
	else
		return nullptr;
}
