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
#include <memory>

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "luxrays/utils/thread.h"

#include "slg/core/indexoctree.h"
#include "slg/scene/scene.h"
#include "slg/engines/renderengine.h"
#include "slg/utils/scenevisibility.h"
#include "slg/utils/pathdepthinfo.h"

// Required for explicit instantiations
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
// Required for explicit instantiations
#include "slg/engines/caches/photongi/photongicache.h"
// Required for explicit instantiations
#include "slg/lights/visibility/envlightvisibilitycache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TraceVisibilityThread
//------------------------------------------------------------------------------

template <class T>
SceneVisibility<T>::TraceVisibilityThread::TraceVisibilityThread(SceneVisibility<T> &svis, const u_int index,
		SobolSamplerSharedData &sobolSharedData,
		IndexOctree<T> *octree, boost::mutex &octreeMutex,
		boost::atomic<u_int> &gParticlesCount,
		u_int &cacheLookUp, u_int &cacheHits,
		bool &warmUp) :
		sv(svis), threadIndex(index),
		visibilitySobolSharedData(sobolSharedData),
		particlesOctree(octree), particlesOctreeMutex(octreeMutex),
		globalVisibilityParticlesCount(gParticlesCount),
		visibilityCacheLookUp(cacheLookUp), visibilityCacheHits(cacheHits),
		visibilityWarmUp(warmUp),
		renderThread(nullptr) {
}

template <class T>
SceneVisibility<T>::TraceVisibilityThread::~TraceVisibilityThread() {
	Join();
}

template <class T>
void SceneVisibility<T>::TraceVisibilityThread::Start() {
	renderThread = new boost::thread(&TraceVisibilityThread::RenderFunc, this);
}

template <class T>
void SceneVisibility<T>::TraceVisibilityThread::Join() {
	if (renderThread) {
		renderThread->join();

		delete renderThread;
		renderThread = nullptr;
	}
}

template <class T>
void SceneVisibility<T>::TraceVisibilityThread::GenerateEyeRay(const Camera *camera, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const {
	const u_int *subRegion = camera->filmSubRegion;
	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0] + 1);
	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2] + 1);

	const float timeSample = sampler->GetSample(4);
	const float time = (sv.timeStart <= sv.timeEnd) ?
		Lerp(timeSample, sv.timeStart, sv.timeEnd) :
		camera->GenerateRayTime(timeSample);

	camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3));
}

template <class T>
void SceneVisibility<T>::TraceVisibilityThread::RenderFunc() {
	const u_int workSize = 4096;
	
	// Hard coded RR parameters
	const u_int rrDepth = 3;
	const float rrImportanceCap = .5f;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	const Scene *scene = sv.scene;
	const Camera *camera = scene->camera;

	// Initialize the sampler
	RandomGenerator rnd(1 + threadIndex);
	SobolSampler sampler(&rnd, NULL, NULL, true, 0.f, 0.f,
			16, 16, 1, 1,
			&visibilitySobolSharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 4;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		sv.maxPathDepth * sampleStepSize; // For each path vertex
	sampler.RequestSamples(PIXEL_NORMALIZED_ONLY, sampleSize);
	
	// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = sv.maxPathDepth;
	maxPathDepthInfo.diffuseDepth = sv.maxPathDepth;
	maxPathDepthInfo.glossyDepth = sv.maxPathDepth;
	maxPathDepthInfo.specularDepth = sv.maxPathDepth;

	vector<T> visibilityParticles;

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
		if (workCounter >= sv.maxSampleCount)
			break;
		
		u_int workToDo = (workCounter + workSize > sv.maxSampleCount) ?
			(sv.maxSampleCount - workCounter) : workSize;
		
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
				const bool hit = scene->Intersect(NULL,
						EYE_RAY | (sampleResult.firstPathVertex ? CAMERA_RAY : GENERIC_RAY),
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

				const bool continuePath = sv.ProcessHitPoint(bsdf, volInfo, visibilityParticles);
				if (!continuePath)
					break;

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

				eyeRay.Update(bsdf.GetRayOrigin(sampledDir), sampledDir);
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
			const float maxDistance2 = Sqr(sv.lookUpRadius * 0.9f);
			for (auto const &vp : visibilityParticles) {
				const bool cacheHit = sv.ProcessVisibilityParticle(vp, sv.visibilityParticles,
						particlesOctree, maxDistance2);
				if (cacheHit)
					++cacheHits;

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
				if ((cacheLookUp > 64 * 64) && (cacheHitRate > 100.0 * sv.targetHitRate))
					break;
			}
			
			// Print some progress information
			if (threadIndex == 0) {
				const double now = WallClockTime();
				if (now - lastPrintTime > 2.0) {
					SLG_LOG(boost::format("SceneVisibility hits: %d/%d [%.1f%%, %.1fM paths/sec]") %
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
		SLG_LOG("SceneVisibility hit rate: " << boost::str(boost::format("%.4f") % cacheHitRate) << "%");
}

//------------------------------------------------------------------------------
// SceneVisibility
//------------------------------------------------------------------------------

template <class T>
SceneVisibility<T>::SceneVisibility(const Scene *scn, vector<T> &parts,
		const u_int maxDepth,  const u_int sampleCount,
		const float hitRate, const float r, const float ang,
		const float t0, const float t1) :
	scene(scn), visibilityParticles(parts),
	maxPathDepth(maxDepth), maxSampleCount(sampleCount),
	targetHitRate(hitRate), lookUpRadius(r), lookUpNormalAngle(ang),
	timeStart(t0), timeEnd(t1) {
}

template <class T>
SceneVisibility<T>::~SceneVisibility() {
}

template <class T>
void SceneVisibility<T>::Build() {
	const size_t renderThreadCount = GetHardwareThreadCount();
	vector<TraceVisibilityThread *> renderThreads(renderThreadCount, nullptr);
	SLG_LOG("SceneVisibility trace thread count: " << renderThreadCount);

	// Initialize the Octree where to store the visibility points
	//
	// Note: I use an Octree because it can be built at runtime.
	unique_ptr<IndexOctree<T> > particlesOctree(AllocOctree());
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
				particlesOctree.get(), particlesOctreeMutex,
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
	SLG_LOG("SceneVisibility total entries: " << visibilityParticles.size());
}

//------------------------------------------------------------------------------
// Explicit instantiations
//------------------------------------------------------------------------------

// C++ can be quite horrible...

namespace slg {
template class SceneVisibility<DLSCVisibilityParticle>;
template class SceneVisibility<PGICVisibilityParticle>;
template class SceneVisibility<ELVCVisibilityParticle>;
}
