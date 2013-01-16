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

#include "pathcpu/pathcpu.h"

#include "luxrays/utils/core/mc.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

// TODO: use only brute force to sample infinitelight

//------------------------------------------------------------------------------
// PathCPU RenderThread
//------------------------------------------------------------------------------

PathCPURenderThread::PathCPURenderThread(PathCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPURenderThread(engine, index, device, true, false) {
}

void PathCPURenderThread::DirectLightSampling(
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		const int depth, Spectrum *radiance) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	
	if (!bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(u0, &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW;
		Spectrum lightRadiance = light->Illuminate(scene, bsdf.hitPoint,
				u1, u2, u3, &lightRayDir, &distance, &directPdfW);

		if (!lightRadiance.Black()) {
			BSDFEvent event;
			float bsdfPdfW;
			Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW);

			if (!bsdfEval.Black()) {
				Ray shadowRay(bsdf.hitPoint, lightRayDir,
						MachineEpsilon::E(bsdf.hitPoint),
						distance - MachineEpsilon::E(distance));
				RayHit shadowRayHit;
				BSDF shadowBsdf;
				Spectrum connectionThroughput;
				// Check if the light source is visible
				if (!scene->Intersect(device, false, u4, &shadowRay,
						&shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					const float cosThetaToLight = AbsDot(lightRayDir, bsdf.shadeN);
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;
					const float factor = cosThetaToLight / directLightSamplingPdfW;

					if (depth >= engine->rrDepth) {
						// Russian Roulette
						bsdfPdfW *= Max(bsdfEval.Filter(), engine->rrImportanceCap);
					}

					// MIS between direct light sampling and BSDF sampling
					const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

					*radiance += (weight * factor) * pathThrouput * connectionThroughput * lightRadiance * bsdfEval;
				}
			}
		}
	}
}

void PathCPURenderThread::DirectHitFiniteLight(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const float distance, const BSDF &bsdf, const float lastPdfW,
		Spectrum *radiance) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(scene, &directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!lastSpecular) {
			const float lightPickProb = scene->PickLightPdf();
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.fixedDir, bsdf.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		*radiance +=  pathThrouput * weight * emittedRadiance;
	}
}

void PathCPURenderThread::DirectHitInfiniteLight(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const Vector &eyeDir, const float lastPdfW, Spectrum *radiance) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Infinite light
	float directPdfW;
	if (scene->envLight) {
		const Spectrum envRadiance = scene->envLight->GetRadiance(scene, -eyeDir, &directPdfW);
		if (!envRadiance.Black()) {
			if(!lastSpecular) {
				// MIS between BSDF sampling and direct light sampling
				*radiance += pathThrouput * PowerHeuristic(lastPdfW, directPdfW) * envRadiance;
			} else
				*radiance += pathThrouput * envRadiance;
		}
	}

	// Sun light
	if (scene->sunLight) {
		const Spectrum sunRadiance = scene->sunLight->GetRadiance(scene, -eyeDir, &directPdfW);
		if (!sunRadiance.Black()) {
			if(!lastSpecular) {
				// MIS between BSDF sampling and direct light sampling
				*radiance += pathThrouput * PowerHeuristic(lastPdfW, directPdfW) * sunRadiance;
			} else
				*radiance += pathThrouput * sunRadiance;
		}
	}
}

void PathCPURenderThread::RenderFunc() {
	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film * film = threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();

	// Setup the sampler
	double metropolisSharedTotalLuminance, metropolisSharedSampleCount;
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film,
			&metropolisSharedTotalLuminance, &metropolisSharedSampleCount);
	const unsigned int sampleBootSize = 4;
	const unsigned int sampleStepSize = 9;
	const unsigned int sampleSize = 
		sampleBootSize + // To generate eye ray
		engine->maxPathDepth * sampleStepSize; // For each path vertex
	sampler->RequestSamples(sampleSize);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults(1);
	sampleResults[0].type = PER_PIXEL_NORMALIZED;
	while (!boost::this_thread::interruption_requested()) {
		float alpha = 1.f;

		Ray eyeRay;
		const float screenX = min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
		const float screenY = min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
		camera->GenerateRay(screenX, screenY, &eyeRay,
			sampler->GetSample(2), sampler->GetSample(3));

		int depth = 1;
		bool lastSpecular = true;
		float lastPdfW = 1.f;
		Spectrum radiance;
		Spectrum pathThrouput(1.f, 1.f, 1.f);
		BSDF bsdf;
		while (depth <= engine->maxPathDepth) {
			const unsigned int sampleOffset = sampleBootSize + (depth - 1) * sampleStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			if (!scene->Intersect(device, false, sampler->GetSample(sampleOffset),
					&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput)) {
				// Nothing was hit, look for infinitelight
				DirectHitInfiniteLight(lastSpecular, pathThrouput * connectionThroughput, eyeRay.d,
						lastPdfW, &radiance);

				if (depth == 1)
					alpha = 0.f;
				break;
			}
			pathThrouput *= connectionThroughput;

			// Something was hit

			// Check if it is a light source
			if (bsdf.IsLightSource()) {
				DirectHitFiniteLight(lastSpecular, pathThrouput,
						eyeRayHit.t, bsdf, lastPdfW, &radiance);
			}

			// Note: pass-through check is done inside SceneIntersect()

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			DirectLightSampling(sampler->GetSample(sampleOffset + 1),
					sampler->GetSample(sampleOffset + 2),
					sampler->GetSample(sampleOffset + 3),
					sampler->GetSample(sampleOffset + 4),
					sampler->GetSample(sampleOffset + 5),
					pathThrouput, bsdf, depth, &radiance);

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			Vector sampledDir;
			BSDFEvent event;
			float cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
					sampler->GetSample(sampleOffset + 6),
					sampler->GetSample(sampleOffset + 7),
					&lastPdfW, &cosSampledDir, &event);
			if (bsdfSample.Black())
				break;

			lastSpecular = ((event & SPECULAR) != 0);

			if ((depth >= engine->rrDepth) && !lastSpecular) {
				// Russian Roulette
				const float prob = Max(bsdfSample.Filter(), engine->rrImportanceCap);
				if (sampler->GetSample(sampleOffset + 8) < prob)
					lastPdfW *= prob;
				else
					break;
			}

			pathThrouput *= bsdfSample * (cosSampledDir / lastPdfW);
			assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

			eyeRay = Ray(bsdf.hitPoint, sampledDir);
			++depth;
		}

		assert (!radiance.IsNaN() && !radiance.IsInf());

		sampleResults[0].screenX = screenX;
		sampleResults[0].screenY = screenY;
		sampleResults[0].radiance = radiance;
		sampleResults[0].alpha = alpha;
		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}

}
