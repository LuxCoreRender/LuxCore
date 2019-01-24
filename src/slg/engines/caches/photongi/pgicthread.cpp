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
#include "slg/samplers/random.h"
#include "slg/samplers/sobol.h"
#include "slg/samplers/metropolis.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

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
	directPhotons.clear();
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
	assert (candidatePathSamples().size() == currentPathSamples.size());
	assert (mutationSize != 0.f);

	for (u_int i = 0; i < currentPathSamples.size(); ++i) {
		const float deltaU = ((rndGen.floatValue() < .5f) ? -1.f : 1.f) *
				powf(rndGen.floatValue(), 1.f / mutationSize + 1.f);
		
		candidatePathSamples[i] = currentPathSamples[i] + deltaU;
		
		// candidatePathSamples[i] can still be 1.f due to numerical precision problems
		if (candidatePathSamples[i] == 1.f)
			candidatePathSamples[i] = 0.f;
	}
}

bool TracePhotonsThread::TeacePhotonPath(RandomGenerator &rndGen,
		const vector<float> &samples,
		vector<Photon> *newDirectPhotons,
		vector<Photon> *newIndirectPhotons,
		vector<Photon> *newCausticPhotons,
		vector<RadiancePhoton> *newRadiancePhotons) {
	if (newDirectPhotons)
		newDirectPhotons->clear();
	if (newIndirectPhotons)
		newIndirectPhotons->clear();
	if (newCausticPhotons)
		newCausticPhotons->clear();
	if (newRadiancePhotons)
		newRadiancePhotons->clear();
	
	const Scene *scene = pgic.scene;
	const Camera *camera = scene->camera;

	bool usefulPath = false;
	
	Spectrum lightPathFlux;
	lightPathFlux = Spectrum();

	const float timeSample = samples[8];
	const float time = camera->GenerateRayTime(timeSample);

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->
			SampleLights(samples[2], &lightPickPdf);

	if (light) {
		// Initialize the light path
		float lightEmitPdfW;
		Ray nextEventRay;
		lightPathFlux = light->Emit(*scene,
			samples[3], samples[4], samples[5], samples[6], samples[7],
				&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW);
		nextEventRay.UpdateMinMaxWithEpsilon();
		nextEventRay.time = time;

		if (!lightPathFlux.Black()) {
			lightPathFlux /= lightEmitPdfW * lightPickPdf;
			assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

			//------------------------------------------------------------------
			// Trace the light path
			//------------------------------------------------------------------

			bool specularPath = true;
			u_int depth = 1;
			PathVolumeInfo volInfo;
			while (depth <= pgic.maxPathDepth) {
				const u_int sampleOffset = sampleBootSize +	(depth - 1) * sampleStepSize;

				RayHit nextEventRayHit;
				BSDF bsdf;
				Spectrum connectionThroughput;
				const bool hit = scene->Intersect(nullptr, true, false, &volInfo, samples[sampleOffset],
						&nextEventRay, &nextEventRayHit, &bsdf,
						&connectionThroughput);

				if (hit) {
					// Something was hit

					lightPathFlux *= connectionThroughput;

					//----------------------------------------------------------
					// Deposit photons only on diffuse surfaces
					//----------------------------------------------------------

					if (bsdf.IsPhotonGIEnabled()) {
						const Spectrum alpha = lightPathFlux * AbsDot(bsdf.hitPoint.shadeN, -nextEventRay.d);

						// Flip the normal if required
						const Normal landingSurfaceNormal = ((Dot(bsdf.hitPoint.shadeN, -nextEventRay.d) > 0.f) ?
							1.f : -1.f) * bsdf.hitPoint.shadeN;

						bool usedPhoton = false;
						if ((depth == 1) && (pgic.directEnabled || pgic.indirectEnabled)) {
							// It is a direct light photon
							if (!directDone && newDirectPhotons) {
								newDirectPhotons->push_back(Photon(bsdf.hitPoint.p, nextEventRay.d,
										alpha, landingSurfaceNormal));
								usedPhoton = true;
							}

							usefulPath = true;
						} else if ((depth > 1) && specularPath && (pgic.causticEnabled || pgic.indirectEnabled)) {
							// It is a caustic photon
							if (!causticDone && newCausticPhotons) {
								newCausticPhotons->push_back(Photon(bsdf.hitPoint.p, nextEventRay.d,
										alpha, landingSurfaceNormal));
								usedPhoton = true;
							}

							usefulPath = true;
						} else if (pgic.indirectEnabled) {
							// It is an indirect photon
							if (!indirectDone && newIndirectPhotons) {
								newIndirectPhotons->push_back(Photon(bsdf.hitPoint.p, nextEventRay.d,
										alpha, landingSurfaceNormal));
								usedPhoton = true;
							}

							usefulPath = true;
						} 

						if (usedPhoton) {
							// Decide if to deposit a radiance photon
							if (newRadiancePhotons && (pgic.indirectEnabled) && (rndGen.floatValue() > .1f)) {
								// I save the bsdf.EvaluateTotal() for later usage while
								// the radiance photon values are computed.

								newRadiancePhotons->push_back(RadiancePhoton(bsdf.hitPoint.p,
										landingSurfaceNormal, bsdf.EvaluateTotal()));
							}
						}
					}

					if (depth >= pgic.maxPathDepth)
						break;

					//----------------------------------------------------------
					// Build the next vertex path ray
					//----------------------------------------------------------

					float bsdfPdf;
					Vector sampledDir;
					BSDFEvent event;
					float cosSampleDir;
					const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
							samples[sampleOffset + 2],
							samples[sampleOffset + 3],
							&bsdfPdf, &cosSampleDir, &event);
					if (bsdfSample.Black())
						break;

					// Is it still a specular path ?
					specularPath = specularPath && (event & SPECULAR);

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
	}

	return usefulPath;
}

void TracePhotonsThread::AddPhotons(vector<Photon> &newDirectPhotons,
		const vector<Photon> &newIndirectPhotons,
		const vector<Photon> &newCausticPhotons,
		const vector<RadiancePhoton> &newRadiancePhotons) {
	directPhotons.insert(directPhotons.end(), newDirectPhotons.begin(),
			newDirectPhotons.end());
	indirectPhotons.insert(indirectPhotons.end(), newIndirectPhotons.begin(),
			newIndirectPhotons.end());
	causticPhotons.insert(causticPhotons.end(), newCausticPhotons.begin(),
			newCausticPhotons.end());
	radiancePhotons.insert(radiancePhotons.end(), newRadiancePhotons.begin(),
			newRadiancePhotons.end());	
}

// The metropolis sampler used here is based on:
//  "Robust Adaptive Photon Tracing using Photon Path Visibility"
//  by TOSHIYA HACHISUKA and HENRIK WANN JENSEN

void TracePhotonsThread::RenderFunc() {
	const u_int workSize = 4096;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	RandomGenerator rndGen(1 + threadIndex);

	sampleBootSize = 9;
	sampleStepSize = 4;
	sampleSize = 
			sampleBootSize + // To generate the initial setup
			pgic.maxPathDepth * sampleStepSize; // For each light vertex
	vector<float> currentPathSamples(sampleSize);
	vector<float> candidatePathSamples(sampleSize);

	vector<Photon> newDirectPhotons;
	vector<Photon> newIndirectPhotons;
	vector<Photon> newCausticPhotons;
	vector<RadiancePhoton> newRadiancePhotons;

	//--------------------------------------------------------------------------
	// Get a bucket of work to do
	//--------------------------------------------------------------------------

	const double startTime = WallClockTime();
	double lastPrintTime = WallClockTime();
	while(!boost::this_thread::interruption_requested()) {
		// Get some work to do
		u_int workCounter;
		do {
			workCounter = pgic.globalPhotonsCounter;
		} while (!pgic.globalPhotonsCounter.compare_exchange_weak(workCounter, workCounter + workSize));

		// Check if it is time to stop
		if (workCounter >= pgic.maxPhotonTracedCount)
			break;

		directDone = (pgic.globalDirectSize >= pgic.maxDirectSize);
		indirectDone = (pgic.globalIndirectSize >= pgic.maxIndirectSize);
		causticDone = (pgic.globalCausticSize >= pgic.maxCausticSize);

		u_int workToDo = (workCounter + workSize > pgic.maxPhotonTracedCount) ?
			(pgic.maxPhotonTracedCount - workCounter) : workSize;

		if (!directDone)
			pgic.globalDirectPhotonsTraced += workToDo;
		if (!indirectDone)
			pgic.globalIndirectPhotonsTraced += workToDo;
		if (!causticDone)
			pgic.globalCausticPhotonsTraced += workToDo;

		// Print some progress information
		if (threadIndex == 0) {
			const double now = WallClockTime();
			if (now - lastPrintTime > 2.0) {
				const float directProgress = pgic.directEnabled ?
					((pgic.maxDirectSize > 0) ? ((100.0 * pgic.globalDirectSize) / pgic.maxDirectSize) : 0.f) :
					100.f;
				const float indirectProgress = pgic.indirectEnabled ?
					((pgic.globalIndirectSize > 0) ? ((100.0 * pgic.globalIndirectSize) / pgic.maxIndirectSize) : 0.f) :
					100.f;
				const float causticProgress = pgic.causticEnabled ?
					((pgic.globalCausticSize > 0) ? ((100.0 * pgic.globalCausticSize) / pgic.maxCausticSize) : 0.f) :
					100.f;
				
				SLG_LOG(boost::format("Photon GI Cache photon traced: %d/%d [%.1f%%, %.1fM photons/sec, Map sizes (%.1f%%, %.1f%%, %.1f%%)]") %
						workCounter % pgic.maxPhotonTracedCount %
						((100.0 * workCounter) / pgic.maxPhotonTracedCount) %
						(workCounter / (1000.0 * (WallClockTime() - startTime))) %
						directProgress %
						indirectProgress %
						causticProgress);
				lastPrintTime = now;
			}
		}

		const u_int directPhotonsStart = directPhotons.size();
		const u_int indirectPhotonsStart = indirectPhotons.size();
		const u_int causticPhotonsStart = causticPhotons.size();

		//----------------------------------------------------------------------
		// Metropolis Sampler
		//----------------------------------------------------------------------

		if (pgic.samplerType == PGIC_SAMPLER_METROPOLIS) {
			// Look for a useful path to start with

			bool foundUseful = false;
			for (u_int i = 0; i < 16384; ++i) {
				UniformMutate(rndGen, currentPathSamples);

				foundUseful = TeacePhotonPath(rndGen, currentPathSamples);
				if (foundUseful)
					break;

#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}

			if (!foundUseful) {
				// I was unable to find a useful path. Something wrong. this
				// may be an empty scene.
				throw runtime_error("Unable to find a useful path in TracePhotonsThread::RenderFunc()");
			}

			// Trace light paths

			float mutationSize = 1.f;
			u_int acceptedCount = 1;
			u_int mutatedCount = 1;
			u_int uniformCount = 1;
			u_int workToDoIndex = workToDo;
			while (workToDoIndex-- && !boost::this_thread::interruption_requested()) {
				UniformMutate(rndGen, candidatePathSamples);

				if (TeacePhotonPath(rndGen, candidatePathSamples, &newDirectPhotons,
						&newIndirectPhotons, &newCausticPhotons, &newRadiancePhotons)) {
					// The candidate path becomes the current one
					copy(candidatePathSamples.begin(), candidatePathSamples.end(), currentPathSamples.begin());
					++uniformCount;

					// Add the new photons
					AddPhotons(newDirectPhotons, newIndirectPhotons, newCausticPhotons,
							newRadiancePhotons);
				} else {
					// Try a mutation of the current path
					Mutate(rndGen, currentPathSamples, candidatePathSamples, mutationSize);
					++mutatedCount;

					if (TeacePhotonPath(rndGen, candidatePathSamples, &newDirectPhotons,
							&newIndirectPhotons, &newCausticPhotons, &newRadiancePhotons)) {
						// The candidate path becomes the current one
						copy(candidatePathSamples.begin(), candidatePathSamples.end(), currentPathSamples.begin());
						++acceptedCount;

						// Add the new photons
						AddPhotons(newDirectPhotons, newIndirectPhotons, newCausticPhotons,
								newRadiancePhotons);
					}

					const float R = acceptedCount / (float)mutatedCount;
					mutationSize += (R - .234f) / mutatedCount;
				}

#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}

			// Scale all photon values
			const float scaleFactor = uniformCount /  (float)workToDo;

			for (u_int i = directPhotonsStart; i < directPhotons.size(); ++i)
				directPhotons[i].alpha *= scaleFactor;
			for (u_int i = indirectPhotonsStart; i < indirectPhotons.size(); ++i)
				indirectPhotons[i].alpha *= scaleFactor;
			for (u_int i = causticPhotonsStart; i < causticPhotons.size(); ++i)
				causticPhotons[i].alpha *= scaleFactor;
		} else

		//----------------------------------------------------------------------
		// Random Sampler
		//----------------------------------------------------------------------

		if (pgic.samplerType == PGIC_SAMPLER_RANDOM) {
			// Trace light paths

			u_int workToDoIndex = workToDo;
			while (workToDoIndex-- && !boost::this_thread::interruption_requested()) {
				UniformMutate(rndGen, currentPathSamples);

				TeacePhotonPath(rndGen, currentPathSamples, &newDirectPhotons,
						&newIndirectPhotons, &newCausticPhotons, &newRadiancePhotons);

				// Add the new photons
				AddPhotons(newDirectPhotons, newIndirectPhotons, newCausticPhotons,
						newRadiancePhotons);

#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}
		}
		
		//----------------------------------------------------------------------
		
		// Update size counters
		pgic.globalDirectSize += directPhotons.size() - directPhotonsStart;
		pgic.globalIndirectSize += indirectPhotons.size() - indirectPhotonsStart;
		pgic.globalCausticSize += causticPhotons.size() - causticPhotonsStart;

		// Check if it is time to stop. I can do the check only here because
		// globalPhotonsTraced was already incremented
		if (directDone && indirectDone && causticDone)
			break;
	}
}
