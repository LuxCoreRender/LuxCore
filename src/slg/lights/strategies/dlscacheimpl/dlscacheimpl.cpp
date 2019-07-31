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

#include <algorithm>

#if defined(_OPENMP)
#include <omp.h>
#endif

#include <boost/format.hpp>
#include <boost/circular_buffer.hpp>

#include "luxrays/core/geometry/bbox.h"
#include "slg/samplers/sobol.h"
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
#include "slg/lights/strategies/dlscacheimpl/dlscoctree.h"
#include "slg/lights/strategies/dlscacheimpl/dlscbvh.h"
#include "slg/utils/film2sceneradius.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// DLSCacheEntry
//------------------------------------------------------------------------------

DLSCacheEntry::DLSCacheEntry() :
		lightsDistribution(nullptr), tmpInfo(nullptr) {
}

DLSCacheEntry::DLSCacheEntry(DLSCacheEntry &&other) {
	p = other.p;
	n = other.n;
	isVolume = other.isVolume;

	distributionIndexToLightIndex = other.distributionIndexToLightIndex;

	// We "steal" the resource from "other"
	lightsDistribution = other.lightsDistribution;
	tmpInfo = other.tmpInfo;

	// "other" will soon be destroyed and its destructor will do nothing
	// because we null out its resource here
	other.lightsDistribution = nullptr;
	other.tmpInfo = nullptr;
}

DLSCacheEntry::~DLSCacheEntry() {
	DeleteTmpInfo();

	delete lightsDistribution;		
}

DLSCacheEntry &DLSCacheEntry::operator=(DLSCacheEntry &&other) {
	p = other.p;
	n = other.n;
	isVolume = other.isVolume;

	distributionIndexToLightIndex = other.distributionIndexToLightIndex;

	// "other" is soon going to be destroyed, so we let it destroy our
	// current resource instead and we take "other"'s current resource via
	// swapping
	std::swap(lightsDistribution, other.lightsDistribution);
	std::swap(tmpInfo, other.tmpInfo);

	return *this;
}

void DLSCacheEntry::AddSamplingPoint(const BSDF &bsdf, const PathVolumeInfo &vi) {
	tmpInfo->bsdfList.push_back(bsdf);
	tmpInfo->volInfoList.push_back(vi);
	

	tmpInfo->isTransparent = (tmpInfo->isTransparent || (bsdf.GetEventTypes() & TRANSMIT));
}

void DLSCacheEntry::Init(const BSDF &bsdf, const PathVolumeInfo &vi) {
	p = bsdf.hitPoint.p;
	n = bsdf.hitPoint.GetLandingShadeN();
	isVolume = bsdf.IsVolume();

	delete lightsDistribution;
	lightsDistribution = nullptr;

	delete tmpInfo;
	tmpInfo = new TemporayInformation();

	// Add the initial point as a valid sampling point
	AddSamplingPoint(bsdf, vi);
}

//------------------------------------------------------------------------------
// DirectLightSamplingCache
//------------------------------------------------------------------------------

DirectLightSamplingCache::DirectLightSamplingCache() {
	octree = nullptr;
	bvh = nullptr;
	
	params.maxSampleCount = 10000000;
	params.maxDepth = 4;
	params.targetCacheHitRate = .995f;
	params.lightThreshold = .01f;

	params.entry.radius = .15f;
	params.entry.normalAngle = 10.f;
	params.entry.convergenceThreshold = .01f;
	params.entry.warmUpSamples = 12;
	params.entry.mergePasses = 3;
	params.entry.maxPasses = 1024;

	params.entry.enabledOnVolumes = false;
}

DirectLightSamplingCache::~DirectLightSamplingCache() {
	delete octree;
	delete bvh;
}

bool DirectLightSamplingCache::IsDLSCEnabled(const BSDF &bsdf) const {
	return !bsdf.IsDelta() && (params.entry.enabledOnVolumes || !bsdf.IsVolume());
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
		return dlsc.IsDLSCEnabled(bsdf);
	}

private:
	const DirectLightSamplingCache &dlsc;
};

}

float DirectLightSamplingCache::EvaluateBestRadius(const Scene *scene) {
	SLG_LOG("Direct light sampling cache evaluating best radius");

	// The percentage of image plane to cover with the radius
	const float imagePlaneRadius = .05f;

	// The old default radius: 15cm
	const float defaultRadius = .15f;
	
	DLSCFilm2SceneRadiusValidator validator(*this);

	return Film2SceneRadius(scene,  imagePlaneRadius, defaultRadius, params.maxDepth,
		scene->camera->shutterOpen, scene->camera->shutterClose,
		&validator);
}

//------------------------------------------------------------------------------
// Build the list of cache entries
//------------------------------------------------------------------------------

void DirectLightSamplingCache::GenerateEyeRay(const Camera *camera, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const {
	const u_int *subRegion = camera->filmSubRegion;
	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0] + 1);
	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2] + 1);

	const float timeSample = sampler->GetSample(4);
	const float time = camera->GenerateRayTime(timeSample);

	camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3));
}

void DirectLightSamplingCache::BuildCacheEntries(const Scene *scene) {
	SLG_LOG("Building direct light sampling cache: building cache entries");

	// Initialize the Octree where to store the cache points
	delete octree;
	octree = new DLSCOctree(allEntries, scene->dataSet->GetBBox(),
			params.entry.radius, params.entry.normalAngle);
			
	// Initialize the sampler
	RandomGenerator rnd(131);
	SobolSamplerSharedData sharedData(&rnd, NULL);
	SobolSampler sampler(&rnd, NULL, NULL, 0.f, &sharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 3;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		params.maxDepth * sampleStepSize; // For each path vertex
	sampler.RequestSamples(sampleSize);
	
	// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepth;
	maxPathDepth.depth = params.maxDepth;
	maxPathDepth.diffuseDepth = params.maxDepth;
	maxPathDepth.glossyDepth = params.maxDepth;
	maxPathDepth.specularDepth = params.maxDepth;

	double lastPrintTime = WallClockTime();
	u_int cacheLookUp = 0;
	u_int cacheHits = 0;
	double cacheHitRate = 0.0;
	bool cacheHitRateIsGood = false;
	for (u_int i = 0; i < params.maxSampleCount; ++i) {
		sampleResult.radiance[0] = Spectrum();
		
		Ray eyeRay;
		PathVolumeInfo volInfo;
		GenerateEyeRay(scene->camera, eyeRay, volInfo, &sampler, sampleResult);
		
		BSDFEvent lastBSDFEvent = SPECULAR;
		Spectrum pathThroughput(1.f);
		PathDepthInfo depthInfo;
		BSDF bsdf;
		for (;;) {
			sampleResult.firstPathVertex = (depthInfo.depth == 0);
			const u_int sampleOffset = sampleBootSize + depthInfo.depth * sampleStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			const bool hit = scene->Intersect(NULL, false, sampleResult.firstPathVertex,
					&volInfo, sampler.GetSample(sampleOffset),
					&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput,
					&pathThroughput, &sampleResult);
			pathThroughput *= connectionThroughput;
			// Note: pass-through check is done inside Scene::Intersect()

			if (!hit) {
				// Nothing was hit, time to stop
				break;
			}

			//------------------------------------------------------------------
			// Something was hit
			//------------------------------------------------------------------

			// Check if I have to flip the normal
			const Normal surfaceNormal = bsdf.hitPoint.intoObject ? bsdf.hitPoint.shadeN : -bsdf.hitPoint.shadeN;

			if (IsDLSCEnabled(bsdf)) {
				// Check if a cache entry is available for this point
				u_int entryIndex = octree->GetEntry(bsdf.hitPoint.p, surfaceNormal, bsdf.IsVolume());
				if (entryIndex != NULL_INDEX) {
					DLSCacheEntry &entry = allEntries[entryIndex];

					// It isn't. Add a new sampling points.
					entry.AddSamplingPoint(bsdf, volInfo);

					++cacheHits;
				} else {
					// Add as last entry
					allEntries.resize(allEntries.size() + 1);
					entryIndex = allEntries.size() - 1;

					DLSCacheEntry &entry = allEntries[entryIndex];
					entry.Init(bsdf, volInfo);

					octree->Add(entryIndex);
				}
				++cacheLookUp;
			}

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			// Check if I reached the max. depth
			sampleResult.lastPathVertex = depthInfo.IsLastPathVertex(maxPathDepth, bsdf.GetEventTypes());
			if (sampleResult.lastPathVertex && !sampleResult.firstPathVertex)
				break;

			Vector sampledDir;
			float cosSampledDir, lastPdfW;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler.GetSample(sampleOffset + 1),
						sampler.GetSample(sampleOffset + 2),
						&lastPdfW, &cosSampledDir, &lastBSDFEvent);
			sampleResult.passThroughPath = false;

			assert (!bsdfSample.IsNaN() && !bsdfSample.IsInf());
			if (bsdfSample.Black())
				break;
			assert (!isnan(lastPdfW) && !isinf(lastPdfW));

			if (sampleResult.firstPathVertex)
				sampleResult.firstPathVertexEvent = lastBSDFEvent;

			// Increment path depth informations
			depthInfo.IncDepths(lastBSDFEvent);

			pathThroughput *= bsdfSample;
			assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

			// Update volume information
			volInfo.Update(lastBSDFEvent, bsdf);

			eyeRay.Update(bsdf.hitPoint.p, surfaceNormal, sampledDir);
		}
		
		sampler.NextSample(sampleResults);

		//----------------------------------------------------------------------
		// Check if I have a cache hit rate high enough to stop
		//----------------------------------------------------------------------

		if (i == 64 * 64) {
			// End of the warm up period. Reset the cache hit counters
			cacheHits = 0;
			cacheLookUp = 0;
		} else if (i > 2 * 64 * 64) {
			// After the warm up period, I can check the cache hit ratio to know
			// if it is time to stop

			cacheHitRate = (100.0 * cacheHits) / cacheLookUp;
			if ((cacheLookUp > 64 * 64) && (cacheHitRate > 100.0 * params.targetCacheHitRate)) {
				SLG_LOG("Direct light sampling cache hit is greater than: " << boost::str(boost::format("%.4f") % (100.0 * params.targetCacheHitRate)) << "%");
				cacheHitRateIsGood = true;
				break;
			}

			const double now = WallClockTime();
			if (now - lastPrintTime > 2.0) {
				SLG_LOG("Direct light sampling cache entries: " << i << "/" << params.maxSampleCount <<" (" << (u_int)((100.0 * i) / params.maxSampleCount) << "%)");
				SLG_LOG("Direct light sampling cache hits: " << cacheHits << "/" << cacheLookUp <<" (" << boost::str(boost::format("%.4f") % cacheHitRate) << "%)");
				lastPrintTime = now;
			}
		}

#ifdef WIN32
		// Work around Windows bad scheduling
		boost::this_thread::yield();
#endif
	}

	if (!cacheHitRateIsGood)
		SLG_LOG("WARNING: direct light sampling cache hit rate is not good enough: " << boost::str(boost::format("%.4f") % cacheHitRate) << "%");

	allEntries.shrink_to_fit();
	SLG_LOG("Direct light sampling cache total entries: " << allEntries.size());
}

//------------------------------------------------------------------------------
// Fill cache entry
//------------------------------------------------------------------------------

float DirectLightSamplingCache::SampleLight(const Scene *scene, DLSCacheEntry *entry,
		const LightSource *light, const u_int pass) const {
	const float u1 = RadicalInverse(pass, 3);
	const float u2 = RadicalInverse(pass, 5);
	const float u3 = RadicalInverse(pass, 7);
	const float u4 = RadicalInverse(pass, 11);

	// Select a sampling point
	const size_t bsdfListSize = entry->tmpInfo->bsdfList.size();
	const u_int bsdfListIndexIndex = Min<u_int>(Floor2UInt(u4 * bsdfListSize), bsdfListSize - 1);
	const BSDF &samplingBSDF = entry->tmpInfo->bsdfList[bsdfListIndexIndex];

	Vector lightRayDir;
	float distance, directPdfW;
	Spectrum lightRadiance = light->Illuminate(*scene, samplingBSDF,
			u1, u2, u3, &lightRayDir, &distance, &directPdfW);
	assert (!lightRadiance.IsNaN() && !lightRadiance.IsInf());

	if (!lightRadiance.Black() && (entry->isVolume ||
			entry->tmpInfo->isTransparent ||
			Dot(lightRayDir, samplingBSDF.hitPoint.GetLandingShadeN()) > 0.f)) {
		assert (!isnan(directPdfW) && !isinf(directPdfW));

		const float time = RadicalInverse(pass, 13);
		const float u5 = RadicalInverse(pass, 17);

		Ray shadowRay(samplingBSDF.hitPoint.p, lightRayDir,
				0.f,
				distance,
				time);
		shadowRay.UpdateMinMaxWithEpsilon();
		RayHit shadowRayHit;
		BSDF shadowBsdf;
		Spectrum connectionThroughput;

		// Check if the light source is visible
		if (!scene->Intersect(NULL, false, false, &entry->tmpInfo->volInfoList[bsdfListIndexIndex], u5, &shadowRay,
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

void DirectLightSamplingCache::FillCacheEntry(const Scene *scene, DLSCacheEntry *entry) {
	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();

	vector<float> entryReceivedLuminance(lights.size(), 0.f);
	float maxLuminanceValue = 0.f;

	for (u_int lightIndex = 0; lightIndex < lights.size(); ++lightIndex) {
		const LightSource *light = lights[lightIndex];
	
		// Check if the light source uses direct light sampling
		if (!light->IsDirectLightSamplingEnabled()) {
			// This light source is excluded from direct light sampling
			continue;
		}

		// Check if I can avoid to trace all shadow rays
		bool isAlwaysInShadow = true;
		for (const BSDF &bsdf : entry->tmpInfo->bsdfList) {
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
			receivedLuminance += SampleLight(scene, entry, light, pass);
			
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
		maxLuminanceValue = Max(maxLuminanceValue, entryReceivedLuminance[lightIndex]);

		// For some Debugging
		//SLG_LOG("Light #" << lightIndex << ": " << entryReceivedLuminance[lightIndex] <<	" (pass " << pass << ")");
	}
	
	if (maxLuminanceValue > 0.f) {
		// Use the higher light luminance to establish a threshold. Using an 1%
		// threshold at the moment.
		const float luminanceThreshold = maxLuminanceValue * params.lightThreshold;

		for (u_int lightIndex = 0; lightIndex < lights.size(); ++lightIndex) {
			if (entryReceivedLuminance[lightIndex] > luminanceThreshold) {
				// Add this light
				entry->tmpInfo->lightReceivedLuminance.push_back(entryReceivedLuminance[lightIndex]);
				entry->tmpInfo->distributionIndexToLightIndex.push_back(lightIndex);
			}
		}

		entry->tmpInfo->lightReceivedLuminance.shrink_to_fit();
		entry->tmpInfo->distributionIndexToLightIndex.shrink_to_fit();
	}

	/*SLG_LOG("===================================================================");
	for (u_int i = 0; i < entry->tmpInfo->lightReceivedLuminance.size(); ++i) {
		SLG_LOG("Light #" << entry->tmpInfo->distributionIndexToLightIndex[i] << ": " <<
				entry->tmpInfo->lightReceivedLuminance[i]);
	}
	SLG_LOG("===================================================================");*/
}

void DirectLightSamplingCache::FillCacheEntries(const Scene *scene) {
	// Print the number of light with enabled direct light sampling
	const vector<LightSource *> &lights = scene->lightDefs.GetLightSources();
	u_int dlsLightCount = 0;
	for (u_int lightIndex = 0; lightIndex < lights.size(); ++lightIndex) {
		const LightSource *light = lights[lightIndex];
	
		// Check if the light source uses direct light sampling
		if (light->IsDirectLightSamplingEnabled())
			++dlsLightCount;
	}
	SLG_LOG("Building direct light sampling cache: filling cache entries with " << dlsLightCount << " light sources");

	double lastPrintTime = WallClockTime();
	atomic<u_int> counter(0);
	
	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < allEntries.size(); ++i) {
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
				SLG_LOG("Direct light sampling cache filled entries: " << counter << "/" << allEntries.size() <<" (" << (u_int)((100.0 * counter) / allEntries.size()) << "%)");
				lastPrintTime = now;
			}
		}
		
		FillCacheEntry(scene, &allEntries[i]);
		
		++counter;
	}
}

//------------------------------------------------------------------------------
// Merge entry information
//------------------------------------------------------------------------------

void DirectLightSamplingCache::MergeCacheEntry(const Scene *scene, const u_int entryIndex) {
	const DLSCacheEntry &entry = allEntries[entryIndex];

	// Copy the temporary information
	entry.tmpInfo->mergedLightReceivedLuminance = entry.tmpInfo->lightReceivedLuminance;
	entry.tmpInfo->mergedDistributionIndexToLightIndex = entry.tmpInfo->distributionIndexToLightIndex;

	// Look for all near cache entries
	vector<u_int> nearEntriesIndex;
	octree->GetAllNearEntries(nearEntriesIndex, entry.p, entry.n, entry.isVolume,
			params.entry.radius * 2.f);

	// Merge all found entries
	for (auto const &nearEntryindex : nearEntriesIndex) {
		// Avoid to merge with myself
		if (nearEntryindex == entryIndex)
			continue;

		const DLSCacheEntry &nearEntry = allEntries[nearEntryindex];
		for (u_int i = 0; i < nearEntry.tmpInfo->distributionIndexToLightIndex.size(); ++i) {
			const u_int lightIndex = nearEntry.tmpInfo->distributionIndexToLightIndex[i];
			
			// Check if I have already this light
			bool found = false;
			for (u_int j = 0; j < entry.tmpInfo->mergedDistributionIndexToLightIndex.size(); ++j) {
				if (lightIndex == entry.tmpInfo->mergedDistributionIndexToLightIndex[j]) {
					// It is a light source I already have
					
					entry.tmpInfo->mergedLightReceivedLuminance[j] = Max(entry.tmpInfo->mergedLightReceivedLuminance[j],
							nearEntry.tmpInfo->lightReceivedLuminance[i]);
					found = true;
					break;
				}
			}
			
			if (!found) {
				// It is a new light
				entry.tmpInfo->mergedLightReceivedLuminance.push_back(nearEntry.tmpInfo->lightReceivedLuminance[i]);
				entry.tmpInfo->mergedDistributionIndexToLightIndex.push_back(lightIndex);
			}
		}
	}
}

void DirectLightSamplingCache::FinalizedMergeCacheEntry(const u_int entryIndex) {
	const DLSCacheEntry &entry = allEntries[entryIndex];

	// Copy back the temporary information
	entry.tmpInfo->lightReceivedLuminance = entry.tmpInfo->mergedLightReceivedLuminance;
	entry.tmpInfo->distributionIndexToLightIndex = entry.tmpInfo->mergedDistributionIndexToLightIndex;
	
	entry.tmpInfo->mergedLightReceivedLuminance.resize(0);
	entry.tmpInfo->mergedDistributionIndexToLightIndex.resize(0);
}

void DirectLightSamplingCache::MergeCacheEntries(const Scene *scene) {
	SLG_LOG("Building direct light sampling cache: merging cache entries");

	// Merge near cache entry information (even multiple time to further
	// propagate the information)
	for (u_int pass = 0; pass < params.entry.mergePasses; ++pass) {
		SLG_LOG("Direct light sampling cache merged entries pass " << pass);

		double lastPrintTime = WallClockTime();
		atomic<u_int> counter(0);

		#pragma omp parallel for
		for (
				// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
				unsigned
#endif
				int i = 0; i < allEntries.size(); ++i) {
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
					SLG_LOG("Direct light sampling cache merged entries: " << counter << "/" << allEntries.size() <<" (" << (u_int)((100.0 * counter) / allEntries.size()) << "%)");
					lastPrintTime = now;
				}
			}

			MergeCacheEntry(scene, i);

			++counter;
		}


		#pragma omp parallel for
		for (
				// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
				unsigned
#endif
				int i = 0; i < allEntries.size(); ++i) {
			FinalizedMergeCacheEntry(i);
		}
	}
}

//------------------------------------------------------------------------------
// Initialize entry distributions
//------------------------------------------------------------------------------

void DirectLightSamplingCache::InitDistributionEntry(const Scene *scene, DLSCacheEntry *entry) {
	// Initialize the distribution
	if (entry->tmpInfo->lightReceivedLuminance.size() > 0) {
#ifndef NDEBUG
		for (u_int i = 0; i < entry->tmpInfo->lightReceivedLuminance.size(); ++i)
			assert (!isnan(entry->tmpInfo->lightReceivedLuminance[i]) && !isinf(entry->tmpInfo->lightReceivedLuminance[i]));
#endif

		// Use log of the received luminance like in LOG_POWER strategy to avoid
		// problems when there is a huge difference in light contribution like
		// with sun and sky
		vector<float> logReceivedLuminance(entry->tmpInfo->lightReceivedLuminance.size());
		for (u_int i = 0; i < logReceivedLuminance.size(); ++i)
			logReceivedLuminance[i] = logf(1.f + entry->tmpInfo->lightReceivedLuminance[i]);

		entry->lightsDistribution = new Distribution1D(&(logReceivedLuminance[0]),
				logReceivedLuminance.size());

		entry->distributionIndexToLightIndex = entry->tmpInfo->distributionIndexToLightIndex;
		entry->distributionIndexToLightIndex.shrink_to_fit();
	}
}

void DirectLightSamplingCache::InitDistributionEntries(const Scene *scene) {
	SLG_LOG("Building direct light sampling cache: initializing distributions");

	double lastPrintTime = WallClockTime();
	atomic<u_int> counter(0);

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < allEntries.size(); ++i) {
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
				SLG_LOG("Direct light sampling cache initializing distribution: " << counter << "/" << allEntries.size() <<" (" << (u_int)((100.0 * counter) / allEntries.size()) << "%)");
				lastPrintTime = now;
			}
		}
		
		InitDistributionEntry(scene, &allEntries[i]);
		
		++counter;
	}
}

//------------------------------------------------------------------------------
// Build BVH
//------------------------------------------------------------------------------

void DirectLightSamplingCache::BuildBVH(const Scene *scene) {
	SLG_LOG("Building direct light sampling cache: build BVH");

	bvh = new DLSCBvh(&allEntries, params.entry.radius, params.entry.normalAngle);
}

//------------------------------------------------------------------------------
// Build
//------------------------------------------------------------------------------

void DirectLightSamplingCache::Build(const Scene *scene) {
	// This check is required because FILESAVER engine doesn't
	// initialize any accelerator
	if (!scene->dataSet->GetAccelerator()) {
		SLG_LOG("Direct light sampling cache is not built");
		return;
	}
	
	SLG_LOG("Building direct light sampling cache");

	allEntries.clear();

	if (params.entry.radius == 0.f) {
		params.entry.radius = EvaluateBestRadius(scene);
		SLG_LOG("Direct light sampling cache best radius: " << params.entry.radius);
	}

	BuildCacheEntries(scene);
	FillCacheEntries(scene);
	MergeCacheEntries(scene);
	InitDistributionEntries(scene);

	// Delete all temporary information
	for (auto &entry : allEntries)
		entry.DeleteTmpInfo();

	// Delete the Octree and build the BVH
	delete octree;
	octree = NULL;

	BuildBVH(scene);

	// Export the entries for debugging
	//DebugExport("entries-point.scn", entryRadius * .05f);
}

const DLSCacheEntry *DirectLightSamplingCache::GetEntry(const luxrays::Point &p,
		const luxrays::Normal &n, const bool isVolume) const {
	if (!bvh || (isVolume && !params.entry.enabledOnVolumes))
		return nullptr;

	return bvh->GetEntry(p, n, isVolume);
}

void DirectLightSamplingCache::DebugExport(const string &fileName, const float sphereRadius) const {
	Properties prop;

	prop <<
			Property("scene.materials.dlsc_material.type")("matte") <<
			Property("scene.materials.dlsc_material.kd")("0.75 0.75 0.75") <<
			Property("scene.materials.dlsc_material_red.type")("matte") <<
			Property("scene.materials.dlsc_material_red.kd")("0.75 0.0 0.0") <<
			Property("scene.materials.dlsc_material_red.emission")("0.25 0.0 0.0");

	for (u_int i = 0; i < allEntries.size(); ++i) {
		const DLSCacheEntry &entry = allEntries[i];
		if (entry.IsDirectLightSamplingDisabled())
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
