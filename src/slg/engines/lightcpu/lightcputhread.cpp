/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "slg/engines/lightcpu/lightcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCPU RenderThread
//------------------------------------------------------------------------------

LightCPURenderThread::LightCPURenderThread(LightCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device) {
}

void LightCPURenderThread::ConnectToEye(const float u0,
		const BSDF &bsdf, const Point &lensPoint, const Spectrum &flux,
		PathVolumeInfo volInfo, vector<SampleResult> *sampleResults) {
	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector eyeDir(bsdf.hitPoint.p - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(-eyeDir, &event);

	if (!bsdfEval.Black()) {
		const float epsilon = Max(MachineEpsilon::E(lensPoint), MachineEpsilon::E(eyeDistance));
		Ray eyeRay(lensPoint, eyeDir,
				epsilon,
				eyeDistance - epsilon);

		float filmX, filmY;
		if (scene->camera->GetSamplePosition(&eyeRay, &filmX, &filmY)) {
			// I have to flip the direction of the traced ray because
			// the information inside PathVolumeInfo are about the path from
			// the light toward the camera (i.e. ray.o would be in the wrong
			// place).
			Ray traceRay(bsdf.hitPoint.p, -eyeDir, eyeRay.mint, eyeRay.maxt);
			RayHit traceRayHit;
			BSDF bsdfConn;

			Spectrum connectionThroughput, connectionEmission;
			if (!scene->Intersect(device, true, &volInfo, u0, &traceRay, &traceRayHit, &bsdfConn,
					&connectionThroughput, NULL, NULL, &connectionEmission)) {
				// Nothing was hit, the light path vertex is visible
				const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

				const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
					scene->camera->GetPixelArea());
				const float fluxToRadianceFactor = cameraPdfW / (eyeDistance * eyeDistance);

				const Spectrum radiance = connectionThroughput * flux * fluxToRadianceFactor * bsdfEval;
				SampleResult::AddSampleResult(*sampleResults, filmX, filmY, radiance + connectionEmission);
			}
		}
	}
}

void LightCPURenderThread::TraceEyePath(const float time,
		Sampler *sampler, PathVolumeInfo volInfo,
		vector<SampleResult> *sampleResults) {
	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;
	Film *film = threadFilm;
	const u_int filmWidth = film->GetWidth();
	const u_int filmHeight = film->GetHeight();

	Ray eyeRay;
	const float filmX = min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
	const float filmY = min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
	camera->GenerateRay(filmX, filmY, &eyeRay,
		sampler->GetSample(10), sampler->GetSample(11), time);

	Spectrum radiance, eyePathThroughput(1.f);
	int depth = 1;
	while (depth <= engine->maxPathDepth) {
		const u_int sampleOffset = sampleBootSize + (depth - 1) * sampleEyeStepSize;

		RayHit eyeRayHit;
		BSDF bsdf;
		Spectrum connectionThroughput, connectionEmission;
		const bool somethingWasHit = scene->Intersect(device, false, &volInfo,
				sampler->GetSample(sampleOffset), &eyeRay, &eyeRayHit, &bsdf,
				&connectionThroughput, NULL, NULL, &connectionEmission);
		radiance += eyePathThroughput * connectionEmission;

		if (!somethingWasHit) {
			// Nothing was hit, check infinite lights (including sun)
			const Spectrum throughput = eyePathThroughput * connectionThroughput;
			BOOST_FOREACH(EnvLightSource *el, scene->lightDefs.GetEnvLightSources())
				radiance +=  throughput * el->GetRadiance(*scene, -eyeRay.d);
			break;
		} else {
			// Something was hit, check if it is a light source
			if (bsdf.IsLightSource())
				radiance = eyePathThroughput * connectionThroughput * bsdf.GetEmittedRadiance();
			else {
				// Check if it is a specular bounce

				float bsdfPdf;
				Vector sampledDir;
				BSDFEvent event;
				float cosSampleDir;
				const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 1),
						sampler->GetSample(sampleOffset + 2),
						&bsdfPdf, &cosSampleDir, &event);
				if (bsdfSample.Black() || ((depth == 1) && !(event & SPECULAR)))
					break;

				// If depth = 1 and it is a specular bounce, I continue to trace the
				// eye path looking for a light source

				eyePathThroughput *= connectionThroughput * bsdfSample;
				assert (!eyePathThroughput.IsNaN() && !eyePathThroughput.IsInf());

				eyeRay.Update(bsdf.hitPoint.p, sampledDir);
			}

			++depth;
		}
	}

	// Add a sample even if it is black in order to avoid aliasing problems
	// between sampled pixel and not sampled one (in PER_PIXEL_NORMALIZED buffer)
	SampleResult::AddSampleResult(*sampleResults, filmX, filmY, radiance, (depth == 1) ? 1.f : 0.f);
}

void LightCPURenderThread::RenderFunc() {
	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;
	Film *film = threadFilm;

	// Setup the sampler

	// metropolisSharedTotalLuminance and metropolisSharedSampleCount are
	// initialized inside MetropolisSampler::RequestSamples()
	double metropolisSharedTotalLuminance, metropolisSharedSampleCount;
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film,
			&metropolisSharedTotalLuminance, &metropolisSharedSampleCount);
	const u_int sampleSize = 
		sampleBootSize + // To generate the initial setup
		engine->maxPathDepth * sampleEyeStepSize + // For each eye vertex
		engine->maxPathDepth * sampleLightStepSize; // For each light vertex
	sampler->RequestSamples(sampleSize);

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults;
	const u_int haltDebug = engine->renderConfig->GetProperty("batch.haltdebug").
		Get<u_int>() * film->GetWidth() * film->GetHeight();

	for(u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		sampleResults.clear();

		const float time = sampler->GetSample(12);

		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(sampler->GetSample(2), &lightPickPdf);

		// Initialize the light path
		float lightEmitPdfW;
		Ray nextEventRay;
		Spectrum lightPathFlux = light->Emit(*scene,
			sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5), sampler->GetSample(6), sampler->GetSample(7),
			&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW);
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
		TraceEyePath(time, sampler, eyeVolInfo, &sampleResults);

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
			Spectrum connectionThroughput, connectEmission;
			const bool hit = scene->Intersect(device, true, &volInfo, sampler->GetSample(sampleOffset),
					&nextEventRay, &nextEventRayHit, &bsdf,
					&connectionThroughput, NULL, NULL, &connectEmission);

			if (hit) {
				// Something was hit

				lightPathFlux *= connectionThroughput;
				lightPathFlux += connectEmission;

				//--------------------------------------------------------------
				// Try to connect the light path vertex with the eye
				//--------------------------------------------------------------

				ConnectToEye(sampler->GetSample(sampleOffset + 1),
						bsdf, lensPoint, lightPathFlux, volInfo, &sampleResults);

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

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif

		if ((haltDebug > 0u) && (steps >= haltDebug))
			break;
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread halted");
}
