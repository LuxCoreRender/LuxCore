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

#include <boost/format.hpp>

#include "luxrays/utils/thread.h"

#include "slg/scene/scene.h"
#include "slg/engines/renderengine.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/engines/caches/photongi/tracephotonsthread.h"
#include "slg/utils/pathdepthinfo.h"
#include "slg/utils/pathinfo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TracePhotonsThread
//------------------------------------------------------------------------------

TracePhotonsThread::TracePhotonsThread(PhotonGICache &cache, const u_int index,
		const u_int seed, const u_int photonCount,
		const bool indirectCacheDone, const bool causticCacheDone,
		boost::atomic<u_int> &gPhotonsCounter, boost::atomic<u_int> &gIndirectPhotonsTraced,
		boost::atomic<u_int> &gCausticPhotonsTraced, boost::atomic<u_int> &gIndirectSize,
		boost::atomic<u_int> &gCausticSize) :
	pgic(cache), threadIndex(index), seedBase(seed), photonTracedCount(photonCount),
	globalPhotonsCounter(gPhotonsCounter),
	globalIndirectPhotonsTraced(gIndirectPhotonsTraced),
	globalCausticPhotonsTraced(gCausticPhotonsTraced),
	globalIndirectSize(gIndirectSize),
	globalCausticSize(gCausticSize),
	renderThread(nullptr),
	indirectDone(indirectCacheDone), causticDone(causticCacheDone) {
}

TracePhotonsThread::~TracePhotonsThread() {
	Join();
}

void TracePhotonsThread::Start() {
	indirectPhotons.clear();
	causticPhotons.clear();

	renderThread = new boost::thread(&TracePhotonsThread::RenderFunc, this);
}

void TracePhotonsThread::Join() {
	if (renderThread) {
		renderThread->join();

		delete renderThread;
		renderThread = nullptr;
	}
}

void TracePhotonsThread::UniformMutate(RandomGenerator &rndGen, vector<float> &samples) const {
	for (auto &sample: samples)
		sample = rndGen.floatValue();
}

void TracePhotonsThread::Mutate(RandomGenerator &rndGen,
		const vector<float> &currentPathSamples,
		vector<float> &candidatePathSamples,
		const float mutationSize) const {
	assert (candidatePathSamples.size() == currentPathSamples.size());
	assert (mutationSize != 0.f);

	for (u_int i = 0; i < currentPathSamples.size(); ++i) {
		const float deltaU = powf(rndGen.floatValue(), 1.f / mutationSize + 1.f);

		float mutateValue = currentPathSamples[i];
		if (rndGen.floatValue() < .5f) {
			mutateValue += deltaU;
			mutateValue = (mutateValue < 1.f) ? mutateValue : (mutateValue - 1.f);
		} else {
			mutateValue -= deltaU;
			mutateValue = (mutateValue < 0.f) ? (mutateValue + 1.f) : mutateValue;
		}
		
		// mutateValue can still be 1.f due to numerical precision problems
		candidatePathSamples[i] = (mutateValue == 1.f) ? 0.f : mutateValue;

		assert ((candidatePathSamples[i] >= 0.f) && (candidatePathSamples[i] < 1.f));
	}
}

bool TracePhotonsThread::TracePhotonPath(RandomGenerator &rndGen,
		const vector<float> &samples,
		vector<RadiancePhotonEntry> &newIndirectPhotons,
		vector<Photon> &newCausticPhotons) {
	// Hard coded RR parameters
	const u_int rrDepth = 3;
	const float rrImportanceCap = .5f;

	newIndirectPhotons.clear();
	newCausticPhotons.clear();
	vector<u_int> allNearEntryIndices;
	
	const Scene *scene = pgic.scene;
	const Camera *camera = scene->camera;

	bool usefulPath = false;
	
	Spectrum lightPathFlux;

	const float timeSample = samples[0];
	const float time = (pgic.params.photon.timeStart <= pgic.params.photon.timeEnd) ?
		Lerp(timeSample, pgic.params.photon.timeStart, pgic.params.photon.timeEnd) :
		camera->GenerateRayTime(timeSample);

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->
			SampleLights(samples[1], &lightPickPdf);

	if (light) {
		// Initialize the light path
		float lightEmitPdfW;
		Ray nextEventRay;
		lightPathFlux = light->Emit(*scene,
				time, samples[2], samples[3], samples[4], samples[5], samples[6],
				nextEventRay, lightEmitPdfW);

		if (!lightPathFlux.Black()) {
			lightPathFlux /= lightEmitPdfW * lightPickPdf;
			assert (lightPathFlux.IsValid());

			//------------------------------------------------------------------
			// Trace the light path
			//------------------------------------------------------------------

			LightPathInfo pathInfo;
			for (;;) {
				const u_int sampleOffset = sampleBootSize +	pathInfo.depth.depth * sampleStepSize;

				RayHit nextEventRayHit;
				BSDF bsdf;
				Spectrum connectionThroughput;
				const bool hit = scene->Intersect(nullptr, LIGHT_RAY | GENERIC_RAY, &pathInfo.volume, samples[sampleOffset],
						&nextEventRay, &nextEventRayHit, &bsdf,
						&connectionThroughput);

				if (hit) {
					// Something was hit

					lightPathFlux *= connectionThroughput;

					//----------------------------------------------------------
					// Deposit photons only on diffuse surfaces
					//----------------------------------------------------------

					if (pgic.IsPhotonGIEnabled(bsdf) &&
							// This is an anti-NaNs/Infs safety net to avoid poisoning
							// the caches
							lightPathFlux.IsValid()) {
						// Flip the normal if required
						const Normal landingSurfaceNormal = ((Dot(bsdf.hitPoint.geometryN, -nextEventRay.d) > 0.f) ?
							1.f : -1.f) * bsdf.hitPoint.geometryN;

						// Check if the point is visible
						allNearEntryIndices.clear();
						pgic.visibilityParticlesKdTree->GetAllNearEntries(allNearEntryIndices,
								bsdf.hitPoint.p, landingSurfaceNormal, bsdf.IsVolume(),
								pgic.params.visibility.lookUpRadius2,
								pgic.params.visibility.lookUpNormalCosAngle);

						if (allNearEntryIndices.size() > 0) {
							if ((pathInfo.depth.depth > 0) && pathInfo.IsSpecularPath() && !causticDone) {
								// It is a caustic photon
								newCausticPhotons.push_back(Photon(bsdf.hitPoint.p, nextEventRay.d,
										light->GetID(), lightPathFlux, landingSurfaceNormal, bsdf.IsVolume()));

								usefulPath = true;
							}
							
							if (!indirectDone) {
								// It is an indirect photon

								// Add outgoingRadiance to each near visible entry 
								for (auto const &vpIndex : allNearEntryIndices)
									newIndirectPhotons.push_back(RadiancePhotonEntry(vpIndex,
											light->GetID(), lightPathFlux));

								usefulPath = true;
							}
						}
					}

					if (pathInfo.depth.depth + 1 >= pgic.params.photon.maxPathDepth)
						break;

					//----------------------------------------------------------
					// Build the next vertex path ray
					//----------------------------------------------------------

					float bsdfPdf;
					Vector sampledDir;
					BSDFEvent bsdfEvent;
					float cosSampleDir;
					const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
							samples[sampleOffset + 2],
							samples[sampleOffset + 3],
							&bsdfPdf, &cosSampleDir, &bsdfEvent);
					if (bsdfSample.Black())
						break;

					pathInfo.AddVertex(bsdf, bsdfEvent, pgic.params.glossinessUsageThreshold);

					// If I have to fill only the caustic cache and last BSDF event
					// is not a (nearly) specular one, I can stop with this path
					if (indirectDone && !causticDone && !pathInfo.IsSpecularPath())
						break;

					// Russian Roulette
					if (pathInfo.UseRR(rrDepth)) {
						const float rrProb = RenderEngine::RussianRouletteProb(bsdfSample, rrImportanceCap);
						if (rrProb < samples[sampleOffset + 4])
							break;

						// Increase path contribution
						lightPathFlux /= rrProb;
					}
					
					lightPathFlux *= bsdfSample;
					assert (lightPathFlux.IsValid());

					nextEventRay.Update(bsdf.GetRayOrigin(sampledDir), sampledDir);
				} else {
					// Ray lost in space...
					break;
				}
			}
		}
	}

	return usefulPath;
}

void TracePhotonsThread::AddPhotons(const vector<RadiancePhotonEntry> &newIndirectPhotons,
		const vector<Photon> &newCausticPhotons) {
	indirectPhotons.insert(indirectPhotons.end(), newIndirectPhotons.begin(),
			newIndirectPhotons.end());
	causticPhotons.insert(causticPhotons.end(), newCausticPhotons.begin(),
			newCausticPhotons.end());
}

void TracePhotonsThread::AddPhotons(const float currentPhotonsScale,
		const vector<RadiancePhotonEntry> &newIndirectPhotons,
		const vector<Photon> &newCausticPhotons) {
	for (auto const &photon : newIndirectPhotons) {
		indirectPhotons.push_back(photon);
		indirectPhotons.back().alpha *= currentPhotonsScale;
	}
	
	for (auto const &photon : newCausticPhotons) {
		causticPhotons.push_back(photon);
		causticPhotons.back().alpha *= currentPhotonsScale;
	}
}

// The metropolis sampler used here is based on:
//  "Robust Adaptive Photon Tracing using Photon Path Visibility"
//  by TOSHIYA HACHISUKA and HENRIK WANN JENSEN

void TracePhotonsThread::RenderFunc() {
	const u_int workSize = 4096;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);
	
	RandomGenerator rndGen(seedBase + threadIndex);

	sampleBootSize = 7;
	sampleStepSize = 5;
	sampleSize = 
			sampleBootSize + // To generate the initial setup
			pgic.params.photon.maxPathDepth * sampleStepSize; // For each light vertex

	vector<float> currentPathSamples(sampleSize);
	vector<float> candidatePathSamples(sampleSize);
	vector<float> uniformPathSamples(sampleSize);

	vector<RadiancePhotonEntry> currentIndirectPhotons;
	vector<Photon> currentCausticPhotons;

	vector<RadiancePhotonEntry> candidateIndirectPhotons;
	vector<Photon> candidateCausticPhotons;

	vector<RadiancePhotonEntry> uniformIndirectPhotons;
	vector<Photon> uniformCausticPhotons;

	//--------------------------------------------------------------------------
	// Get a bucket of work to do
	//--------------------------------------------------------------------------

	const double startTime = WallClockTime();
	double lastPrintTime = startTime;
	bool foundUsefulFirstPrint = true;
	while(!boost::this_thread::interruption_requested()) {
		// Get some work to do
		u_int workCounter;
		do {
			workCounter = globalPhotonsCounter;
		} while (!globalPhotonsCounter.compare_exchange_weak(workCounter, workCounter + workSize));

		// Check if it is time to stop
		if (workCounter >= photonTracedCount)
			break;

		indirectDone = indirectDone || (!pgic.params.indirect.enabled) ||
				((pgic.params.indirect.maxSize > 0) && (globalIndirectSize >= pgic.params.indirect.maxSize));
		causticDone = causticDone || (!pgic.params.caustic.enabled) ||
				(globalCausticSize >= pgic.params.caustic.maxSize);

		// Check if it is time to stop
		if (indirectDone && causticDone)
			break;

		u_int workToDo = (workCounter + workSize > photonTracedCount) ?
			(photonTracedCount - workCounter) : workSize;

		if (!indirectDone)
			globalIndirectPhotonsTraced += workToDo;
		if (!causticDone)
			globalCausticPhotonsTraced += workToDo;

		// Print some progress information
		if (threadIndex == 0) {
			const double now = WallClockTime();
			if (now - lastPrintTime > 2.0) {
				const float indirectProgress = !indirectDone ?
					(((globalIndirectSize > 0) && (pgic.params.indirect.maxSize > 0)) ?
						((100.0 * globalIndirectSize) / pgic.params.indirect.maxSize) : 0.f) :
					100.f;
				const float causticProgress = !causticDone ?
					((globalCausticSize > 0) ? ((100.0 * globalCausticSize) / pgic.params.caustic.maxSize) : 0.f) :
					100.f;

				SLG_LOG(boost::format("PhotonGI Cache photon traced: %d/%d [%.1f%%, %.1fM photons/sec, Map sizes (%.1f%%, %.1f%%)]") %
						workCounter % photonTracedCount %
						((100.0 * workCounter) / photonTracedCount) %
						(workCounter / (1000.0 * (now - startTime))) %
						indirectProgress %
						causticProgress);
				lastPrintTime = now;
			}
		}

		const u_int indirectPhotonsStart = indirectPhotons.size();
		const u_int causticPhotonsStart = causticPhotons.size();

		//----------------------------------------------------------------------
		// Metropolis Sampler
		//----------------------------------------------------------------------

		if (pgic.params.samplerType == PGIC_SAMPLER_METROPOLIS) {
			// Look for a useful path to start with

			bool foundUseful = false;
			for (u_int i = 0; i < 16384; ++i) {
				UniformMutate(rndGen, currentPathSamples);

				foundUseful = TracePhotonPath(rndGen, currentPathSamples, currentIndirectPhotons,
						currentCausticPhotons);
				if (foundUseful)
					break;

#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}

			if (!foundUseful) {
				// I was unable to find a useful path. Something wrong. this
				// may be an empty scene, a dark room, etc.
				if (foundUsefulFirstPrint) {
					SLG_LOG("PhotonGI metropolis sampler is unable to find a useful light path");
					foundUsefulFirstPrint = false;
				}
			} else {
				// Trace light paths

				u_int currentPhotonsScale = 1;
				float mutationSize = 1.f;
				u_int acceptedCount = 1;
				u_int mutatedCount = 1;
				u_int uniformCount = 1;
				u_int workToDoIndex = workToDo;
				while (workToDoIndex-- && !boost::this_thread::interruption_requested()) {
					UniformMutate(rndGen, uniformPathSamples);

					if (TracePhotonPath(rndGen, uniformPathSamples, uniformIndirectPhotons,
							uniformCausticPhotons)) {
						// Add the old current photons (scaled by currentPhotonsScale)
						AddPhotons(currentPhotonsScale, currentIndirectPhotons, currentCausticPhotons);

						// The candidate path becomes the current one
						copy(uniformPathSamples.begin(), uniformPathSamples.end(), currentPathSamples.begin());

						currentPhotonsScale = 1;
						currentIndirectPhotons = uniformIndirectPhotons;
						currentCausticPhotons = uniformCausticPhotons;

						++uniformCount;
					} else {
						// Try a mutation of the current path
						Mutate(rndGen, currentPathSamples, candidatePathSamples, mutationSize);
						++mutatedCount;

						if (TracePhotonPath(rndGen, candidatePathSamples, candidateIndirectPhotons,
								candidateCausticPhotons)) {
							// Add the old current photons (scaled by currentPhotonsScale)
							AddPhotons(currentPhotonsScale, currentIndirectPhotons, currentCausticPhotons);

							// The candidate path becomes the current one
							copy(candidatePathSamples.begin(), candidatePathSamples.end(), currentPathSamples.begin());

							currentPhotonsScale = 1;
							currentIndirectPhotons = candidateIndirectPhotons;
							currentCausticPhotons = candidateCausticPhotons;

							++acceptedCount;
						} else
							++currentPhotonsScale;

						const float R = acceptedCount / (float)mutatedCount;
						// 0.234 => the optimal asymptotic acceptance ratio has been
						// derived 23.4% [Roberts et al. 1997]
						mutationSize += (R - .234f) / mutatedCount;
					}

#ifdef WIN32
					// Work around Windows bad scheduling
					renderThread->yield();
#endif
				}

				// Add the last current photons (scaled by currentPhotonsScale)
				if (currentPhotonsScale > 1) {
					AddPhotons(currentPhotonsScale, currentIndirectPhotons, currentCausticPhotons);
				}

				// Scale all photon values
				const float scaleFactor = uniformCount /  (float)workToDo;

				for (u_int i = indirectPhotonsStart; i < indirectPhotons.size(); ++i)
					indirectPhotons[i].alpha *= scaleFactor;
				for (u_int i = causticPhotonsStart; i < causticPhotons.size(); ++i)
					causticPhotons[i].alpha *= scaleFactor;
			}
		} else

		//----------------------------------------------------------------------
		// Random Sampler
		//----------------------------------------------------------------------

		if (pgic.params.samplerType == PGIC_SAMPLER_RANDOM) {
			// Trace light paths

			u_int workToDoIndex = workToDo;
			while (workToDoIndex-- && !boost::this_thread::interruption_requested()) {
				UniformMutate(rndGen, currentPathSamples);

				TracePhotonPath(rndGen, currentPathSamples, currentIndirectPhotons, currentCausticPhotons);

				// Add the new photons
				AddPhotons(currentIndirectPhotons, currentCausticPhotons);

#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}
		} else
			throw runtime_error("Unknown sampler type in TracePhotonsThread::RenderFunc(): " + ToString(pgic.params.samplerType));

		//----------------------------------------------------------------------
		
		// Update size counters
		globalIndirectSize += indirectPhotons.size() - indirectPhotonsStart;
		globalCausticSize += causticPhotons.size() - causticPhotonsStart;
	}
}
