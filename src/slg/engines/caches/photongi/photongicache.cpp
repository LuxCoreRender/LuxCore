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
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

// TODO: serialization

//------------------------------------------------------------------------------
// TracePhotonsThread
//------------------------------------------------------------------------------

TracePhotonsThread::TracePhotonsThread(PhotonGICache &cache, const u_int index) :
	pgic(cache), threadIndex(index), renderThread(nullptr) {
}

TracePhotonsThread::~TracePhotonsThread() {
	Join();
}

void TracePhotonsThread::Start() {
	renderThread = new boost::thread(&TracePhotonsThread::RenderFunc, this);
}

void TracePhotonsThread::Join() {
	if (renderThread) {
		renderThread->join();

		delete renderThread;
		renderThread = nullptr;
	}
}

void TracePhotonsThread::RenderFunc() {
	//const u_int workSize = 8192;
	const u_int workSize = 100;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	RandomGenerator rndGen(1 + threadIndex);
	//const Scene *scene = pgic.scene;
	//const Camera *camera = scene->camera;

	// Setup the sampler
	MetropolisSampler sampler(&rndGen, NULL, NULL, 512, .4f, .1f, &pgic.samplerSharedData);
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 3;
	const u_int sampleSize = 
		sampleBootSize + // To generate the initial setup
		pgic.maxPathDepth * sampleStepSize; // For each light vertex
	sampler.RequestSamples(sampleSize);

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults;
	double lastPrintTime = WallClockTime();
	while(!boost::this_thread::interruption_requested()) {
		// Get some work to do
		u_int workCounter;
		do {
			workCounter = pgic.globalCounter;
		} while (!pgic.globalCounter.compare_exchange_weak(workCounter, workCounter + workSize));

		// Check if it is time to stop
		if (workCounter >= pgic.photonCount)
			break;

		// Print some progress information
		if (threadIndex == 0) {
			const double now = WallClockTime();
			if (now - lastPrintTime > 2.0) {
				SLG_LOG("Photon GI Cache photon traced: " << workCounter << "/" << pgic.photonCount <<" (" << (u_int)((100.0 * workCounter) / pgic.photonCount) << "%)");
				lastPrintTime = now;
			}
		}
		
		u_int workToDo = (workCounter + workSize > pgic.photonCount) ?
			(pgic.photonCount - workCounter) : workSize;
		
		while (workToDo-- && !boost::this_thread::interruption_requested()) {
			boost::this_thread::sleep(boost::posix_time::millisec(1));
			/*
			Spectrum lightPathFlux;
			sampleResults.clear();
			lightPathFlux = Spectrum();

			const float timeSample = sampler->GetSample(12);
			const float time = scene->camera->GenerateRayTime(timeSample);

			// Select one light source
			float lightPickPdf;
			const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->
					SampleLights(sampler->GetSample(2), &lightPickPdf);

			if (light) {
				// Initialize the light path
				float lightEmitPdfW;
				Ray nextEventRay;
				lightPathFlux = light->Emit(*scene,
					sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5), sampler->GetSample(6), sampler->GetSample(7),
						&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW);
				nextEventRay.UpdateMinMaxWithEpsilon();
				nextEventRay.time = time;

				if (lightPathFlux.Black()) {
					sampler->NextSample(sampleResults);
					continue;
				}
				lightPathFlux /= lightEmitPdfW * lightPickPdf;
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				// Sample a point on the camera lens
				Point lensPoint;
				if (!camera->SampleLens(time, sampler->GetSample(8), sampler->GetSample(9),
						&lensPoint)) {
					sampler->NextSample(sampleResults);
					continue;
				}

				//----------------------------------------------------------------------
				// I don't try to connect the light vertex directly with the eye
				// because InfiniteLight::Emit() returns a point on the scene bounding
				// sphere. Instead, I trace a ray from the camera like in BiDir.
				// This is also a good way to test the Film Per-Pixel-Normalization and
				// the Per-Screen-Normalization Buffers used by BiDir.
				//----------------------------------------------------------------------

				PathVolumeInfo eyeVolInfo;
				TraceEyePath(timeSample, sampler, eyeVolInfo, sampleResults);

				//----------------------------------------------------------------------
				// Trace the light path
				//----------------------------------------------------------------------

				int depth = 1;
				PathVolumeInfo volInfo;
				while (depth <= engine->maxPathDepth) {
					const u_int sampleOffset = sampleBootSize + sampleEyeStepSize * engine->maxPathDepth +
						(depth - 1) * sampleLightStepSize;

					RayHit nextEventRayHit;
					BSDF bsdf;
					Spectrum connectionThroughput;
					const bool hit = scene->Intersect(device, true, false, &volInfo, sampler->GetSample(sampleOffset),
							&nextEventRay, &nextEventRayHit, &bsdf,
							&connectionThroughput);

					if (hit) {
						// Something was hit

						lightPathFlux *= connectionThroughput;

						//--------------------------------------------------------------
						// Try to connect the light path vertex with the eye
						//--------------------------------------------------------------

						ConnectToEye(nextEventRay.time,
								sampler->GetSample(sampleOffset + 1), *light,
								bsdf, lensPoint, lightPathFlux, volInfo, sampleResults);

						if (depth >= engine->maxPathDepth)
							break;

						//--------------------------------------------------------------
						// Build the next vertex path ray
						//--------------------------------------------------------------

						float bsdfPdf;
						Vector sampledDir;
						BSDFEvent event;
						float cosSampleDir;
						const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
								sampler->GetSample(sampleOffset + 2),
								sampler->GetSample(sampleOffset + 3),
								&bsdfPdf, &cosSampleDir, &event);
						if (bsdfSample.Black())
							break;

						if (depth >= engine->rrDepth) {
							// Russian Roulette
							const float prob = RenderEngine::RussianRouletteProb(bsdfSample, engine->rrImportanceCap);
							if (sampler->GetSample(sampleOffset + 4) < prob)
								bsdfPdf *= prob;
							else
								break;
						}

						lightPathFlux *= bsdfSample;
						assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

						// Update volume information
						volInfo.Update(event, bsdf);

						nextEventRay.Update(bsdf.hitPoint.p, sampledDir);
						++depth;
					} else {
						// Ray lost in space...
						break;
					}
				}
			}

			// Variance clamping
			if (varianceClamping.hasClamping())
				for(u_int i = 0; i < sampleResults.size(); ++i)
					varianceClamping.Clamp(*(engine->film), sampleResults[i]);

			sampler->NextSample(sampleResults);

	#ifdef WIN32
			// Work around Windows bad scheduling
			renderThread->yield();
	#endif

			// Check halt conditions
			if ((haltDebug > 0u) && (steps >= haltDebug))
				break;
			if (engine->film->GetConvergence() == 1.f)
				break;*/
		}
	}
}

//------------------------------------------------------------------------------
// PhotonGICache
//------------------------------------------------------------------------------

PhotonGICache::PhotonGICache(const Scene *scn, const u_int pCount,
		const u_int pathDepth) : scene(scn), photonCount(pCount),
		maxPathDepth(pathDepth) {
}

PhotonGICache::~PhotonGICache() {
}

void PhotonGICache::TracePhotons() {
	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	std::vector<TracePhotonsThread *> renderThreads(renderThreadCount, nullptr);
	
	// Start the photon tracing threads
	globalCounter = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i] = new TracePhotonsThread(*this, i);

	// Start photon tracing threads
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
	
	// Wait for the end of photon tracing threads
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		renderThreads[i]->Join();

		delete renderThreads[i];
		renderThreads[i] = nullptr;
	}
}

/*void PhotonGICache::TracePhotons() {
	const u_int workSize = 8192;

	double lastPrintTime = WallClockTime();
	atomic<u_int> counter(0);

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < photonCount / workSize; ++i) {
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
				SLG_LOG("Photon GI Cache photon traced: " << counter << "/" << photonCount <<" (" << (u_int)((100.0 * counter) / photonCount) << "%)");
				lastPrintTime = now;
			}
		}

		AddPhotons(i * workSize, workSize);
		counter += workSize;
	}
}*/

void PhotonGICache::Preprocess() {
	TracePhotons();
}

Properties PhotonGICache::ToProperties(const Properties &cfg) {
	Properties props;
	
	props <<
			cfg.Get(GetDefaultProps().Get("path.photongi.enabled")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.count")) <<
			cfg.Get(GetDefaultProps().Get("path.photongi.depth"));

	return props;
}

const Properties &PhotonGICache::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.photongi.enabled")(false) <<
			Property("path.photongi.count")(100000) <<
			Property("path.photongi.depth")(4);

	return props;
}

PhotonGICache *PhotonGICache::FromProperties(const Scene *scn, const Properties &cfg) {
	const bool enabled = cfg.Get(GetDefaultProps().Get("path.photongi.enabled")).Get<bool>();
	
	if (enabled) {
		const u_int count = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.count")).Get<u_int>());
		const u_int depth = Max(1u, cfg.Get(GetDefaultProps().Get("path.photongi.depth")).Get<u_int>());

		return new PhotonGICache(scn, count, depth);
	} else
		return nullptr;
}
