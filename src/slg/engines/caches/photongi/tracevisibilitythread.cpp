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

#include "slg/scene/scene.h"
#include "slg/engines/renderengine.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/engines/caches/photongi/tracevisibilitythread.h"
#include "slg/utils/pathdepthinfo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TraceVisibilityThread
//------------------------------------------------------------------------------

TraceVisibilityThread::TraceVisibilityThread(PhotonGICache &cache, const u_int index,
		SobolSamplerSharedData &sobolSharedData,
		PGCIOctree *octree, boost::mutex &octreeMutex,
		boost::atomic<u_int> &gParticlesCount,
		u_int &cacheLookUp, u_int &cacheHits,
		bool &warmUp) :
		pgic(cache), threadIndex(index),
		visibilitySobolSharedData(sobolSharedData),
		particlesOctree(octree), particlesOctreeMutex(octreeMutex),
		globalVisibilityParticlesCount(gParticlesCount),
		visibilityCacheLookUp(cacheLookUp), visibilityCacheHits(cacheHits),
		visibilityWarmUp(warmUp),
		renderThread(nullptr) {
}

TraceVisibilityThread::~TraceVisibilityThread() {
	Join();
}

void TraceVisibilityThread::Start() {
	renderThread = new boost::thread(&TraceVisibilityThread::RenderFunc, this);
}

void TraceVisibilityThread::Join() {
	if (renderThread) {
		renderThread->join();

		delete renderThread;
		renderThread = nullptr;
	}
}

void TraceVisibilityThread::GenerateEyeRay(const Camera *camera, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const {
	const u_int *subRegion = camera->filmSubRegion;
	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0] + 1);
	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2] + 1);

	const float timeSample = sampler->GetSample(4);
	const float time = camera->GenerateRayTime(timeSample);

	camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3));
}

void TraceVisibilityThread::RenderFunc() {
	const u_int workSize = 4096;
	
	// Hard coded RR parameters
	const u_int rrDepth = 3;
	const float rrImportanceCap = .5f;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	const Scene *scene = pgic.scene;
	const Camera *camera = scene->camera;

	// Initialize the sampler
	RandomGenerator rnd(1 + threadIndex);
	SobolSampler sampler(&rnd, NULL, NULL, 0.f, &visibilitySobolSharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 4;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		pgic.params.photon.maxPathDepth * sampleStepSize; // For each path vertex
	sampler.RequestSamples(sampleSize);
	
	// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = pgic.params.photon.maxPathDepth;
	maxPathDepthInfo.diffuseDepth = pgic.params.photon.maxPathDepth;
	maxPathDepthInfo.glossyDepth = pgic.params.photon.maxPathDepth;
	maxPathDepthInfo.specularDepth = pgic.params.photon.maxPathDepth;

	vector<VisibilityParticle> visibilityParticles;

	//--------------------------------------------------------------------------
	// Get a bucket of work to do
	//--------------------------------------------------------------------------

	const double startTime = WallClockTime();
	double lastPrintTime = startTime;	
	double cacheHitRate = 0.0;
	while(!boost::this_thread::interruption_requested()) {
		// Get some work to do
		u_int workCounter;
		do {
			workCounter = globalVisibilityParticlesCount;
		} while (!globalVisibilityParticlesCount.compare_exchange_weak(workCounter, workCounter + workSize));

		// Check if it is time to stop
		if (workCounter >= pgic.params.visibility.maxSampleCount)
			break;
		
		u_int workToDo = (workCounter + workSize > pgic.params.visibility.maxSampleCount) ?
			(pgic.params.visibility.maxSampleCount - workCounter) : workSize;
		
		//----------------------------------------------------------------------
		// Build the list of visibility particles to add
		//----------------------------------------------------------------------

		visibilityParticles.clear();
		u_int workToDoIndex = workToDo;
		while (workToDoIndex-- && !boost::this_thread::interruption_requested()) {
			sampleResult.radiance[0] = Spectrum();

			Ray eyeRay;
			PathVolumeInfo volInfo;
			GenerateEyeRay(camera, eyeRay, volInfo, &sampler, sampleResult);

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

				//--------------------------------------------------------------
				// Something was hit
				//--------------------------------------------------------------

				// Check if I have to flip the normal
				const Normal surfaceNormal = ((Dot(bsdf.hitPoint.geometryN, -eyeRay.d) > 0.f) ?
					1.f : -1.f) * bsdf.hitPoint.geometryN;

				if (pgic.IsPhotonGIEnabled(bsdf)) {
					const Spectrum bsdfEvalTotal = bsdf.EvaluateTotal();
					assert (bsdfEvalTotal.IsValid());

					visibilityParticles.push_back(VisibilityParticle(bsdf.hitPoint.p,
							surfaceNormal, bsdfEvalTotal, bsdf.IsVolume()));
				}

				// Check if I reached the max. depth
				sampleResult.lastPathVertex = depthInfo.IsLastPathVertex(maxPathDepthInfo, bsdf.GetEventTypes());
				if (sampleResult.lastPathVertex && !sampleResult.firstPathVertex)
					break;

				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

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

				// Russian Roulette
				if (!(lastBSDFEvent & SPECULAR) && (depthInfo.GetRRDepth() >= rrDepth)) {
					const float rrProb = RenderEngine::RussianRouletteProb(bsdfSample, rrImportanceCap);
					if (rrProb < sampler.GetSample(sampleOffset + 3))
						break;

					// Increase path contribution
					pathThroughput /= rrProb;
				}

				pathThroughput *= bsdfSample;
				assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

				// Update volume information
				volInfo.Update(lastBSDFEvent, bsdf);

				eyeRay.Update(bsdf.hitPoint.p, surfaceNormal, sampledDir);
			}

			sampler.NextSample(sampleResults);

#ifdef WIN32
			// Work around Windows bad scheduling
			boost::this_thread::yield();
#endif
		}

		//----------------------------------------------------------------------
		// Add all visibility particles to the Octree (under a mutex)
		//----------------------------------------------------------------------
		
		if (visibilityParticles.size() > 0) {
			boost::unique_lock<boost::mutex> lock(particlesOctreeMutex);

			u_int cacheLookUp = 0;
			u_int cacheHits = 0;
			// I need some overlap between entries to avoid very small and
			// hard to find regions. I use a 10% overlap.
			const float maxDistance2 = Sqr(pgic.params.visibility.lookUpRadius * 0.9f);
			for (auto const &vp : visibilityParticles) {
				// Check if a cache entry is available for this point
				const u_int entryIndex = particlesOctree->GetNearestEntry(vp.p, vp.n, vp.isVolume);

				if (entryIndex == NULL_INDEX) {
					// Add as a new entry
					pgic.visibilityParticles.push_back(vp);
					particlesOctree->Add(pgic.visibilityParticles.size() - 1);
				} else {
					VisibilityParticle &entry = pgic.visibilityParticles[entryIndex];
					const float distance2 = DistanceSquared(vp.p, entry.p);

					if (distance2 > maxDistance2) {
						// Add as a new entry
						pgic.visibilityParticles.push_back(vp);
						particlesOctree->Add(pgic.visibilityParticles.size() - 1);
					} else {
						++cacheHits;

						// Update the statistics about the area covered by this entry
						entry.hitsAccumulatedDistance += sqrtf(distance2);
						entry.hitsCount += 1;
					}
				}

				++cacheLookUp;
			}
			
			visibilityCacheLookUp += cacheLookUp;
			visibilityCacheHits += cacheHits;

			// Check if I have a cache hit rate high enough to stop

			if (visibilityWarmUp && (workCounter > 8 * workSize)) {
				// End of the warm up period. Reset the cache hit counters
				cacheHits = 0;
				cacheLookUp = 0;

				visibilityWarmUp = false;
			} else if (!visibilityWarmUp && (workCounter > 2 * 8 * workSize)) {
				// After the warm up period, I can check the cache hit ratio to know
				// if it is time to stop

				cacheHitRate = (100.0 * cacheHits) / cacheLookUp;
				if ((cacheLookUp > 64 * 64) && (cacheHitRate > 100.0 * pgic.params.visibility.targetHitRate))
					break;
			}
			
			// Print some progress information
			if (threadIndex == 0) {
				const double now = WallClockTime();
				if (now - lastPrintTime > 2.0) {
					SLG_LOG(boost::format("PhotonGI visibility hits: %d/%d [%.1f%%, %.1fM paths/sec]") %
							visibilityCacheHits % 
							visibilityCacheLookUp %
							cacheHitRate %
							(workCounter / (1000.0 * (now - startTime))));

					lastPrintTime = now;
				}
			}
		}
	}
	
	if (threadIndex == 0)
		SLG_LOG("PhotonGI visibility hit rate: " << boost::str(boost::format("%.4f") % cacheHitRate) << "%");
}
