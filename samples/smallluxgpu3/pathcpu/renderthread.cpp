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

//------------------------------------------------------------------------------
// PathCPU RenderThread
//------------------------------------------------------------------------------

static Spectrum DirectLightSampling(const Scene *scene, RandomGenerator *rndGen,
		const bool lastSpecular, const Spectrum &pathThrouput,
		const BSDF &bsdf, const Vector &eyeDir,
		const float rrImportanceCap) {
	if (!bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW;
		Spectrum lightRadiance = light->Illuminate(scene, bsdf.hitPoint,
				rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
				&lightRayDir, &distance, &directPdfW);

		if (!lightRadiance.Black()) {
			BSDFEvent event;
			float bsdfPdfW;
			Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, eyeDir, &event, &bsdfPdfW);

			if (!bsdfEval.Black()) {
				Ray shadowRay(bsdf.hitPoint, lightRayDir,
						MachineEpsilon::E(bsdf.hitPoint),
						distance - MachineEpsilon::E(distance));
				RayHit shadowRayHit;
				if (!scene->dataSet->Intersect(&shadowRay, &shadowRayHit)) {
					const float cosThetaToLight = AbsDot(lightRayDir, bsdf.shadeN);
					const float factor = cosThetaToLight / (lightPickPdf * directPdfW);

					float weight;
					if (!lastSpecular) {
						weight = PowerHeuristic(directPdfW * lightPickPdf, bsdfPdfW);

						if (rrImportanceCap > 0.f) {
							// Russian Roulette
							const float prob = Max(bsdfEval.Filter(), rrImportanceCap);
							if (prob > rndGen->floatValue())
								bsdfEval *= prob;
						}
					} else
						weight = 1.f;

					return pathThrouput * lightRadiance * (weight * factor * bsdfEval);
				}
				// TODO: pass through support
			}
		}
	}

	return Spectrum();
}

static Spectrum DirectHitLightSampling(const Scene *scene,
		const bool lastspecular, const Spectrum &pathThrouput,
		const Vector &eyeDir, const float distance,
		const BSDF &bsdf, const float lastPdfW) {
	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(scene,
		eyeDir, &directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!lastspecular) {
			const float lightPickProb = scene->PickLightPdf();
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(eyeDir, bsdf.shadeN));

			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		return  pathThrouput * weight * emittedRadiance;
	} else
		return Spectrum();
}

void PathCPURenderEngine::RenderThreadFuncImpl(CPURenderThread *renderThread) {
	//SLG_LOG("[PathCPURenderEngine::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathCPURenderEngine *renderEngine = (PathCPURenderEngine *)renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;
	PerspectiveCamera *camera = renderEngine->renderConfig->scene->camera;
	Film * film = renderThread->threadFilmPPN;
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
		float lastPdfW;
		Spectrum radiance(0.f);
		Spectrum pathThrouput(1.f, 1.f, 1.f);
		while (depth <= renderEngine->maxPathDepth) {
			RayHit eyeRayHit;
			if (scene->dataSet->Intersect(&eyeRay, &eyeRayHit)) {
				// Something was hit
				BSDF bsdf(false, *scene, eyeRay, eyeRayHit, rndGen->floatValue());

				// Check if it is a light source
				if (bsdf.IsLightSource()) {
					radiance += DirectHitLightSampling(scene, lastSpecular,
							pathThrouput, -eyeRay.d, eyeRayHit.t, bsdf, lastPdfW);

					// SLG light sources are like black bodies
					break;
				}

				// Check if it is pass-through point
				if (bsdf.IsPassThrough()) {
					// It is a pass-through material, continue to trace the ray
					eyeRay.mint = eyeRayHit.t + MachineEpsilon::E(eyeRayHit.t);
					eyeRay.maxt = std::numeric_limits<float>::infinity();
					continue;
				}

				//----------------------------------------------------------
				// Direct light sampling
				//----------------------------------------------------------

				radiance += DirectLightSampling(scene, rndGen, lastSpecular,
						pathThrouput, bsdf, -eyeRay.d,
						(depth >= renderEngine->rrDepth) ? renderEngine->rrImportanceCap : -1.f);

				//----------------------------------------------------------
				// Build the next vertex path ray
				//----------------------------------------------------------

				Vector sampledDir;
				BSDFEvent event;
				const Spectrum bsdfSample = bsdf.Sample(-eyeRay.d, &sampledDir,
						rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
						&lastPdfW, &event);
				if ((lastPdfW <= 0.f) || bsdfSample.Black())
					break;

				if ((depth >= renderEngine->rrDepth) && !lastSpecular) {
					// Russian Roulette
					const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
					if (prob > rndGen->floatValue())
						lastPdfW *= prob;
					else
						break;
				}

				lastSpecular = ((event & SPECULAR) != 0);
				pathThrouput *= AbsDot(sampledDir, bsdf.shadeN) * bsdfSample / lastPdfW;
				assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

				eyeRay = Ray(bsdf.hitPoint, sampledDir);
				++depth;
			} else
				break;
		}

		assert (!radiance.IsNaN() && !radiance.IsInf());

		film->AddSampleCount(1.f);
		film->SplatFiltered(screenX, screenY, radiance);
	}

	//SLG_LOG("[PathCPURenderEngine::" << renderThread->threadIndex << "] Rendering thread halted");
}
