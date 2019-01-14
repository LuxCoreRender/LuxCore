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

SampleResult &TracePhotonsThread::AddResult(vector<SampleResult> &sampleResults, const bool fromLight) const {
	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	SampleResult &sampleResult = sampleResults[size];

	sampleResult.Init(
			fromLight ?
				Film::RADIANCE_PER_SCREEN_NORMALIZED :
				(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA |
				Film::DEPTH | Film::SAMPLECOUNT),
			1);

	return sampleResult;
}

void TracePhotonsThread::ConnectToEye(const float time, const float u0,
		const LightSource &light,
		const BSDF &bsdf, const Point &lensPoint,
		const Spectrum &flux, PathVolumeInfo volInfo,
		vector<SampleResult> &sampleResults) {
	// I don't connect camera invisible objects with the eye
	if (bsdf.IsCameraInvisible())
		return;

	const Scene *scene = pgic.scene;

	Vector eyeDir(bsdf.hitPoint.p - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(-eyeDir, &event);

	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir,
				0.f,
				eyeDistance,
				time);
		eyeRay.UpdateMinMaxWithEpsilon();

		float filmX, filmY;
		if (scene->camera->GetSamplePosition(&eyeRay, &filmX, &filmY)) {
			// I have to flip the direction of the traced ray because
			// the information inside PathVolumeInfo are about the path from
			// the light toward the camera (i.e. ray.o would be in the wrong
			// place).
			Ray traceRay(bsdf.hitPoint.p, -eyeRay.d,
					0.f,
					eyeRay.maxt,
					time);
			traceRay.UpdateMinMaxWithEpsilon();
			RayHit traceRayHit;

			BSDF bsdfConn;
			Spectrum connectionThroughput;
			if (!scene->Intersect(nullptr, true, true, &volInfo, u0, &traceRay, &traceRayHit, &bsdfConn,
					&connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				const float cameraPdfW = scene->camera->GetPDF(eyeDir, filmX, filmY);
				const float fluxToRadianceFactor = cameraPdfW / (eyeDistance * eyeDistance);

				SampleResult &sampleResult = AddResult(sampleResults, true);
				sampleResult.filmX = filmX;
				sampleResult.filmY = filmY;

				// Add radiance from the light source
				sampleResult.radiance[light.GetID()] = connectionThroughput * flux * fluxToRadianceFactor * bsdfEval;
			}
		}
	}
}

void TracePhotonsThread::RenderFunc() {
	const u_int workSize = 1024;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	RandomGenerator rndGen(1 + threadIndex);
	const Scene *scene = pgic.scene;
	const Camera *camera = scene->camera;

	// Setup the sampler
	//MetropolisSampler sampler(&rndGen, NULL, NULL, 512, .4f, .1f, &pgic.samplerSharedData);
	SobolSampler sampler(&rndGen, NULL, NULL, 0.f, &pgic.samplerSharedData);
	const u_int sampleBootSize = 11;
	const u_int sampleStepSize = 4;
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
			workCounter = pgic.globalPhotonsCounter;
		} while (!pgic.globalPhotonsCounter.compare_exchange_weak(workCounter, workCounter + workSize));

		// Check if it is time to stop
		if (workCounter >= pgic.maxPhotonTracedCount)
			break;

		const bool directDone = (pgic.globalDirectSize >= pgic.maxDirectSize);
		const bool indirectDone = (pgic.globalIndirectSize >= pgic.maxIndirectSize);
		const bool causticDone = (pgic.globalCausticSize >= pgic.maxCausticSize);

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
				SLG_LOG("Photon GI Cache photon traced: " << workCounter << "/" << pgic.maxPhotonTracedCount <<" (" << (u_int)((100.0 * workCounter) / pgic.maxPhotonTracedCount) << "%)");
				lastPrintTime = now;
			}
		}

		u_int directCreated = 0;
		u_int indirectCreated = 0;
		u_int causticCreated = 0;
		while (workToDo-- && !boost::this_thread::interruption_requested()) {
			Spectrum lightPathFlux;
			sampleResults.clear();
			lightPathFlux = Spectrum();

			const float timeSample = sampler.GetSample(10);
			const float time = camera->GenerateRayTime(timeSample);

			// Select one light source
			float lightPickPdf;
			const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->
					SampleLights(sampler.GetSample(2), &lightPickPdf);

			if (light) {
				// Initialize the light path
				float lightEmitPdfW;
				Ray nextEventRay;
				lightPathFlux = light->Emit(*scene,
					sampler.GetSample(3), sampler.GetSample(4), sampler.GetSample(5), sampler.GetSample(6), sampler.GetSample(7),
						&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW);
				nextEventRay.UpdateMinMaxWithEpsilon();
				nextEventRay.time = time;

				if (lightPathFlux.Black()) {
					sampler.NextSample(sampleResults);
					continue;
				}
				lightPathFlux /= lightEmitPdfW * lightPickPdf;
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				// Sample a point on the camera lens
				Point lensPoint;
				if (!camera->SampleLens(time, sampler.GetSample(8), sampler.GetSample(9),
						&lensPoint)) {
					sampler.NextSample(sampleResults);
					continue;
				}

				//----------------------------------------------------------------------
				// Trace the light path
				//----------------------------------------------------------------------

				bool specularPath = true;
				u_int depth = 1;
				PathVolumeInfo volInfo;
				while (depth <= pgic.maxPathDepth) {
					const u_int sampleOffset = sampleBootSize +	(depth - 1) * sampleStepSize;

					RayHit nextEventRayHit;
					BSDF bsdf;
					Spectrum connectionThroughput;
					const bool hit = scene->Intersect(nullptr, true, false, &volInfo, sampler.GetSample(sampleOffset),
							&nextEventRay, &nextEventRayHit, &bsdf,
							&connectionThroughput);

					if (hit) {
						// Something was hit

						lightPathFlux *= connectionThroughput;
						
						//--------------------------------------------------------------
						// Deposit photons only on diffuse surfaces
						//--------------------------------------------------------------

						// TODO: support for generic material
						if (pgic.IsCachedMaterial(bsdf.GetMaterialType())) {
							const Spectrum alpha = lightPathFlux * AbsDot(bsdf.hitPoint.shadeN, -nextEventRay.d);

							const Normal landingSurfaceNormal = ((Dot(bsdf.hitPoint.shadeN, -nextEventRay.d) > 0.f) ?
								1.f : -1.f) * bsdf.hitPoint.shadeN;

							bool usedPhoton = false;
							if (!directDone && (depth == 1) && (pgic.directEnabled || pgic.indirectEnabled)) {
								// It is a direct light photon
								directPhotons.push_back(Photon(bsdf.hitPoint.p, nextEventRay.d,
										alpha, landingSurfaceNormal));
								++directCreated;
								usedPhoton = true;
							} else if (!causticDone && (depth > 1) && specularPath && (pgic.causticEnabled || pgic.indirectEnabled)) {
								// It is a caustic photon
								causticPhotons.push_back(Photon(bsdf.hitPoint.p, nextEventRay.d,
										alpha, landingSurfaceNormal));
								++causticCreated;
								usedPhoton = true;
							} else if (!indirectDone && pgic.indirectEnabled) {
								// It is an indirect photon
								indirectPhotons.push_back(Photon(bsdf.hitPoint.p, nextEventRay.d,
										alpha, landingSurfaceNormal));
								++indirectCreated;
								usedPhoton = true;
							} 

							if (usedPhoton && pgic.indirectEnabled) {
								// Decide if to deposit a radiance photon
								if (rndGen.floatValue() > .1f) {
									// Flip the normal if required
									radiancePhotons.push_back(RadiancePhoton(bsdf.hitPoint.p,
											landingSurfaceNormal, Spectrum()));
								}
							}
						}

						//--------------------------------------------------------------
						// Try to connect the light path vertex with the eye
						//
						// Note: I'm connecting to the eye in order to support
						// metropolis sampler. Otherwise I could avoid this work.
						//--------------------------------------------------------------

						ConnectToEye(nextEventRay.time,
								sampler.GetSample(sampleOffset + 1), *light,
								bsdf, lensPoint, lightPathFlux, volInfo, sampleResults);

						if (depth >= pgic.maxPathDepth)
							break;

						//--------------------------------------------------------------
						// Build the next vertex path ray
						//--------------------------------------------------------------

						float bsdfPdf;
						Vector sampledDir;
						BSDFEvent event;
						float cosSampleDir;
						const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
								sampler.GetSample(sampleOffset + 2),
								sampler.GetSample(sampleOffset + 3),
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

			sampler.NextSample(sampleResults);

#ifdef WIN32
			// Work around Windows bad scheduling
			renderThread->yield();
#endif
		}

		// Update size counters
		pgic.globalDirectSize += directCreated;
		pgic.globalIndirectSize += indirectCreated;
		pgic.globalCausticSize += causticCreated;

		// Check if it is time to stop. I can do the check only here because
		// globalPhotonsTraced was already incremented
		if (directDone && indirectDone && causticDone)
			break;
	}
}
