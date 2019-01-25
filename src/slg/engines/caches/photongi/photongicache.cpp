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

#include <boost/format.hpp>

#if defined(_OPENMP)
#include <omp.h>
#endif

#include "slg/samplers/sobol.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/utils/pathdepthinfo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

// TODO: serialization

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

PhotonGICache::PhotonGICache(const Scene *scn,
		const PhotonGISamplerType smplType,
		const bool visEnabled,
		const float visTargetHitRate,
		const u_int visMaxSampleCount,
		const u_int tracedCount, const u_int pathDepth, const u_int maxLookUp,
		const float radius, const float normalAngle,
		const bool direct, const u_int maxDirect,
		const bool indirect, const u_int maxIndirect,
		const bool caustic, const u_int maxCaustic,
		const PhotonGIDebugType debug) : scene(scn),
		samplerType(smplType), 
		visibilityEnabled(visEnabled),
		visibilityTargetHitRate(visTargetHitRate),
		visibilityMaxSampleCount(visMaxSampleCount),
		maxPhotonTracedCount(tracedCount), maxPathDepth(pathDepth), entryMaxLookUpCount(maxLookUp),
		entryRadius(radius), entryRadius2(radius * radius), entryNormalAngle(normalAngle),
		directEnabled(direct), indirectEnabled(indirect), causticEnabled(caustic),
		maxDirectSize(maxDirect), maxIndirectSize(maxIndirect), maxCausticSize(maxCaustic),
		debugType(debug),
		samplerSharedData(nullptr),
		directPhotonTracedCount(0),
		indirectPhotonTracedCount(0),
		causticPhotonTracedCount(0),
		visibilityParticlesOctree(nullptr),
		directPhotonsBVH(nullptr),
		indirectPhotonsBVH(nullptr),
		causticPhotonsBVH(nullptr),
		radiancePhotonsBVH(nullptr) {
}

PhotonGICache::~PhotonGICache() {
	delete samplerSharedData;
	
	delete visibilityParticlesOctree;

	delete directPhotonsBVH;
	delete indirectPhotonsBVH;
	delete causticPhotonsBVH;
	delete radiancePhotonsBVH;
}

void PhotonGICache::GenerateEyeRay(const Camera *camera, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const {
	const u_int *subRegion = camera->filmSubRegion;
	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0] + 1);
	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2] + 1);

	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));
}

void PhotonGICache::TraceVisibilityParticles() {
	SLG_LOG("Photon GI tracing visibility particles");

	// Initialize the Octree where to store the visibility points
	visibilityParticlesOctree = new PGCIOctree(visibilityParticles, scene->dataSet->GetBBox(),
			2.f * entryRadius, entryNormalAngle);

	// Initialize the sampler
	RandomGenerator rnd(131);
	SobolSamplerSharedData sharedData(&rnd, NULL);
	SobolSampler sampler(&rnd, NULL, NULL, 0.f, &sharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 3;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		maxPathDepth * sampleStepSize; // For each path vertex
	sampler.RequestSamples(sampleSize);
	
	// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = maxPathDepth;
	maxPathDepthInfo.diffuseDepth = maxPathDepth;
	maxPathDepthInfo.glossyDepth = maxPathDepth;
	maxPathDepthInfo.specularDepth = maxPathDepth;

	double lastPrintTime = WallClockTime();
	u_int cacheLookUp = 0;
	u_int cacheHits = 0;
	double cacheHitRate = 0.0;
	bool cacheHitRateIsGood = false;
	for (u_int i = 0; i < visibilityMaxSampleCount; ++i) {
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

			if (bsdf.IsPhotonGIEnabled()) {
				// Check if a cache entry is available for this point
				const u_int entryIndex = visibilityParticlesOctree->GetNearestEntry(bsdf.hitPoint.p, surfaceNormal);

				if (entryIndex == NULL_INDEX) {
					// Add as a new entry
					visibilityParticles.push_back(VisibilityParticle(bsdf.hitPoint.p, surfaceNormal));
					visibilityParticlesOctree->Add(visibilityParticles.size() - 1);
				} else
					++cacheHits;

				++cacheLookUp;
			}

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			// Check if I reached the max. depth
			sampleResult.lastPathVertex = depthInfo.IsLastPathVertex(maxPathDepthInfo, bsdf.GetEventTypes());
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
			if ((cacheLookUp > 64 * 64) && (cacheHitRate > 100.0 * visibilityTargetHitRate)) {
				SLG_LOG("Photon GI visibility cache hit is greater than: " << boost::str(boost::format("%.4f") % (100.0 * visibilityTargetHitRate)) << "%");
				cacheHitRateIsGood = true;
				break;
			}

			const double now = WallClockTime();
			if (now - lastPrintTime > 2.0) {
				SLG_LOG("Photon GI visibility entries: " << i << "/" << visibilityMaxSampleCount <<" (" << (u_int)((100.0 * i) / visibilityMaxSampleCount) << "%)");
				SLG_LOG("Photon GI visibility hits: " << cacheHits << "/" << cacheLookUp <<" (" << boost::str(boost::format("%.4f") % cacheHitRate) << "%)");
				lastPrintTime = now;
			}
		}

#ifdef WIN32
		// Work around Windows bad scheduling
		boost::this_thread::yield();
#endif
	}

	if (!cacheHitRateIsGood)
		SLG_LOG("WARNING: PhotonGI visibility hit rate is not good enough: " << boost::str(boost::format("%.4f") % cacheHitRate) << "%");

	visibilityParticles.shrink_to_fit();
	SLG_LOG("PhotonGI visibility total entries: " << visibilityParticles.size());
}

void PhotonGICache::TracePhotons(vector<Photon> &directPhotons, vector<Photon> &indirectPhotons,
		vector<Photon> &causticPhotons) {
	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	vector<TracePhotonsThread *> renderThreads(renderThreadCount, nullptr);
	SLG_LOG("Photon GI trace photons thread count: " << renderThreads.size());
	
	globalPhotonsCounter = 0;
	globalDirectPhotonsTraced = 0;
	globalIndirectPhotonsTraced = 0;
	globalCausticPhotonsTraced = 0;
	globalDirectSize = 0;
	globalIndirectSize = 0;
	globalCausticSize = 0;

	// Create the photon tracing threads
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
		radiancePhotons.insert(radiancePhotons.end(), renderThreads[i]->radiancePhotons.begin(),
				renderThreads[i]->radiancePhotons.end());

		delete renderThreads[i];
		renderThreads[i] = nullptr;
	}

	directPhotonTracedCount = globalDirectPhotonsTraced;
	indirectPhotonTracedCount = globalIndirectPhotonsTraced;
	causticPhotonTracedCount = globalCausticPhotonsTraced;
	
	directPhotons.shrink_to_fit();
	indirectPhotons.shrink_to_fit();
	causticPhotons.shrink_to_fit();
	radiancePhotons.shrink_to_fit();

	// globalPhotonsCounter isn't exactly the number: there is an error due
	// last bucket of work likely being smaller than work bucket size
	SLG_LOG("Photon GI total photon traced: " << globalPhotonsCounter);
	SLG_LOG("Photon GI total direct photon stored: " << directPhotons.size() <<
			" (" << directPhotonTracedCount << " traced)");
	SLG_LOG("Photon GI total indirect photon stored: " << indirectPhotons.size() <<
			" (" << indirectPhotonTracedCount << " traced)");
	SLG_LOG("Photon GI total caustic photon stored: " << causticPhotons.size() <<
			" (" << causticPhotonTracedCount << " traced)");
	SLG_LOG("Photon GI total radiance photon stored: " << radiancePhotons.size());
}

void PhotonGICache::AddOutgoingRadiance(RadiancePhoton &radiacePhoton, const PGICPhotonBvh *photonsBVH,
			const u_int photonTracedCount) const {
	if (photonsBVH) {
		vector<NearPhoton> entries;
		entries.reserve(entryMaxLookUpCount);

		float maxDistance2;
		photonsBVH->GetAllNearEntries(entries, radiacePhoton.p, radiacePhoton.n, maxDistance2);

		if (entries.size() > 0) {
			Spectrum result;
			for (auto const &nearPhoton : entries) {
				const Photon *photon = (const Photon *)nearPhoton.photon;

				// Using a box filter here (i.e. multiply by 1.0)
				result += photon->alpha;
			}

			result /= photonTracedCount * maxDistance2 * M_PI;

			radiacePhoton.outgoingRadiance += result;
		}
	}
}

void PhotonGICache::FillRadiancePhotonData(RadiancePhoton &radiacePhoton) {
	// This value was saved at RadiancePhoton creation time
	const Spectrum bsdfEvaluateTotal = radiacePhoton.outgoingRadiance;

	radiacePhoton.outgoingRadiance = Spectrum();
	AddOutgoingRadiance(radiacePhoton, directPhotonsBVH, directPhotonTracedCount);
	AddOutgoingRadiance(radiacePhoton, indirectPhotonsBVH, indirectPhotonTracedCount);
	AddOutgoingRadiance(radiacePhoton, causticPhotonsBVH, causticPhotonTracedCount);

	radiacePhoton.outgoingRadiance *= bsdfEvaluateTotal * INV_PI;
}

void PhotonGICache::FillRadiancePhotonsData() {
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

void PhotonGICache::Preprocess() {
	//--------------------------------------------------------------------------
	// Trace visibility particles
	//--------------------------------------------------------------------------

	// Visibility information are used only by Metropolis sampler
	if ((samplerType == PGIC_SAMPLER_METROPOLIS) && visibilityEnabled)
		TraceVisibilityParticles();

	//--------------------------------------------------------------------------
	// Fill all photon vectors
	//--------------------------------------------------------------------------

	TracePhotons(directPhotons, indirectPhotons, causticPhotons);

	//--------------------------------------------------------------------------
	// Direct light photon map
	//--------------------------------------------------------------------------

	if ((directPhotons.size() > 0) && (directEnabled || indirectEnabled)) {
		SLG_LOG("Photon GI building direct photons BVH");
		directPhotonsBVH = new PGICPhotonBvh(directPhotons, entryMaxLookUpCount,
				entryRadius, entryNormalAngle);
	}

	//--------------------------------------------------------------------------
	// Indirect light photon map
	//--------------------------------------------------------------------------

	if ((indirectPhotons.size() > 0) && indirectEnabled) {
		SLG_LOG("Photon GI building indirect photons BVH");
		indirectPhotonsBVH = new PGICPhotonBvh(indirectPhotons, entryMaxLookUpCount,
				entryRadius, entryNormalAngle);
	}

	//--------------------------------------------------------------------------
	// Caustic photon map
	//--------------------------------------------------------------------------

	if ((causticPhotons.size() > 0) && (causticEnabled || indirectEnabled)) {
		SLG_LOG("Photon GI building caustic photons BVH");
		causticPhotonsBVH = new PGICPhotonBvh(causticPhotons, entryMaxLookUpCount,
				entryRadius, entryNormalAngle);
	}

	//--------------------------------------------------------------------------
	// Radiance photon map
	//--------------------------------------------------------------------------

	if ((radiancePhotons.size() > 0) && indirectEnabled) {	
		SLG_LOG("Photon GI building radiance photon data");
		FillRadiancePhotonsData();

		SLG_LOG("Photon GI building radiance photons BVH");
		radiancePhotonsBVH = new PGICRadiancePhotonBvh(radiancePhotons, entryMaxLookUpCount,
				entryRadius, entryNormalAngle);
	}
	
	//--------------------------------------------------------------------------
	// Check what I can free because it is not going to be used during
	// the rendering
	//--------------------------------------------------------------------------
	
	// I can always free indirect photon map because I'm going to use the
	// radiance map if the indirect cache is enabled
	delete indirectPhotonsBVH;
	indirectPhotonsBVH = nullptr;
	indirectPhotons.clear();
	indirectPhotons.shrink_to_fit();

	if (!directEnabled) {
		delete directPhotonsBVH;
		directPhotonsBVH = nullptr;
		directPhotons.clear();
		directPhotons.shrink_to_fit();
	}

	if (!indirectEnabled) {
		delete indirectPhotonsBVH;
		indirectPhotonsBVH = nullptr;
		indirectPhotons.clear();
		indirectPhotons.shrink_to_fit();
	}

	if (!causticEnabled) {
		delete causticPhotonsBVH;
		causticPhotonsBVH = nullptr;
		causticPhotons.clear();
		causticPhotons.shrink_to_fit();
	}
	
	//--------------------------------------------------------------------------
	// Print some statistics about memory usage
	//--------------------------------------------------------------------------

	size_t totalMemUsage = 0;
	if (directPhotonsBVH) {
		SLG_LOG("Photon GI direct cache photons memory usage: " << ToMemString(directPhotons.size() * sizeof(Photon)));
		SLG_LOG("Photon GI direct cache BVH memory usage: " << ToMemString(directPhotonsBVH->GetMemoryUsage()));

		totalMemUsage += directPhotons.size() * sizeof(Photon) + directPhotonsBVH->GetMemoryUsage();
	}

	if (indirectPhotonsBVH) {
		SLG_LOG("Photon GI indirect cache photons memory usage: " << ToMemString(indirectPhotons.size() * sizeof(Photon)));
		SLG_LOG("Photon GI indirect cache BVH memory usage: " << ToMemString(indirectPhotonsBVH->GetMemoryUsage()));

		totalMemUsage += indirectPhotons.size() * sizeof(Photon) + indirectPhotonsBVH->GetMemoryUsage();
	}

	if (causticPhotonsBVH) {
		SLG_LOG("Photon GI caustic cache photons memory usage: " << ToMemString(causticPhotons.size() * sizeof(Photon)));
		SLG_LOG("Photon GI caustic cache BVH memory usage: " << ToMemString(causticPhotonsBVH->GetMemoryUsage()));

		totalMemUsage += causticPhotons.size() * sizeof(Photon) + causticPhotonsBVH->GetMemoryUsage();
	}

	if (radiancePhotonsBVH) {
		SLG_LOG("Photon GI radiance cache photons memory usage: " << ToMemString(radiancePhotons.size() * sizeof(RadiancePhoton)));
		SLG_LOG("Photon GI radiance cache BVH memory usage: " << ToMemString(radiancePhotonsBVH->GetMemoryUsage()));

		totalMemUsage += radiancePhotons.size() * sizeof(Photon) + radiancePhotonsBVH->GetMemoryUsage();
	}

	SLG_LOG("Photon GI total memory usage: " << ToMemString(totalMemUsage));
}

Spectrum PhotonGICache::GetAllRadiance(const BSDF &bsdf) const {
	assert (bsdf.IsPhotonGIEnabled());

	Spectrum result;
	if (radiancePhotonsBVH) {
		vector<NearPhoton> entries;
		entries.reserve(entryMaxLookUpCount);

		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		float maxDistance2;
		radiancePhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p, n, maxDistance2);

		if (entries.size() > 0) {
			for (auto const &nearPhoton : entries) {
				const RadiancePhoton *radiancePhoton = (const RadiancePhoton *)nearPhoton.photon;

				// Using a box filter here
				result += radiancePhoton->outgoingRadiance;
			}

			result /= entries.size();
		}
	}

	return result;
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
		const u_int photonTracedCount, const float maxDistance2, const BSDF &bsdf) const {
	Spectrum result;

	if (entries.size() > 0) {
		if (bsdf.GetMaterialType() == MaterialType::MATTE) {
			// A fast path for matte material

			for (auto const &nearPhoton : entries) {
				const Photon *photon = (const Photon *)nearPhoton.photon;

				// Using a Simpson filter here
				result += SimpsonKernel(bsdf.hitPoint.p, photon->p, maxDistance2) * photon->alpha;
			}
			
			result *= bsdf.EvaluateTotal() * INV_PI;
		} else {
			// Generic path

			BSDFEvent event;
			for (auto const &nearPhoton : entries) {
				const Photon *photon = (const Photon *)nearPhoton.photon;

				// Using a Simpson filter here
				result += SimpsonKernel(bsdf.hitPoint.p, photon->p, maxDistance2) *
						bsdf.Evaluate(-photon->d, &event, nullptr, nullptr) * photon->alpha;
			}
		}
	}
	
	result /= photonTracedCount * maxDistance2;

	return result;
}

Spectrum PhotonGICache::GetDirectRadiance(const BSDF &bsdf) const {
	assert (bsdf.IsPhotonGIEnabled());

	if (directPhotonsBVH) {
		vector<NearPhoton> entries;
		entries.reserve(entryMaxLookUpCount);

		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		float maxDistance2;
		directPhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p, n, maxDistance2);

		return ProcessCacheEntries(entries, directPhotonTracedCount, maxDistance2, bsdf);
	} else
		return Spectrum();
}

Spectrum PhotonGICache::GetIndirectRadiance(const BSDF &bsdf) const {
	assert (bsdf.IsPhotonGIEnabled());

	Spectrum result;
	if (radiancePhotonsBVH) {
		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		const RadiancePhoton *radiancePhoton = radiancePhotonsBVH->GetNearestEntry(bsdf.hitPoint.p, n);

		if (radiancePhoton)
			result = radiancePhoton->outgoingRadiance;
	}
	
	return result;
}

Spectrum PhotonGICache::GetCausticRadiance(const BSDF &bsdf) const {
	assert (bsdf.IsPhotonGIEnabled());

	if (causticPhotonsBVH) {
		vector<NearPhoton> entries;
		entries.reserve(entryMaxLookUpCount);

		// Flip the normal if required
		const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.shadeN;
		float maxDistance2;
		causticPhotonsBVH->GetAllNearEntries(entries, bsdf.hitPoint.p, n, maxDistance2);

		return ProcessCacheEntries(entries, causticPhotonTracedCount, maxDistance2, bsdf);
	} else
		return Spectrum();
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
	if (type == "showdirect")
		return PhotonGIDebugType::PGIC_DEBUG_SHOWDIRECT;
	else if (type == "showindirect")
		return PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECT;
	else if (type == "showcaustic")
		return PhotonGIDebugType::PGIC_DEBUG_SHOWCAUSTIC;
	else if (type == "none")
		return PhotonGIDebugType::PGIC_DEBUG_NONE;
	else
		throw runtime_error("Unknown PhotonGI cache debug type: " + type);
}

string PhotonGICache::DebugType2String(const PhotonGIDebugType type) {
	switch (type) {
		case PhotonGIDebugType::PGIC_DEBUG_SHOWDIRECT:
			return "showdirect";
		case PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECT:
			return "showindirect";
		case PhotonGIDebugType::PGIC_DEBUG_SHOWCAUSTIC:
			return "showcaustic";
		case PhotonGIDebugType::PGIC_DEBUG_NONE:
			return "none";
		default:
			throw runtime_error("Unsupported wrap type in PhotonGICache::DebugType2String(): " + ToString(type));
	}
}

Properties PhotonGICache::ToProperties(const Properties &cfg) {
	Properties props;

	props <<
			cfg.Get(GetDefaultProps().Get("path.photongi.sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.visibility.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.visibility.targethitrate")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.visibility.maxsamplecount")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.direct.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxcount")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.direct.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.indirect.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.caustic.maxsize")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.lookup.maxcount")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.lookup.radius")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.lookup.normalangle")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.debug.type"));

	return props;
}

const Properties &PhotonGICache::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.photongi.sampler.type")("METROPOLIS") <<
			Property("path.photongi.visibility.enabled")(true) <<
			Property("path.photongi.visibility.targethitrate")(.99f) <<
			Property("path.photongi.visibility.maxsamplecount")(1024 * 1024) <<
			Property("path.photongi.direct.enabled")(false) <<
			Property("path.photongi.indirect.enabled")(false) <<
			Property("path.photongi.caustic.enabled")(false) <<
			Property("path.photongi.photon.maxcount")(100000) <<
			Property("path.photongi.photon.maxdepth")(4) <<
			Property("path.photongi.direct.maxsize")(100000) <<
			Property("path.photongi.indirect.maxsize")(100000) <<
			Property("path.photongi.caustic.maxsize")(100000) <<
			Property("path.photongi.lookup.maxcount")(48) <<
			Property("path.photongi.lookup.radius")(.15f) <<
			Property("path.photongi.lookup.normalangle")(10.f) <<
			Property("path.photongi.debug.type")("none");

	return props;
}

PhotonGICache *PhotonGICache::FromProperties(const Scene *scn, const Properties &cfg) {
	const bool directEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.direct.enabled")).Get<bool>();
	const bool indirectEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.indirect.enabled")).Get<bool>();
	const bool causticEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.caustic.enabled")).Get<bool>();
	
	if (directEnabled || indirectEnabled || causticEnabled) {
		const PhotonGISamplerType samplerType = String2SamplerType(cfg.Get(GetDefaultProps().Get("path.photongi.sampler.type")).Get<string>());

		bool visibilityEnabled = false;
		float visibilityTargetHitRate = 0.f;
		float visibilityMaxSampleCount = 0;
		if (samplerType == PGIC_SAMPLER_METROPOLIS) {
			visibilityEnabled = cfg.Get(GetDefaultProps().Get("path.photongi.visibility.enabled")).Get<bool>();
			visibilityTargetHitRate = cfg.Get(GetDefaultProps().Get("path.photongi.visibility.targethitrate")).Get<float>();
			visibilityMaxSampleCount = cfg.Get(GetDefaultProps().Get("path.photongi.visibility.maxsamplecount")).Get<u_int>();
		}

		const u_int maxPhotonTracedCount = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxcount")).Get<u_int>());
		const u_int maxDepth = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.photon.maxdepth")).Get<u_int>());

		const u_int maxLookUpCount = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.lookup.maxcount")).Get<u_int>());
		const float radius = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.lookup.radius")).Get<float>());
		const float normalAngle = Max(DEFAULT_EPSILON_MIN, cfg.Get(GetDefaultProps().Get("path.photongi.lookup.normalangle")).Get<float>());

		const u_int maxDirectSize = (directEnabled || indirectEnabled) ? Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.direct.maxsize")).Get<u_int>()) : 0;
		const u_int maxIndirectSize = indirectEnabled ? Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.indirect.maxsize")).Get<u_int>()) : 0;
		const u_int maxCausticSize = (causticEnabled || indirectEnabled) ? Max(0u, cfg.Get(GetDefaultProps().Get("path.photongi.caustic.maxsize")).Get<u_int>()) : 0;

		const PhotonGIDebugType debugType = String2DebugType(cfg.Get(GetDefaultProps().Get("path.photongi.debug.type")).Get<string>());
		
		return new PhotonGICache(scn,
				samplerType,
				visibilityEnabled, visibilityTargetHitRate, visibilityMaxSampleCount,
				maxPhotonTracedCount, maxDepth,
				maxLookUpCount, radius, normalAngle,
				directEnabled, maxDirectSize,
				indirectEnabled, maxIndirectSize,
				causticEnabled, maxCausticSize,
				debugType);
	} else
		return nullptr;
}
