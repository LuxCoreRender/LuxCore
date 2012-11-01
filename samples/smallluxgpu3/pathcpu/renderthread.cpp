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

#include "renderconfig.h"
#include "pathcpu/pathcpu.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sdl/bsdf.h"
#include "luxrays/utils/core/mc.h"

// TODO: use only brute force to sample infinitelight

//------------------------------------------------------------------------------
// PathCPU RenderThread
//------------------------------------------------------------------------------

void PathCPURenderEngine::DirectLightSampling(
		const float u0, const float u1, const float u2, const float u3,
		const float u4, const float u5,
		const Spectrum &pathThrouput, const BSDF &bsdf, const Vector &eyeDir,
		const int depth, Spectrum *radiance) {
	Scene *scene = renderConfig->scene;
	
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
			Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, eyeDir, &event, &bsdfPdfW);

			if (!bsdfEval.Black()) {
				Ray shadowRay(bsdf.hitPoint, lightRayDir,
						MachineEpsilon::E(bsdf.hitPoint),
						distance - MachineEpsilon::E(distance));
				RayHit shadowRayHit;
				BSDF shadowBsdf;
				Spectrum connectionThroughput;
				if (!SceneIntersect(false, false, u5, &shadowRay, &shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					const float cosThetaToLight = AbsDot(lightRayDir, bsdf.shadeN);
					const float factor = cosThetaToLight / (directPdfW * lightPickPdf);

					const float weight = PowerHeuristic(directPdfW * lightPickPdf, bsdfPdfW);

					if (depth >= rrDepth) {
						// Russian Roulette
						const float prob = Max(bsdfEval.Filter(), rrImportanceCap);
						if (prob > u4)
							bsdfEval *= prob;
					}

					*radiance += (weight * factor) * pathThrouput * connectionThroughput * lightRadiance * bsdfEval;
				}
			}
		}
	}
}

void PathCPURenderEngine::DirectHitLightSampling(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const Vector &eyeDir, const float distance,
		const BSDF &bsdf, const float lastPdfW, Spectrum *radiance) {
	Scene *scene = renderConfig->scene;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(scene,
		eyeDir, &directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!lastSpecular) {
			const float lightPickProb = scene->PickLightPdf();
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(eyeDir, bsdf.shadeN));

			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		*radiance +=  pathThrouput * weight * emittedRadiance;
	}
}

void PathCPURenderEngine::DirectHitInfiniteLight(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const Vector &eyeDir, const float lastPdfW, Spectrum *radiance) {
	float directPdfW;
	Spectrum lightRadiance = renderConfig->scene->GetEnvLightsRadiance(-eyeDir, Point(), &directPdfW);
	if (lightRadiance.Black())
		return;

	float weight;
	if(!lastSpecular)
		weight = PowerHeuristic(lastPdfW, directPdfW);
	else
		weight = 1.f;

	*radiance += pathThrouput * weight * lightRadiance;
}

void PathCPURenderEngine::RenderThreadFuncImpl(CPURenderThread *renderThread) {
	//SLG_LOG("[PathCPURenderEngine::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathCPURenderEngine *renderEngine = (PathCPURenderEngine *)renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	PerspectiveCamera *camera = renderEngine->renderConfig->scene->camera;
	Film * film = renderThread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	while (!boost::this_thread::interruption_requested()) {
		Ray eyeRay;
		const float screenX = min(rndGen->floatValue() * filmWidth, (float)(filmWidth - 1));
		const float screenY = min(rndGen->floatValue() * filmHeight, (float)(filmHeight - 1));
		camera->GenerateRay(screenX, screenY, &eyeRay,
			rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue());

		int depth = 1;
		bool lastSpecular = true;
		float lastPdfW = 1.f;
		Spectrum radiance(0.f);
		Spectrum pathThrouput(1.f, 1.f, 1.f);
		BSDF bsdf;
		while (depth <= renderEngine->maxPathDepth) {
			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			if (!renderEngine->SceneIntersect(false, true, rndGen->floatValue(), &eyeRay,
					&eyeRayHit, &bsdf, &connectionThroughput)) {
				// Nothing was hit, look for infinitelight
				renderEngine->DirectHitInfiniteLight(lastSpecular, pathThrouput * connectionThroughput, eyeRay.d,
						lastPdfW, &radiance);

				if (depth == 1)
					film->SplatFilteredAlpha(screenX, screenY, 0.f);
				break;
			}
			pathThrouput *= connectionThroughput;

			// Something was hit
			if (depth == 1)
				film->SplatFilteredAlpha(screenX, screenY, 1.f);


			// Check if it is a light source
			if (bsdf.IsLightSource()) {
				renderEngine->DirectHitLightSampling(lastSpecular, pathThrouput,
						-eyeRay.d, eyeRayHit.t, bsdf, lastPdfW, &radiance);

				// SLG light sources are like black bodies
				break;
			}

			// Note: pass-through check is done inside SceneIntersect()

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			renderEngine->DirectLightSampling(rndGen->floatValue(), rndGen->floatValue(),
					rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
					rndGen->floatValue(), pathThrouput, bsdf, -eyeRay.d,
					depth, &radiance);

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			Vector sampledDir;
			BSDFEvent event;
			float cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(-eyeRay.d, &sampledDir,
					rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
					&lastPdfW, &cosSampledDir, &event);
			if (bsdfSample.Black())
				break;

			lastSpecular = ((event & SPECULAR) != 0);

			if ((depth >= renderEngine->rrDepth) && !lastSpecular) {
				// Russian Roulette
				const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
				if (prob > rndGen->floatValue())
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

		film->AddSampleCount(PER_PIXEL_NORMALIZED, 1.f);
		film->SplatFiltered(PER_PIXEL_NORMALIZED, screenX, screenY, radiance);
	}

	//SLG_LOG("[PathCPURenderEngine::" << renderThread->threadIndex << "] Rendering thread halted");
}
