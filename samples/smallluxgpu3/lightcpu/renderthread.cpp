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
#include "lightcpu/lightcpu.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sdl/bsdf.h"

//------------------------------------------------------------------------------
// LightCPU RenderThread
//------------------------------------------------------------------------------

static void ConnectToEye(const Scene *scene, Film *film, const float u0,
		Vector eyeDir, const float eyeDistance,
		const Point &hitPoint, const Normal &geometryN, const Spectrum bsdfEval,
		const Spectrum &flux) {
	if (!bsdfEval.Black()) {
		Ray eyeRay(hitPoint, eyeDir);
		eyeRay.maxt = eyeDistance;

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(eyeRay.o, -eyeRay.d, eyeDistance, &scrX, &scrY)) {
			for (;;) {
				RayHit eyeRayHit;
				if (!scene->dataSet->Intersect(&eyeRay, &eyeRayHit)) {
					// Nothing was hit, the light path vertex is visible

					// cosToCamera is already included when connecting a vertex on the light
					const float cosToCamera = Dot(geometryN, eyeDir);
					const float cosAtCamera = Dot(scene->camera->GetDir(), -eyeDir);

					const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
						scene->camera->GetPixelArea());
					const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
					const float fluxToRadianceFactor = cameraPdfA;

					film->SplatFiltered(scrX, scrY, flux * fluxToRadianceFactor * bsdfEval);
					break;
				} else {
					// Check if it is a pass through point
					BSDF bsdf(true, *scene, eyeRay, eyeRayHit, u0);

					// Check if it is pass-through point
					if (bsdf.IsPassThrough()) {
						// It is a pass-through material, continue to trace the ray
						eyeRay.mint = eyeRayHit.t + MachineEpsilon::E(eyeRayHit.t);
						eyeRay.maxt = std::numeric_limits<float>::infinity();
						continue;
					}
					break;
				}
			}
		}
	}
}

static void ConnectToEye(const Scene *scene, Film *film, const float u0, const BSDF &bsdf,
		const Vector &lightDir, const Spectrum flux) {
	Vector eyeDir(scene->camera->orig - bsdf.hitPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	BSDFEvent event;
	Spectrum bsdfEval = bsdf.Evaluate(lightDir, eyeDir, &event);

	ConnectToEye(scene, film, u0, eyeDir, eyeDistance, bsdf.hitPoint, bsdf.geometryN, bsdfEval, flux);
}

void LightCPURenderEngine::RenderThreadFuncImpl(CPURenderThread *renderThread) {
	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *renderEngine = (LightCPURenderEngine *)renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	while (!boost::this_thread::interruption_requested()) {
		renderThread->threadFilmPSN->AddSampleCount(1);

		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lightPickPdf);

		// Initialize the light path
		float lightEmitPdf;
		Ray nextEventRay;
		Normal lightN;
		Spectrum lightPathFlux = light->Emit(scene,
			rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
			&nextEventRay.o, &nextEventRay.d, &lightN, &lightEmitPdf);
		if ((lightEmitPdf == 0.f) || lightPathFlux.Black())
			continue;
		lightPathFlux /= lightEmitPdf * lightPickPdf;
		assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

		//----------------------------------------------------------------------
		// Try to connect the light vertex directly with the eye
		//----------------------------------------------------------------------

		Vector eyeDir(scene->camera->orig - nextEventRay.o);
		if (Dot(eyeDir, lightN) > 0.f) {
			const float eyeDistance = eyeDir.Length();
			eyeDir /= eyeDistance;
			ConnectToEye(scene, renderThread->threadFilmPSN, rndGen->floatValue(), eyeDir, eyeDistance,
					nextEventRay.o, lightN, lightPathFlux, Spectrum(1.f, 1.f, 1.f));
		}

		//----------------------------------------------------------------------
		// Trace the light path
		//----------------------------------------------------------------------
		int depth = 1;
		while (depth <= renderEngine->maxPathDepth) {
			RayHit nextEventRayHit;
			if (scene->dataSet->Intersect(&nextEventRay, &nextEventRayHit)) {
				// Something was hit
				BSDF bsdf(true, *scene, nextEventRay, nextEventRayHit, rndGen->floatValue());

				// Check if it is a light source
				if (bsdf.IsLightSource()) {
					// SLG light sources are like black bodies
					break;
				}

				// Check if it is pass-through point
				if (bsdf.IsPassThrough()) {
					// It is a pass-through material, continue to trace the ray
					nextEventRay.mint = nextEventRayHit.t + MachineEpsilon::E(nextEventRayHit.t);
					nextEventRay.maxt = std::numeric_limits<float>::infinity();
					continue;
				}

				//--------------------------------------------------------------
				// Try to connect the light path vertex with the eye
				//--------------------------------------------------------------

				ConnectToEye(scene, renderThread->threadFilmPSN, rndGen->floatValue(),
						bsdf, -nextEventRay.d, lightPathFlux);

				if (depth >= renderEngine->maxPathDepth)
					break;
				
				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				float bsdfPdf;
				Vector sampledDir;
				BSDFEvent event;
				const Spectrum bsdfSample = bsdf.Sample(-nextEventRay.d, &sampledDir,
						rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
						&bsdfPdf, &event);
				if ((bsdfPdf <= 0.f) || bsdfSample.Black())
					break;

				if (depth >= renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
					if (prob >= rndGen->floatValue())
						bsdfPdf *= prob;
					else
						break;
				}

				lightPathFlux *= AbsDot(bsdf.shadeN, sampledDir) * bsdfSample / bsdfPdf;
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				nextEventRay = Ray(bsdf.hitPoint, sampledDir);
				++depth;
			} else {
				// Ray lost in space...
				break;
			}
		}
	}

	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
