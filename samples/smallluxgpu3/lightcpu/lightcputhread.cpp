/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "lightcpu/lightcpu.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

//------------------------------------------------------------------------------
// LightCPU RenderThread
//------------------------------------------------------------------------------

LightCPURenderThread::LightCPURenderThread(LightCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPURenderThread(engine, index, device, true, true) {
}

void LightCPURenderThread::ConnectToEye(const float u0,
		const BSDF &bsdf, const Point &lensPoint, const Spectrum &flux,
		vector<SampleResult> &sampleResults) {
	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector eyeDir(bsdf.hitPoint - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(-eyeDir, &event);

	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir);
		eyeRay.maxt = eyeDistance - MachineEpsilon::E(eyeDistance);

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(lensPoint, eyeDir, eyeDistance, &scrX, &scrY)) {
			RayHit eyeRayHit;
			BSDF bsdfConn;
			Spectrum connectionThroughput;
			if (!scene->Intersect(device, true, true, u0, &eyeRay, &eyeRayHit, &bsdfConn, &connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				const float cosToCamera = Dot(bsdf.shadeN, -eyeDir);
				const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

				const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
					scene->camera->GetPixelArea());
				const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
				const float fluxToRadianceFactor = cameraPdfA;

				const Spectrum radiance = connectionThroughput * flux * fluxToRadianceFactor * bsdfEval;

				AddSampleResult(sampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
						radiance, 1.f);
			}
		}
	}
}

void LightCPURenderThread::TraceEyePath(Sampler *sampler, vector<SampleResult> *sampleResults) {
	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();

	// Sample offsets
	const unsigned int sampleBootSize = 11;
	const unsigned int sampleEyeStepSize = 4;

	Ray eyeRay;
	const float screenX = min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
	const float screenY = min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
	camera->GenerateRay(screenX, screenY, &eyeRay,
		sampler->GetSample(9), sampler->GetSample(10));

	Spectrum radiance, eyePathThroughput(1.f, 1.f, 1.f);
	int depth = 1;
	while (depth <= engine->maxPathDepth) {
		const unsigned int sampleOffset = sampleBootSize + (depth - 1) * sampleEyeStepSize;

		RayHit eyeRayHit;
		BSDF bsdf;
		Spectrum connectionThroughput;
		const bool somethingWasHit = scene->Intersect(device, false, true,
				sampler->GetSample(sampleOffset), &eyeRay, &eyeRayHit, &bsdf, &connectionThroughput);
		if (!somethingWasHit) {
			// Nothing was hit, check infinite lights (including sun)
			radiance = eyePathThroughput * connectionThroughput * scene->GetEnvLightsRadiance(-eyeRay.d, Point());
			break;
		} else {
			// Something was hit, check if it is a light source
			if (bsdf.IsLightSource())
				radiance = eyePathThroughput * connectionThroughput * bsdf.GetEmittedRadiance(scene);
			else {
				// Check if it is a specular bounce

				float bsdfPdf;
				Vector sampledDir;
				BSDFEvent event;
				float cosSampleDir;
				const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 1),
						sampler->GetSample(sampleOffset + 2),
						sampler->GetSample(sampleOffset + 3),
						&bsdfPdf, &cosSampleDir, &event);
				if (bsdfSample.Black() || ((depth == 1) && !(event & SPECULAR)))
					break;

				// If depth = 1 and it is a specular bounce, I continue to trace the
				// eye path looking for a light source

				eyePathThroughput *= connectionThroughput * bsdfSample * (cosSampleDir / bsdfPdf);
				assert (!eyePathThroughput.IsNaN() && !eyePathThroughput.IsInf());

				eyeRay = Ray(bsdf.hitPoint, sampledDir);
			}

			++depth;
		}
	}

	// Add a sample even if it is black in order to avoid aliasing problems
	// between sampled pixel and not sampled one (in PER_PIXEL_NORMALIZED buffer)
	AddSampleResult(*sampleResults, PER_PIXEL_NORMALIZED,
			screenX, screenY, radiance, (depth == 1) ? 1.f : 0.f);
}

void LightCPURenderThread::RenderFunc() {
	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = threadFilm;

	// Setup the sampler
	double metropolisSharedTotalLuminance, metropolisSharedSampleCount;
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film,
			&metropolisSharedTotalLuminance, &metropolisSharedSampleCount);
	const unsigned int sampleBootSize = 11;
	const unsigned int sampleEyeStepSize = 4;
	const unsigned int sampleLightStepSize = 6;
	const unsigned int sampleSize = 
		sampleBootSize + // To generate the initial setup
		engine->maxPathDepth * sampleEyeStepSize + // For each eye vertex
		engine->maxPathDepth * sampleLightStepSize; // For each light vertex
	sampler->RequestSamples(sampleSize);

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults;
	while (!boost::this_thread::interruption_requested()) {
		sampleResults.clear();

		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(sampler->GetSample(2), &lightPickPdf);

		// Initialize the light path
		float lightEmitPdfW;
		Ray nextEventRay;
		Spectrum lightPathFlux = light->Emit(scene,
			sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5), sampler->GetSample(6),
			&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW);
		if (lightPathFlux.Black()) {
			sampler->NextSample(sampleResults);
			continue;
		}
		lightPathFlux /= lightEmitPdfW * lightPickPdf;
		assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera->SampleLens(sampler->GetSample(7), sampler->GetSample(8),
				&lensPoint)) {
			sampler->NextSample(sampleResults);
			continue;
		}

		//----------------------------------------------------------------------
		// I don't try to connect the light vertex directly with the eye
		// because InfiniteLight::Emit() returns a point on the scene bounding
		// sphere. Instead, I trace a ray from the camera like in BiDir.
		// This is also a good why to test the Film Per-Pixel-Normalization and
		// the Per-Screen-Normalization Buffers used by BiDir.
		//----------------------------------------------------------------------

		TraceEyePath(sampler, &sampleResults);

		//----------------------------------------------------------------------
		// Trace the light path
		//----------------------------------------------------------------------

		int depth = 1;
		while (depth <= engine->maxPathDepth) {
			const unsigned int sampleOffset = sampleBootSize + sampleEyeStepSize * engine->maxPathDepth +
				(depth - 1) * sampleLightStepSize;

			RayHit nextEventRayHit;
			BSDF bsdf;
			Spectrum connectionThroughput;
			if (scene->Intersect(device, true, true, sampler->GetSample(sampleOffset),
					&nextEventRay, &nextEventRayHit, &bsdf, &connectionThroughput)) {
				// Something was hit

				lightPathFlux *= connectionThroughput;

				//--------------------------------------------------------------
				// Try to connect the light path vertex with the eye
				//--------------------------------------------------------------

				ConnectToEye(sampler->GetSample(sampleOffset + 1),
						bsdf, lensPoint, lightPathFlux, sampleResults);

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
						sampler->GetSample(sampleOffset + 4),
						&bsdfPdf, &cosSampleDir, &event);
				if (bsdfSample.Black())
					break;

				if (depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfSample.Filter(), engine->rrImportanceCap);
					if (sampler->GetSample(sampleOffset + 5) < prob)
						bsdfPdf *= prob;
					else
						break;
				}

				lightPathFlux *= bsdfSample * (cosSampleDir / bsdfPdf);
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				nextEventRay = Ray(bsdf.hitPoint, sampledDir);
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
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread halted");
}

}
