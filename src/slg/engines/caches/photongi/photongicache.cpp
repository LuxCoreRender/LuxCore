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

#include <math.h>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "slg/samplers/sobol.h"
#include "slg/utils/pathdepthinfo.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/engines/caches/photongi/tracephotonsthread.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

PhotonGICache::PhotonGICache() :
		scene(nullptr),
		visibilityParticlesKdTree(nullptr),
		radiancePhotonsBVH(nullptr) ,
		causticPhotonsBVH(nullptr) {
}

PhotonGICache::PhotonGICache(const Scene *scn, const PhotonGICacheParams &p) :
		scene(scn), params(p),
		visibilityParticlesKdTree(nullptr),
		radiancePhotonsBVH(nullptr) ,
		indirectPhotonTracedCount(0),
		causticPhotonsBVH(nullptr),
		causticPhotonTracedCount(0) {
}

PhotonGICache::~PhotonGICache() {
	delete visibilityParticlesKdTree;

	delete causticPhotonsBVH;
	delete radiancePhotonsBVH;
}

bool PhotonGICache::IsPhotonGIEnabled(const BSDF &bsdf) const {
	const BSDFEvent eventTypes = bsdf.GetEventTypes();
	
	if ((eventTypes & TRANSMIT) || (eventTypes & SPECULAR) ||
			((eventTypes & GLOSSY) && (bsdf.GetGlossiness() < params.glossinessUsageThreshold)))
		return false;
	else
		return bsdf.IsPhotonGIEnabled();
}

float PhotonGICache::GetIndirectUsageThreshold(const BSDFEvent lastBSDFEvent,
		const float lastGlossiness, const float u0) const {
	// Decide if the glossy surface is "nearly specular"

	if ((lastBSDFEvent & GLOSSY) && (lastGlossiness < params.glossinessUsageThreshold)) {
		// Disable the cache, the surface is "nearly specular"
		return numeric_limits<float>::infinity();
	} else {
		// Use a larger blend zone for glossy surface
		const float scale = (lastBSDFEvent & GLOSSY) ? 2.f : 1.f;

		// Enable the cache for diffuse or glossy "nearly diffuse" but only after
		// the threshold (before I brute force and cache between 0x and 1x the threshold)
		return scale * u0 * params.indirect.usageThresholdScale * params.indirect.lookUpRadius;
	}
}

bool PhotonGICache::IsDirectLightHitVisible(const bool causticCacheAlreadyUsed,
		const BSDFEvent lastBSDFEvent, const PathDepthInfo &depthInfo) const {
	// This is a specific check to cut fireflies created by some glossy or
	// specular bounce
	if (!(lastBSDFEvent & DIFFUSE) && (depthInfo.diffuseDepth > 0))
		return false;
	else if (!params.caustic.enabled)
		return true;
	else if ((!causticCacheAlreadyUsed || !(lastBSDFEvent & SPECULAR)) &&
			(params.debugType == PGIC_DEBUG_NONE))
		return true;
	else
		return false;
}

void PhotonGICache::TracePhotons(const u_int seedBase, const u_int photonTracedCount,
		const bool indirectCacheDone, const bool causticCacheDone,
		boost::atomic<u_int> &globalIndirectPhotonsTraced, boost::atomic<u_int> &globalCausticPhotonsTraced,
		boost::atomic<u_int> &globalIndirectSize, boost::atomic<u_int> &globalCausticSize) {
	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	vector<TracePhotonsThread *> renderThreads(renderThreadCount, nullptr);

	boost::atomic<u_int> globalPhotonsCounter(0);

	// Create the photon tracing threads
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i] = new TracePhotonsThread(*this, i,
				seedBase, photonTracedCount,
				indirectCacheDone, causticCacheDone,
				globalPhotonsCounter, globalIndirectPhotonsTraced,
				globalCausticPhotonsTraced, globalIndirectSize,
				globalCausticSize);
	}

	// Start photon tracing threads
	for (size_t i = 0; i < renderThreadCount; ++i)
		renderThreads[i]->Start();
	
	// Wait for the end of photon tracing threads
	u_int indirectPhotonStored = 0;
	u_int causticPhotonStored = 0;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i]->Join();

		// Copy all photons
		for (auto const &p : renderThreads[i]->indirectPhotons) {
			PGICVisibilityParticle &vp = visibilityParticles[p.visibilityParticelIndex];

			vp.alphaAccumulated += p.alpha;
		}
		indirectPhotonStored += renderThreads[i]->indirectPhotons.size();

		causticPhotons.insert(causticPhotons.end(), renderThreads[i]->causticPhotons.begin(),
				renderThreads[i]->causticPhotons.end());
		causticPhotonStored += renderThreads[i]->causticPhotons.size();

		delete renderThreads[i];
	}

	indirectPhotonTracedCount = globalIndirectPhotonsTraced;
	causticPhotonTracedCount = globalCausticPhotonsTraced;

	SLG_LOG("PhotonGI additional indirect photon stored: " << indirectPhotonStored);
	SLG_LOG("PhotonGI additional caustic photon stored: " << causticPhotonStored);
	// photonReacedCount isn't exactly but it is quite near
	SLG_LOG("PhotonGI total photon traced: " << Max(indirectPhotonTracedCount, causticPhotonTracedCount));
}

void PhotonGICache::TracePhotons(const bool indirectEnabled, const bool causticEnabled) {
	const size_t renderThreadCount = boost::thread::hardware_concurrency();

	boost::atomic<u_int> globalIndirectPhotonsTraced(0);
	boost::atomic<u_int> globalCausticPhotonsTraced(0);
	boost::atomic<u_int> globalIndirectSize(0);
	boost::atomic<u_int> globalCausticSize(0);

	indirectPhotonTracedCount = 0;
	causticPhotonTracedCount = 0;

	if (indirectEnabled && (params.indirect.maxSize == 0)) {
		// Automatic indirect cache convergence test is required

		const u_int photonTracedStep = 2000000;
		u_int photonTracedCount = 0;
		vector<Spectrum> lastAlpha(visibilityParticles.size());
		vector<Spectrum> currentAlpha(visibilityParticles.size());
		while (photonTracedCount < params.photon.maxTracedCount) {
			//------------------------------------------------------------------
			// Trace additional photons
			//------------------------------------------------------------------

			TracePhotons(updateSeedBase, photonTracedStep, false, !causticEnabled,
				globalIndirectPhotonsTraced, globalCausticPhotonsTraced,
				globalIndirectSize, globalCausticSize);
			photonTracedCount += photonTracedStep;

			//------------------------------------------------------------------
			// Check the convergence if it is not the first step
			//------------------------------------------------------------------

			if (photonTracedCount > photonTracedStep) {
				// Compute current alpha

				for (u_int i = 0; i < visibilityParticles.size(); ++i) {
					const PGICVisibilityParticle vp = visibilityParticles[i];

					currentAlpha[i] = vp.ComputeRadiance(params.indirect.lookUpRadius2, indirectPhotonTracedCount);
				}

				// Filter outgoing radiance

				if (params.indirect.filterRadiusScale > 0.f) {
					vector<Spectrum> filteredCurrentAlpha(visibilityParticles.size());
					FilterVisibilityParticlesRadiance(currentAlpha, filteredCurrentAlpha);

					currentAlpha = filteredCurrentAlpha;
				}

				// Compute the scale for an auto-linear-like tone mapping of values

				float Y = 0.f;
				for (u_int i = 0; i < visibilityParticles.size(); ++i)
					Y += currentAlpha[i].Y();
				Y /= visibilityParticles.size();

				const float alphaScale = (Y > 0.f) ? (1.25f / Y * powf(118.f / 255.f, 2.2f)) : 1.f;
				for (u_int i = 0; i < visibilityParticles.size(); ++i)
					currentAlpha[i] *= alphaScale;

				// Look for the max. error

				float maxError = 0.f;
				for (u_int i = 0; i < visibilityParticles.size(); ++i) {
					if (!currentAlpha[i].Black()) {
						const float currentError =  (currentAlpha[i] - lastAlpha[i]).Abs().Max();

						maxError = Max(maxError, currentError);
					}

					// Update last alpha cache entries
					lastAlpha[i] = currentAlpha[i];
				}

				SLG_LOG(boost::format("PhotonGI estimated current indirect photon error: %.2f%%") % (100.f * maxError));

				// If the error is under the threshold, stop tracing photons for indirect cache
				if (maxError < params.indirect.haltThreshold) {
					// Finish the work for caustic cache too
					if (causticEnabled &&
							(causticPhotons.size() < params.caustic.maxSize) &&
							(photonTracedCount < params.photon.maxTracedCount)) {
						updateSeedBase += renderThreadCount;

						TracePhotons(updateSeedBase,
								params.photon.maxTracedCount - photonTracedCount, true, false,
								globalIndirectPhotonsTraced, globalCausticPhotonsTraced,
								globalIndirectSize, globalCausticSize);
					}

					break;
				}
			} else {
				// Update last alpha cache entries

				for (u_int i = 0; i < visibilityParticles.size(); ++i) {
					const PGICVisibilityParticle vp = visibilityParticles[i];

					lastAlpha[i] = vp.ComputeRadiance(params.indirect.lookUpRadius2, indirectPhotonTracedCount);
				}
			}
			
			updateSeedBase += renderThreadCount;
		}
	} else {
		// Just trace the asked amount of photon paths
		TracePhotons(updateSeedBase, params.photon.maxTracedCount, !indirectEnabled, !causticEnabled,
				globalIndirectPhotonsTraced, globalCausticPhotonsTraced,
				globalIndirectSize, globalCausticSize);

	}

	updateSeedBase += renderThreadCount;

	causticPhotons.shrink_to_fit();
}

void PhotonGICache::FilterVisibilityParticlesRadiance(const vector<Spectrum> &radianceValues,
			vector<Spectrum> &filteredRadianceValues) const {
	const float lookUpRadius2 = Sqr(params.indirect.filterRadiusScale * params.indirect.lookUpRadius);
	const float lookUpCosNormalAngle = cosf(Radians(params.indirect.lookUpNormalAngle));

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int index = 0; index < visibilityParticles.size(); ++index) {
		// Look for all near particles

		vector<u_int> nearParticleIndices;
		const PGICVisibilityParticle &vp = visibilityParticles[index];
		// I can use visibilityParticlesKdTree to get radiance photons indices
		// because there is a one on one correspondence 
		visibilityParticlesKdTree->GetAllNearEntries(nearParticleIndices,
				vp.p, vp.n, vp.isVolume,
				lookUpRadius2, lookUpCosNormalAngle);

		if (nearParticleIndices.size() > 0) {
			Spectrum &v = filteredRadianceValues[index];
			for (auto nearIndex : nearParticleIndices)
				v += radianceValues[nearIndex];

			v /= nearParticleIndices.size();
		} 
	}
}

void PhotonGICache::CreateRadiancePhotons() {
	//--------------------------------------------------------------------------
	// Compute the outgoing radiance for each visibility entry
	//--------------------------------------------------------------------------

	vector<Spectrum> outgoingRadianceValues(visibilityParticles.size());

	for (u_int index = 0 ; index < visibilityParticles.size(); ++index) {
		const PGICVisibilityParticle &vp = visibilityParticles[index];

		outgoingRadianceValues[index] = vp.ComputeRadiance(params.indirect.lookUpRadius2, indirectPhotonTracedCount);
		assert (outgoingRadianceValues[index].IsValid());
	}

	//--------------------------------------------------------------------------
	// Filter outgoing radiance
	//--------------------------------------------------------------------------

	if (params.indirect.filterRadiusScale > 0.f) {
		SLG_LOG("PhotonGI filtering radiance photons");

		vector<Spectrum> filteredOutgoingRadianceValues(visibilityParticles.size());
		FilterVisibilityParticlesRadiance(outgoingRadianceValues, filteredOutgoingRadianceValues);

		outgoingRadianceValues = filteredOutgoingRadianceValues;
	}

	//--------------------------------------------------------------------------
	// Create a radiance map entry for each visibility entry
	//--------------------------------------------------------------------------

	for (u_int index = 0 ; index < visibilityParticles.size(); ++index) {
		if (!outgoingRadianceValues[index].Black()) {
			const PGICVisibilityParticle &vp = visibilityParticles[index];

			radiancePhotons.push_back(RadiancePhoton(vp.p,
					vp.n, outgoingRadianceValues[index], vp.isVolume));
		}
	}
	radiancePhotons.shrink_to_fit();
	
	SLG_LOG("PhotonGI total radiance photon stored: " << radiancePhotons.size());
}

void PhotonGICache::MergeCausticPhotons() {
	// Build a BVH in order to make fast look ups. Note the numeric_limits<u_int>::max()
	// to get all of them.
	PGICPhotonBvh *photonsBVH = new PGICPhotonBvh(&causticPhotons, numeric_limits<u_int>::max(),
			params.caustic.lookUpRadius * params.caustic.mergeRadiusScale,
			params.caustic.lookUpNormalAngle);
	
	// Merge all "similar" photons

	vector<bool> usedPhotons(causticPhotons.size(), false);
	vector<NearPhoton> entries;
	float maxDistance2;
	vector<Photon> mergedCausticPhotons;
	const float normalCosAngle = cosf(Radians(params.caustic.lookUpNormalAngle));
	double lastPrintTime = WallClockTime();

	for (u_int i = 0; i < causticPhotons.size(); ++i) {
		// Check if I have already used this photon
		if (usedPhotons[i])
			continue;

		const Photon &photon = causticPhotons[i];

		// Look for near photons
		entries.clear();
		photonsBVH->GetAllNearEntries(entries, photon.p, photon.landingSurfaceNormal,
				photon.isVolume, maxDistance2);

		// I must find the same photon at least
		assert (entries.size() >= 1);

		Photon newPhoton(photon.p, photon.d, Spectrum(), photon.landingSurfaceNormal, photon.isVolume);
		for (u_int j = 0; j < entries.size(); ++j) {
			const u_int entryIndex = entries[j].photonIndex;

			// Check if I have already used this photon
			if (usedPhotons[entryIndex])
				continue;

			// Check if the direction vector is near "enough"
			if (Dot(causticPhotons[entryIndex].d, photon.d) <= normalCosAngle)
				continue;

			// Accumulate all photon alpha
			newPhoton.alpha += causticPhotons[entryIndex].alpha;
			// Accumulate all photon directions
			newPhoton.d += causticPhotons[entryIndex].d;

			// Mark the photon as used
			usedPhotons[entries[j].photonIndex] = true;
		}
		newPhoton.d = Normalize(newPhoton.d);

		// Add the new merged photon
		mergedCausticPhotons.push_back(newPhoton);
		
		const double now = WallClockTime();
		if (now - lastPrintTime > 2.0) {
			SLG_LOG(boost::format("PhotonGI caustic photons merged: %d/%d [%.1f%%]") %
					i % causticPhotons.size() %
					((100.0 * i) / causticPhotons.size()));
			lastPrintTime = now;
		}
	}
	
	delete photonsBVH;

	SLG_LOG(boost::format("PhotonGI merged caustics photons: %d => %d [%.1f%%]") %
			causticPhotons.size() % mergedCausticPhotons.size() %
			(100.0 * mergedCausticPhotons.size() / causticPhotons.size()));

	causticPhotons = mergedCausticPhotons;
	mergedCausticPhotons.shrink_to_fit();
}

void PhotonGICache::Preprocess(const u_int threadCnt) {
	threadCount = threadCnt;
	threadsSyncBarrier.reset(new boost::barrier(threadCount));
	lastUpdateSpp = 0;
	updateSeedBase = 1;

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

	if (params.indirect.enabled && (params.indirect.lookUpRadius == 0.f)) {
		params.indirect.lookUpRadius = EvaluateBestRadius();
		SLG_LOG("PhotonGI best indirect cache radius: " << params.indirect.lookUpRadius);
	}

	//--------------------------------------------------------------------------
	// Initialize all parameters
	//--------------------------------------------------------------------------

	if (!params.indirect.enabled)
		params.indirect.maxSize = 0;

	if (!params.caustic.enabled)
		params.caustic.maxSize = 0;

	if (params.indirect.enabled) {
		// I always use indirect cache parameters for Visibility particles if the
		// cache is enabled
		params.visibility.lookUpRadius = params.indirect.lookUpRadius;
		params.visibility.lookUpNormalAngle = params.indirect.lookUpNormalAngle;
	} else {
		if (params.caustic.enabled) {
			// Caustic radius is too small for visibility check
			params.visibility.lookUpRadius = EvaluateBestRadius();
			params.visibility.lookUpNormalAngle = params.caustic.lookUpNormalAngle;
		} else
			throw runtime_error("Indirect and/or caustic cache must be enabled in PhotonGI");
	}
	SLG_LOG("PhotonGI visibility lookup radius: " << params.visibility.lookUpRadius);
	params.visibility.lookUpNormalCosAngle = cosf(Radians(params.visibility.lookUpNormalAngle));

	params.visibility.lookUpRadius2 = params.visibility.lookUpRadius * params.visibility.lookUpRadius;
	params.indirect.lookUpRadius2 = params.indirect.lookUpRadius * params.indirect.lookUpRadius;
	params.caustic.lookUpRadius2 = params.caustic.lookUpRadius * params.caustic.lookUpRadius;

	//--------------------------------------------------------------------------
	// Trace visibility particles
	//--------------------------------------------------------------------------

	TraceVisibilityParticles();
	if (visibilityParticles.size() == 0) {
		SLG_LOG("PhotonGI WARNING: nothing is visible and/or cache enabled.");
		return;
	}

	//--------------------------------------------------------------------------
	// Fill all photon vectors
	//--------------------------------------------------------------------------

	// I build indirect and caustic caches with 2 different steps in order to
	// have Metropolis work at beast for the 2 different tasks

	if (params.indirect.enabled) {
		SLG_LOG("PhotonGI tracing indirect cache photons");
		TracePhotons(true, false);
	}

	if (params.caustic.enabled) {
		SLG_LOG("PhotonGI tracing caustic cache photons");
		TracePhotons(false, true);
	}

	//--------------------------------------------------------------------------
	// Radiance photon map
	//--------------------------------------------------------------------------

	if (params.indirect.enabled) {	
		SLG_LOG("PhotonGI building radiance photon data");
		CreateRadiancePhotons();

		if (radiancePhotons.size() > 0) {
			SLG_LOG("PhotonGI building radiance photons BVH");
			radiancePhotonsBVH = new PGICRadiancePhotonBvh(&radiancePhotons,
					params.indirect.lookUpRadius, params.indirect.lookUpNormalAngle);
		}
	}

	//--------------------------------------------------------------------------
	// Caustic photon map
	//--------------------------------------------------------------------------

	if ((causticPhotons.size() > 0) && params.caustic.enabled) {
		if (params.caustic.mergeRadiusScale > 0.f) {
			SLG_LOG("PhotonGI merging caustic photons BVH");
			MergeCausticPhotons();
		}

		SLG_LOG("PhotonGI building caustic photons BVH");
		causticPhotonsBVH = new PGICPhotonBvh(&causticPhotons, params.caustic.lookUpMaxCount,
				params.caustic.lookUpRadius, params.caustic.lookUpNormalAngle);
	}

	//--------------------------------------------------------------------------
	// Free visibility map (only if it is not required for a further update
	//--------------------------------------------------------------------------

	if (!params.caustic.enabled || (params.caustic.updateSpp == 0)) {
		delete visibilityParticlesKdTree;
		visibilityParticlesKdTree = nullptr;
		visibilityParticles.clear();
		visibilityParticles.shrink_to_fit();
	}

	//--------------------------------------------------------------------------
	// Print some statistics about memory usage
	//--------------------------------------------------------------------------

	size_t totalMemUsage = 0;

	if (causticPhotonsBVH) {
		SLG_LOG("PhotonGI caustic cache photons memory usage: " << ToMemString(causticPhotons.size() * sizeof(Photon)));
		SLG_LOG("PhotonGI caustic cache BVH memory usage: " << ToMemString(causticPhotonsBVH->GetMemoryUsage()));

		totalMemUsage += causticPhotons.size() * sizeof(Photon) + causticPhotonsBVH->GetMemoryUsage();
	}

	if (radiancePhotonsBVH) {
		SLG_LOG("PhotonGI indirect cache photons memory usage: " << ToMemString(radiancePhotons.size() * sizeof(RadiancePhoton)));
		SLG_LOG("PhotonGI indirect cache BVH memory usage: " << ToMemString(radiancePhotonsBVH->GetMemoryUsage()));

		totalMemUsage += radiancePhotons.size() * sizeof(Photon) + radiancePhotonsBVH->GetMemoryUsage();
	}

	SLG_LOG("PhotonGI total memory usage: " << ToMemString(totalMemUsage));
	
	//--------------------------------------------------------------------------
	// Check if I have to save the persistent cache
	//--------------------------------------------------------------------------

	if (params.persistent.fileName != "")
		SavePersistentCache(params.persistent.fileName);
}

// Simpson filter from PBRT v2. Filter the photons according their
// distance, giving more weight to the nearest.
static inline float SimpsonKernel(const Point &p1, const Point &p2,
		const float maxDist2) {
	const float dist2 = DistanceSquared(p1, p2);

	// The distance between p1 and p2 is supposed to be < maxDist2
	assert (dist2 <= maxDist2);
    const float s = (1.f - dist2 / maxDist2);

    return 3.f * INV_PI * s * s;
}

Spectrum PhotonGICache::ProcessCacheEntries(const vector<NearPhoton> &entries,
		const float maxDistance2,
		const vector<Photon> &photons, const u_int photonTracedCount,
		const BSDF &bsdf) const {
	Spectrum result;

	if (entries.size() > 0) {
		if (bsdf.GetMaterialType() == MaterialType::MATTE) {
			// A fast path for matte material

			for (auto const &nearPhoton : entries) {
				const Photon &photon = photons[nearPhoton.photonIndex];

				// Using a Simpson filter here
				result += SimpsonKernel(bsdf.hitPoint.p, photon.p, maxDistance2) * 
						photon.alpha;
			}

			result *= bsdf.EvaluateTotal() * INV_PI;
		} else {
			// Generic path

			BSDFEvent event;
			for (auto const &nearPhoton : entries) {
				const Photon &photon = photons[nearPhoton.photonIndex];

				// bsdf.Evaluate() multiplies the result by AbsDot(bsdf.hitPoint.shadeN, -photon->d)
				// so I have to cancel that factor. It is already included in photon density
				// estimation.
				const Spectrum bsdfEval = bsdf.Evaluate(-photon.d, &event, nullptr, nullptr) / AbsDot(bsdf.hitPoint.shadeN, -photon.d);
				
				// Using a Simpson filter here
				result += SimpsonKernel(bsdf.hitPoint.p, photon.p, maxDistance2) *
						bsdfEval * photon.alpha;
			}
		}

		result /= photonTracedCount * maxDistance2;
	}

	return result;
}

Spectrum PhotonGICache::GetIndirectRadiance(const BSDF &bsdf) const {
	assert (IsPhotonGIEnabled(bsdf));

	Spectrum result;
	if (radiancePhotonsBVH) {
		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.geometryN;
		const RadiancePhoton *radiancePhoton = radiancePhotonsBVH->GetNearestEntry(bsdf.hitPoint.p, n, bsdf.IsVolume());

		if (radiancePhoton) {
			result = radiancePhoton->outgoingRadiance;

			assert (result.IsValid());
			assert (DistanceSquared(radiancePhoton->p, bsdf.hitPoint.p) < radiancePhotonsBVH->GetEntryRadius());
			assert (bsdf.IsVolume() == radiancePhoton->isVolume);
			assert (radiancePhoton->isVolume || (Dot(radiancePhoton->n, n) > radiancePhotonsBVH->GetEntryNormalCosAngle()));
		}
	}
	
	return result;
}

Spectrum PhotonGICache::GetCausticRadiance(const BSDF &bsdf) const {
	assert (IsPhotonGIEnabled(bsdf));

	Spectrum result;
	if (causticPhotonsBVH) {
		vector<NearPhoton> entries;
		entries.reserve(causticPhotonsBVH->GetEntryMaxLookUpCount());

		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.geometryN;
		float maxDistance2;
		causticPhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p, n, bsdf.IsVolume(),
				maxDistance2);

		result = ProcessCacheEntries(entries, maxDistance2, causticPhotons, causticPhotonTracedCount, bsdf);

		assert (result.IsValid());
	}
	
	return result;
}

PhotonGISamplerType PhotonGICache::String2SamplerType(const string &type) {
	if (type == "RANDOM")
		return PhotonGISamplerType::PGIC_SAMPLER_RANDOM;
	else if (type == "METROPOLIS")
		return PhotonGISamplerType::PGIC_SAMPLER_METROPOLIS;
	else
		throw runtime_error("Unknown PhotonGI cache debug type: " + type);
}

string PhotonGICache::SamplerType2String(const PhotonGISamplerType type) {
	switch (type) {
		case PhotonGISamplerType::PGIC_SAMPLER_RANDOM:
			return "RANDOM";
		case PhotonGISamplerType::PGIC_SAMPLER_METROPOLIS:
			return "METROPOLIS";
		default:
			throw runtime_error("Unsupported wrap type in PhotonGICache::SamplerType2String(): " + ToString(type));
	}
}

PhotonGIDebugType PhotonGICache::String2DebugType(const string &type) {
	if (type == "none")
		return PhotonGIDebugType::PGIC_DEBUG_NONE;
	else if (type == "showindirect")
		return PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECT;
	else if (type == "showcaustic")
		return PhotonGIDebugType::PGIC_DEBUG_SHOWCAUSTIC;
	else if (type == "showindirectpathmix")
		return PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECTPATHMIX;
	else
		throw runtime_error("Unknown PhotonGI cache debug type: " + type);
}

string PhotonGICache::DebugType2String(const PhotonGIDebugType type) {
	switch (type) {
		case PhotonGIDebugType::PGIC_DEBUG_NONE:
			return "none";
		case PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECT:
			return "showindirect";
		case PhotonGIDebugType::PGIC_DEBUG_SHOWCAUSTIC:
			return "showcaustic";
		case PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECTPATHMIX:
			return "showindirectpathmix";
		default:
			throw runtime_error("Unsupported wrap type in PhotonGICache::DebugType2String(): " + ToString(type));
	}
}
