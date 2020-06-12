/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include <algorithm>

#if defined(_OPENMP)
#include <omp.h>
#endif

#include <boost/format.hpp>
#include <boost/circular_buffer.hpp>

#include "luxrays/core/geometry/bbox.h"
#include "luxrays/utils/safesave.h"

#include "slg/samplers/sobol.h"
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
#include "slg/lights/strategies/dlscacheimpl/dlscoctree.h"
#include "slg/lights/strategies/dlscacheimpl/dlscbvh.h"
#include "slg/utils/film2sceneradius.h"
#include "slg/utils/scenevisibility.h"

using namespace std;
using namespace luxrays;
using namespace slg;

#define NEIGHBORS_RADIUS_SCALE 1.5f

//------------------------------------------------------------------------------
// DirectLightSamplingCache
//------------------------------------------------------------------------------

DirectLightSamplingCache::DirectLightSamplingCache(const DLSCParams &p) :
		params(p), cacheEntriesBVH(nullptr) {
}

DirectLightSamplingCache::~DirectLightSamplingCache() {
	delete cacheEntriesBVH;
}

bool DirectLightSamplingCache::IsCacheEnabled(const BSDF &bsdf) const {
	const BSDFEvent eventTypes = bsdf.GetEventTypes();

	return !bsdf.IsDelta() && !(eventTypes & SPECULAR);
}

//------------------------------------------------------------------------------
// Estimate best radius
//------------------------------------------------------------------------------

namespace slg {

class DLSCFilm2SceneRadiusValidator : public Film2SceneRadiusValidator {
public:
	DLSCFilm2SceneRadiusValidator(const DirectLightSamplingCache &c) : dlsc(c) { }
	virtual ~DLSCFilm2SceneRadiusValidator() { }
	
	virtual bool IsValid(const BSDF &bsdf) const {
		return dlsc.IsCacheEnabled(bsdf);
	}

private:
	const DirectLightSamplingCache &dlsc;
};

}

float DirectLightSamplingCache::EvaluateBestRadius() {
	SLG_LOG("DirectLightSamplingCache evaluating best radius");

	// The percentage of image plane to cover with the radius
	const float imagePlaneRadius = .1f;

	// The old default radius: 15cm
	const float defaultRadius = .15f;

	DLSCFilm2SceneRadiusValidator validator(*this);

	return Film2SceneRadius(scene,  imagePlaneRadius, defaultRadius, params.visibility.maxPathDepth,
		scene->camera->shutterOpen, scene->camera->shutterClose,
		&validator);
}

//------------------------------------------------------------------------------
// Scene visibility
//------------------------------------------------------------------------------

namespace slg {

class DLSCSceneVisibility : public SceneVisibility<DLSCVisibilityParticle> {
public:
	DLSCSceneVisibility(DirectLightSamplingCache &cache) :
		SceneVisibility(cache.scene, cache.visibilityParticles,
				cache.params.visibility.maxPathDepth, cache.params.visibility.maxSampleCount,
				cache.params.visibility.targetHitRate,
				cache.params.visibility.lookUpRadius, cache.params.visibility.lookUpNormalAngle,
				0.f, 1.f),
		dslc(cache) {
	}
	virtual ~DLSCSceneVisibility() { }
	
protected:
	virtual IndexOctree<DLSCVisibilityParticle> *AllocOctree() const {
		return new DLSCOctree(visibilityParticles, scene->dataSet->GetBBox(),
				lookUpRadius, lookUpNormalAngle);
	}

	virtual bool ProcessHitPoint(const BSDF &bsdf, const PathVolumeInfo &volInfo,
			vector<DLSCVisibilityParticle> &visibilityParticles) const {
		if (dslc.IsCacheEnabled(bsdf))
			visibilityParticles.push_back(DLSCVisibilityParticle(bsdf, volInfo));

		return true;
	}

	virtual bool ProcessVisibilityParticle(const DLSCVisibilityParticle &vp,
			vector<DLSCVisibilityParticle> &visibilityParticles,
			IndexOctree<DLSCVisibilityParticle> *octree, const float maxDistance2) const {
		DLSCOctree *particlesOctree = (DLSCOctree *)octree;

		// Check if a cache entry is available for this point
		const u_int entryIndex = particlesOctree->GetNearestEntry(vp.bsdfList[0].hitPoint.p,
				vp.bsdfList[0].hitPoint.GetLandingShadeN(), vp.bsdfList[0].IsVolume());

		if (entryIndex == NULL_INDEX) {
			// Add as a new entry
			visibilityParticles.push_back(vp);
			particlesOctree->Add(visibilityParticles.size() - 1);

			return false;
		} else {
			DLSCVisibilityParticle &entry = visibilityParticles[entryIndex];
			const float distance2 = DistanceSquared(vp.bsdfList[0].hitPoint.p, entry.bsdfList[0].hitPoint.p);

			if (distance2 > maxDistance2) {
				// Add as a new entry
				visibilityParticles.push_back(vp);
				particlesOctree->Add(visibilityParticles.size() - 1);
				
				return false;
			} else {
				entry.Add(vp);
				
				return true;
			}
		}
	}
	
	DirectLightSamplingCache &dslc;
};

}

void DirectLightSamplingCache::TraceVisibilityParticles() {
	DLSCSceneVisibility dlscVisibility(*this);
	
	dlscVisibility.Build();

	if (visibilityParticles.size() == 0) {
		// Something wrong, nothing in the scene is visible and/or cache enabled
		return;
	}
}

//------------------------------------------------------------------------------
// Build the list of cache entries
//------------------------------------------------------------------------------

void DirectLightSamplingCache::InitCacheEntry(const u_int entryIndex) {
	cacheEntries[entryIndex] = DLSCacheEntry(visibilityParticles[entryIndex].bsdfList[0]);
}

float DirectLightSamplingCache::SampleLight(const DLSCVisibilityParticle &visibilityParticle,
		const LightSource *light, const u_int pass) const {
	const float u1 = RadicalInverse(pass, 3);
	const float u2 = RadicalInverse(pass, 5);
	const float u3 = RadicalInverse(pass, 7);
	const float u4 = RadicalInverse(pass, 11);
	const float time = RadicalInverse(pass, 13);

	// Select a sampling point
	const size_t bsdfListSize = visibilityParticle.bsdfList.size();
	const u_int bsdfListIndexIndex = Min<u_int>(Floor2UInt(u4 * bsdfListSize), bsdfListSize - 1);
	const BSDF &samplingBSDF = visibilityParticle.bsdfList[bsdfListIndexIndex];

	Ray shadowRay;
	float directPdfW;
	Spectrum lightRadiance = light->Illuminate(*scene, samplingBSDF,
			time, u1, u2, u3, shadowRay, directPdfW);
	assert (!lightRadiance.IsNaN() && !lightRadiance.IsInf());

	if (!lightRadiance.Black() && ((samplingBSDF.GetEventTypes() & TRANSMIT) ||
			(Dot(shadowRay.d, samplingBSDF.hitPoint.GetLandingShadeN()) > 0.f))) {
		assert (!isnan(directPdfW) && !isinf(directPdfW));

		const float u5 = RadicalInverse(pass, 17);

		RayHit shadowRayHit;
		BSDF shadowBsdf;
		Spectrum connectionThroughput;

		// Check if the light source is visible
		PathVolumeInfo volInfo = visibilityParticle.volInfoList[bsdfListIndexIndex];
		if (!scene->Intersect(nullptr, EYE_RAY | SHADOW_RAY, &volInfo, u5, &shadowRay,
				&shadowRayHit, &shadowBsdf, &connectionThroughput, nullptr,
				nullptr, true)) {
			// It is
			const Spectrum incomingRadiance = connectionThroughput * (lightRadiance / directPdfW);

			assert (!incomingRadiance.IsNaN() && !incomingRadiance.IsInf());

			return incomingRadiance.Y();
		}
	}
	
	return 0.f;
}

void DirectLightSamplingCache::ComputeCacheEntryReceivedLuminance(const u_int entryIndex) {
	const DLSCVisibilityParticle &visibilityParticle = visibilityParticles[entryIndex];
	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();

	//--------------------------------------------------------------------------
	// Build the list of luminance received from each light source
	//--------------------------------------------------------------------------

	vector<float> &entryReceivedLuminance = cacheEntriesReceivedLuminance[entryIndex];

	// For some Debugging
	//SLG_LOG("DLSC entry #" << entryIndex);

	for (u_int lightIndex = 0; lightIndex < lights.size(); ++lightIndex) {
		const LightSource *light = lights[lightIndex];
	
		// Check if the light source uses direct light sampling
		if (!light->IsDirectLightSamplingEnabled()) {
			// This light source is excluded from direct light sampling
			continue;
		}

		// Check if I can avoid to trace all shadow rays
		bool isAlwaysInShadow = true;
		for (const BSDF &bsdf : visibilityParticle.bsdfList) {
			if (!light->IsAlwaysInShadow(*scene, bsdf.hitPoint.p, bsdf.hitPoint.GetLandingShadeN())) {
				isAlwaysInShadow = false;
				break;
			}
		}

		if (isAlwaysInShadow) {
			// It is always in shadow
			continue;
		}
		
		// Most env. light sources are hard to sample and can lead to wrong cache
		// entries. Use not less than 512 samples for them.
		const u_int currentWarmUpSamples = (light->IsEnvironmental() && (light->GetType() != TYPE_SUN)) ?
			Max(params.entry.warmUpSamples, 512u) : params.entry.warmUpSamples;

		float receivedLuminance = 0.f;
		boost::circular_buffer<float> entryReceivedLuminancePreviousStep(currentWarmUpSamples, 0.f);

		u_int pass = 0;
		for (; pass < params.entry.maxPasses; ++pass) {
			receivedLuminance += SampleLight(visibilityParticle, light, pass);

			const float currentStepValue = receivedLuminance / pass;

			if (pass > currentWarmUpSamples) {
				// Convergence test, check if it is time to stop sampling
				// this light source. Using an 1% threshold.

				const float previousStepValue = entryReceivedLuminancePreviousStep[0];
				const float convergence = fabsf(currentStepValue - previousStepValue);

				const float threshold =  currentStepValue * params.entry.convergenceThreshold;

				if ((convergence == 0.f) || (convergence < threshold)) {
					// Done
					break;
				}
			}

			entryReceivedLuminancePreviousStep.push_back(currentStepValue);
		
#ifdef WIN32
			// Work around Windows bad scheduling
			boost::this_thread::yield();
#endif
		}

		entryReceivedLuminance[lightIndex] = receivedLuminance / pass;

		// For some Debugging
		//SLG_LOG("Light #" << lightIndex << ": " << entryReceivedLuminance[lightIndex] <<	" (pass " << pass << ")");
	}
}

void DirectLightSamplingCache::BuildCacheEntryLightDistribution(const u_int entryIndex, const DLSCBvh &bvh) {
	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();

	DLSCacheEntry &entry = cacheEntries[entryIndex];
	vector<float> entryReceivedLuminance(lights.size(), 0.f);
	
	// Look for all neighbor particles
	vector<u_int> allNearEntryIndices;
	bvh.GetAllNearEntries(allNearEntryIndices, entry.p, entry.n, entry.isVolume);

	// Merge near entries (including my self)
	for (auto index : allNearEntryIndices) {
		assert (Distance(cacheEntries[entryIndex].p, cacheEntries[index].p) <= NEIGHBORS_RADIUS_SCALE * params.visibility.lookUpRadius);

		const vector<float> &neighborEntryReceivedLuminance = cacheEntriesReceivedLuminance[index];

		for (u_int i = 0; i < lights.size(); ++i)
			entryReceivedLuminance[i] += neighborEntryReceivedLuminance[i];
	}
	
	const float scale = 1.f / (allNearEntryIndices.size());
	for (u_int i = 0; i < lights.size(); ++i)
		entryReceivedLuminance[i] *= scale;

	// Look for the max. luminance value	
	float maxLuminanceValue = 0.f;
	for (auto const &l : entryReceivedLuminance)
		maxLuminanceValue = Max(maxLuminanceValue, l);

	// If not receives light, I revert to normal light sampling based on power
	if (maxLuminanceValue > 0.f) {
		// Normalize and place a lower cap to received luminance (2.5% of max. value)

		const float invMaxLuminanceValue = 1.f / maxLuminanceValue;
		for (auto &l : entryReceivedLuminance)
			l = Max(l * invMaxLuminanceValue , .025f);

		cacheEntries[entryIndex].lightsDistribution = new Distribution1D(&entryReceivedLuminance[0], entryReceivedLuminance.size());
	}
}

void DirectLightSamplingCache::BuildCacheEntries() {
	//--------------------------------------------------------------------------
	// Print the number of light with enabled direct light sampling
	//--------------------------------------------------------------------------

	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();
	u_int dlsLightCount = 0;
	for (u_int lightIndex = 0; lightIndex < lights.size(); ++lightIndex) {
		const LightSource *light = lights[lightIndex];
	
		// Check if the light source uses direct light sampling
		if (light->IsDirectLightSamplingEnabled())
			++dlsLightCount;
	}
	SLG_LOG("Building direct light sampling cache: filling cache entries with " << dlsLightCount << " light sources");

	//--------------------------------------------------------------------------
	// Initialize visibilityParticlesReceivedLuminance vector
	//--------------------------------------------------------------------------

	cacheEntriesReceivedLuminance.resize(visibilityParticles.size());
	for (u_int visibilityParticleIndex = 0; visibilityParticleIndex < visibilityParticles.size(); ++visibilityParticleIndex)
		cacheEntriesReceivedLuminance[visibilityParticleIndex].resize(lights.size(), 0.f);
		
	//--------------------------------------------------------------------------
	// Initialize compute the cache entries received luminance
	//--------------------------------------------------------------------------

	{
		const double startTime = WallClockTime();
		double lastPrintTime = startTime;
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
					SLG_LOG("DirectLightSamplingCache compute received luminance: " << counter << "/" << visibilityParticles.size() <<" (" <<
							(boost::format("%.2f entries/sec, ") % (counter / (now - startTime))) <<
							(u_int)((100.0 * counter) / visibilityParticles.size()) << "%)");
					lastPrintTime = now;
				}
			}

			InitCacheEntry(i);
			ComputeCacheEntryReceivedLuminance(i);

			++counter;
		}
	}
	//--------------------------------------------------------------------------
	// Merge cache entries received luminance and initialize light distributions
	//--------------------------------------------------------------------------

	{
		// Build a bvh to find all neighbor entries (i.e. distance < NEIGHBORS_RADIUS_SCALE * radius)
		DLSCBvh bvh(&cacheEntries, NEIGHBORS_RADIUS_SCALE * params.visibility.lookUpRadius,
				params.visibility.lookUpNormalAngle);

		const double startTime = WallClockTime();
		double lastPrintTime = startTime;
		atomic<u_int> counter(0);

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
					SLG_LOG("DirectLightSamplingCache build light distribution: " << counter << "/" << visibilityParticles.size() <<" (" <<
							(boost::format("%.2f entries/sec, ") % (counter / (now - startTime))) <<
							(u_int)((100.0 * counter) / visibilityParticles.size()) << "%)");
					lastPrintTime = now;
				}
			}

			BuildCacheEntryLightDistribution(i, bvh);

			++counter;
		}
	}
}

//------------------------------------------------------------------------------
// Build
//------------------------------------------------------------------------------

void DirectLightSamplingCache::Build(const Scene *scn) {
	scene = scn;

	// This check is required because FILESAVER engine doesn't
	// initialize any accelerator
	if (!scene->dataSet->GetAccelerator()) {
		SLG_LOG("DirectLightSamplingCache is not built");
		return;
	}

	if (scene->lightDefs.GetSize() == 0)
		return;

	SLG_LOG("Building DirectLightSamplingCache");

	//--------------------------------------------------------------------------
	// Load the persistent cache file if required
	//--------------------------------------------------------------------------

	if (params.persistent.fileName != "") {
		// Check if the file already exist
		if (boost::filesystem::exists(params.persistent.fileName)) {
			// Load the cache from the file
			LoadPersistentCache(params.persistent.fileName);

			return;
		}
		
		// The file doesn't exist so I have to go trough normal pre-processing
	}

	//--------------------------------------------------------------------------
	// Evaluate best radius if required
	//--------------------------------------------------------------------------

	if (params.visibility.lookUpRadius == 0.f) {
		params.visibility.lookUpRadius = EvaluateBestRadius();
		SLG_LOG("DirectLightSamplingCache best radius: " << params.visibility.lookUpRadius);
	}

	//--------------------------------------------------------------------------
	// Build the list of visible points (i.e. the cache points)
	//--------------------------------------------------------------------------
	
	TraceVisibilityParticles();

	//--------------------------------------------------------------------------
	// Build cache entries
	//--------------------------------------------------------------------------

	if (visibilityParticles.size() > 0)
		BuildCacheEntries();

	//--------------------------------------------------------------------------
	// Free memory
	//--------------------------------------------------------------------------

	visibilityParticles.clear();
	visibilityParticles.shrink_to_fit();
	cacheEntriesReceivedLuminance.clear();
	cacheEntriesReceivedLuminance.shrink_to_fit();

	//--------------------------------------------------------------------------
	// Build cache entries BVH
	//--------------------------------------------------------------------------

	if (cacheEntries.size() > 0) {
		SLG_LOG("DirectLightSamplingCache building cache entries BVH");
		cacheEntriesBVH = new DLSCBvh(&cacheEntries, params.visibility.lookUpRadius,
				params.visibility.lookUpNormalAngle);
	} else
		SLG_LOG("WARNING: DirectLightSamplingCache has an empty cache");

	//--------------------------------------------------------------------------
	// Check if I have to save the persistent cache
	//--------------------------------------------------------------------------

	if (params.persistent.fileName != "")
		SavePersistentCache(params.persistent.fileName);

	// Export the entries for debugging
	//DebugExport("entries-point.scn", entryRadius * .05f);
}

const Distribution1D *DirectLightSamplingCache::GetLightDistribution(const luxrays::Point &p,
		const luxrays::Normal &n, const bool isVolume) const {
	if (cacheEntriesBVH) {
		const DLSCacheEntry *entry = cacheEntriesBVH->GetNearestEntry(p, n, isVolume);

		if (entry)
			return entry->lightsDistribution;
	}
	
	return nullptr;
}

void DirectLightSamplingCache::DebugExport(const string &fileName, const float sphereRadius) const {
	Properties prop;

	prop <<
			Property("scene.materials.dlsc_material.type")("matte") <<
			Property("scene.materials.dlsc_material.kd")("0.75 0.75 0.75") <<
			Property("scene.materials.dlsc_material_red.type")("matte") <<
			Property("scene.materials.dlsc_material_red.kd")("0.75 0.0 0.0") <<
			Property("scene.materials.dlsc_material_red.emission")("0.25 0.0 0.0");

	for (u_int i = 0; i < cacheEntries.size(); ++i) {
		const DLSCacheEntry &entry = cacheEntries[i];
		if (entry.lightsDistribution)
			prop << Property("scene.objects.dlsc_entry_" + ToString(i) + ".material")("dlsc_material_red");
		else
			prop << Property("scene.objects.dlsc_entry_" + ToString(i) + ".material")("dlsc_material");

		prop <<
			Property("scene.objects.dlsc_entry_" + ToString(i) + ".ply")("scenes/simple/sphere.ply") <<
			Property("scene.objects.dlsc_entry_" + ToString(i) + ".transformation")(Matrix4x4(
				sphereRadius, 0.f, 0.f, entry.p.x,
				0.f, sphereRadius, 0.f, entry.p.y,
				0.f, 0.f, sphereRadius, entry.p.z,
				0.f, 0.f, 0.f, 1.f));
	}

	prop.Save(fileName);
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

void DirectLightSamplingCache::LoadPersistentCache(const std::string &fileName) {
	SLG_LOG("Loading persistent EnvLightVisibility cache: " + fileName);

	SerializationInputFile sif(fileName);

	sif.GetArchive() >> params;

	sif.GetArchive() >> cacheEntries;
	sif.GetArchive() >> cacheEntriesBVH;

	visibilityParticles.clear();
	visibilityParticles.shrink_to_fit();
	
	if (!sif.IsGood())
		throw runtime_error("Error while loading DirectLightSamplingCache persistent cache: " + fileName);
}

void DirectLightSamplingCache::SavePersistentCache(const std::string &fileName) {
	SLG_LOG("Saving persistent DirectLightSamplingCache cache: " + fileName);

	SafeSave safeSave(fileName);
	{
		SerializationOutputFile sof(params.persistent.safeSave ? safeSave.GetSaveFileName() : fileName);

		sof.GetArchive() << params;

		sof.GetArchive() << cacheEntries;
		sof.GetArchive() << cacheEntriesBVH;

		visibilityParticles.clear();
		visibilityParticles.shrink_to_fit();

		if (!sof.IsGood())
			throw runtime_error("Error while saving DirectLightSamplingCache persistent cache: " + fileName);

		sof.Flush();

		SLG_LOG("DirectLightSamplingCache persistent cache saved: " << (sof.GetPosition() / 1024) << " Kbytes");
	}
	// Now sof is closed and I can call safeSave.Process()
	
	if (params.persistent.safeSave)
		safeSave.Process();
}

BOOST_CLASS_EXPORT_IMPLEMENT(slg::DLSCacheEntry)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::DLSCBvh)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::DLSCParams)
