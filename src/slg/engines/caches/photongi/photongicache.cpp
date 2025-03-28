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

#include <math.h>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "luxrays/utils/thread.h"
#include "luxrays/utils/strutils.h"

#include "slg/samplers/sobol.h"
#include "slg/utils/pathdepthinfo.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/engines/caches/photongi/tracephotonsthread.h"
#include "slg/utils/pathinfo.h"

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
		causticPhotonTracedCount(0),
		causticPhotonPass(0) {
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

bool PhotonGICache::IsDirectLightHitVisible(const EyePathInfo &pathInfo,
		const bool photonGICausticCacheUsed) const {
	// This is a specific check to cut fireflies created by some glossy or
	// specular bounce
	if (!(pathInfo.lastBSDFEvent & DIFFUSE) && (pathInfo.depth.diffuseDepth > 0))
		return false;
	else if (!params.caustic.enabled || !photonGICausticCacheUsed)
		return true;
	else if (!pathInfo.IsCausticPath() && (params.debugType == PGIC_DEBUG_NONE))
		return true;
	else
		return false;
}

void PhotonGICache::TracePhotons(const u_int seedBase, const u_int photonTracedCount,
		const bool indirectCacheDone, const bool causticCacheDone,
		boost::atomic<u_int> &globalIndirectPhotonsTraced, boost::atomic<u_int> &globalCausticPhotonsTraced,
		boost::atomic<u_int> &globalIndirectSize, boost::atomic<u_int> &globalCausticSize) {
	const size_t renderThreadCount = GetHardwareThreadCount();
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

			vp.alphaAccumulated.Add(p.lightID, p.alpha);
		}
		indirectPhotonStored += renderThreads[i]->indirectPhotons.size();

		causticPhotons.insert(causticPhotons.end(), renderThreads[i]->causticPhotons.begin(),
				renderThreads[i]->causticPhotons.end());
		causticPhotonStored += renderThreads[i]->causticPhotons.size();

		delete renderThreads[i];
	}

	// Update the count only if I have traced this kind of photons
	if (!indirectCacheDone)
		indirectPhotonTracedCount = globalIndirectPhotonsTraced;
	// Update the count only if I have traced this kind of photons
	if (!causticCacheDone)
		causticPhotonTracedCount = globalCausticPhotonsTraced;

	SLG_LOG("PhotonGI additional indirect photon stored: " << indirectPhotonStored);
	SLG_LOG("PhotonGI additional caustic photon stored: " << causticPhotonStored);
	// photonReacedCount isn't exactly but it is quite near
	SLG_LOG("PhotonGI total photon traced: " << Max(indirectPhotonTracedCount, causticPhotonTracedCount));
}

void PhotonGICache::TracePhotons(const bool indirectEnabled, const bool causticEnabled) {
	const size_t renderThreadCount = GetHardwareThreadCount();

	boost::atomic<u_int> globalIndirectPhotonsTraced(0);
	boost::atomic<u_int> globalCausticPhotonsTraced(0);
	boost::atomic<u_int> globalIndirectSize(0);
	boost::atomic<u_int> globalCausticSize(0);

	// Update the count only if I have traced this kind of photons
	if (indirectEnabled)
		indirectPhotonTracedCount = 0;
	// Update the count only if I have traced this kind of photons
	if (causticEnabled)
		causticPhotonTracedCount = 0;

	if (indirectEnabled && (params.indirect.maxSize == 0)) {
		// Automatic indirect cache convergence test is required

		const u_int photonTracedStep = 2000000;
		u_int photonTracedCount = 0;
		vector<SpectrumGroup> lastAlpha(visibilityParticles.size());
		vector<SpectrumGroup> currentAlpha(visibilityParticles.size());
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
					vector<SpectrumGroup> filteredCurrentAlpha(visibilityParticles.size());
					FilterVisibilityParticlesRadiance(currentAlpha, filteredCurrentAlpha);

					currentAlpha = filteredCurrentAlpha;
				}

				// Compute the scale for an auto-linear-like tone mapping of values

				float Y = 0.f;
				for (u_int i = 0; i < visibilityParticles.size(); ++i)
					Y += currentAlpha[i].Sum().Y();
				Y /= visibilityParticles.size();

				const float alphaScale = (Y > 0.f) ? (1.25f / Y * powf(118.f / 255.f, 2.2f)) : 1.f;
				for (u_int i = 0; i < visibilityParticles.size(); ++i)
					currentAlpha[i] *= alphaScale;

				// Look for the max. error

				float maxError = 0.f;
				for (u_int i = 0; i < visibilityParticles.size(); ++i) {
					if (!currentAlpha[i].Black()) {
						SpectrumGroup alpha = currentAlpha[i];
						alpha -= lastAlpha[i];

						for (u_int j = 0; j < alpha.Size(); ++j) {
							const float currentError = alpha[j].Abs().Max();

							maxError = Max(maxError, currentError);
						}
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

void PhotonGICache::FilterVisibilityParticlesRadiance(const vector<SpectrumGroup> &radianceValues,
			vector<SpectrumGroup> &filteredRadianceValues) const {
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
			SpectrumGroup &filtered = filteredRadianceValues[index];
			for (auto nearIndex : nearParticleIndices)
				filtered += radianceValues[nearIndex];

			filtered /= nearParticleIndices.size();
		} 
	}
}

void PhotonGICache::CreateRadiancePhotons() {
	//--------------------------------------------------------------------------
	// Compute the outgoing radiance for each visibility entry
	//--------------------------------------------------------------------------

	vector<SpectrumGroup> outgoingRadianceValues(visibilityParticles.size());

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

		vector<SpectrumGroup> filteredOutgoingRadianceValues(visibilityParticles.size());
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

void PhotonGICache::Preprocess(const u_int threadCnt) {
	threadCount = threadCnt;
	threadsSyncBarrier.reset(new boost::barrier(threadCount));
	lastUpdateSpp = 0;
	updateSeedBase = 1;
	finishUpdateFlag = false;

	if (params.persistent.fileName != "") {
		// Check if the file already exist
		if (boost::filesystem::exists(params.persistent.fileName)) {
			// Load the cache from the file
			LoadPersistentCache(params.persistent.fileName);

			return;
		}
		
		// The file doesn't exist so I have to go through normal pre-processing
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
		// I must use indirect cache parameters for Visibility particles if the
		// cache is enabled
		params.visibility.lookUpRadius = params.indirect.lookUpRadius;
		params.visibility.lookUpNormalAngle = params.indirect.lookUpNormalAngle;
	} else {
		if (params.visibility.lookUpRadius == 0.f) {
			if (params.caustic.enabled) {
				// Caustic radius is too small for visibility check
				params.visibility.lookUpRadius = EvaluateBestRadius();
				params.visibility.lookUpNormalAngle = params.caustic.lookUpNormalAngle;
			} else
				throw runtime_error("Indirect and/or caustic cache must be enabled in PhotonGI");
		}
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
		SLG_LOG("PhotonGI building caustic photons BVH");
		causticPhotonsBVH = new PGICPhotonBvh(&causticPhotons, causticPhotonTracedCount,
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

const SpectrumGroup *PhotonGICache::GetIndirectRadiance(const BSDF &bsdf) const {
	assert (IsPhotonGIEnabled(bsdf));

	if (radiancePhotonsBVH) {
		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.geometryN;
		const RadiancePhoton *radiancePhoton = radiancePhotonsBVH->GetNearestEntry(bsdf.hitPoint.p, n, bsdf.IsVolume());

		if (radiancePhoton) {
			const SpectrumGroup *result = &radiancePhoton->outgoingRadiance;

			assert (result->IsValid());
			assert (DistanceSquared(radiancePhoton->p, bsdf.hitPoint.p) < radiancePhotonsBVH->GetEntryRadius());
			assert (bsdf.IsVolume() == radiancePhoton->isVolume);
			assert (radiancePhoton->isVolume || (Dot(radiancePhoton->n, n) > radiancePhotonsBVH->GetEntryNormalCosAngle()));

			return result;
		}
	}
	
	return nullptr;
}

SpectrumGroup PhotonGICache::ConnectWithCausticPaths(const BSDF &bsdf) const {
	assert (IsPhotonGIEnabled(bsdf));

	SpectrumGroup result;
	if (causticPhotonsBVH) {
		result = causticPhotonsBVH->ConnectAllNearEntries(bsdf);

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
		throw runtime_error("Unknown PhotonGI cache sampler type: " + type);
}

string PhotonGICache::SamplerType2String(const PhotonGISamplerType type) {
	switch (type) {
		case PhotonGISamplerType::PGIC_SAMPLER_RANDOM:
			return "RANDOM";
		case PhotonGISamplerType::PGIC_SAMPLER_METROPOLIS:
			return "METROPOLIS";
		default:
			throw runtime_error("Unsupported sampler type in PhotonGICache::SamplerType2String(): " + ToString(type));
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
