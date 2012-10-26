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

//------------------------------------------------------------------------------
// PathCPU RenderThread
//------------------------------------------------------------------------------

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

		int depth = 0;
		bool lastSpecular = true;
		Spectrum radiance(0.f);
		Spectrum pathThrouput(1.f, 1.f, 1.f);
		while (depth <= renderEngine->maxPathDepth) {
			RayHit eyeRayHit;
			if (scene->dataSet->Intersect(&eyeRay, &eyeRayHit)) {
					// Something was hit
					BSDF bsdf(false, *scene, eyeRay, eyeRayHit, rndGen->floatValue());

					// Check if it is a light source
					if (bsdf.IsLightSource()) {
						// Check if it is a an area light
						float directPdfA;
						const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(scene,
							-eyeRay.d, &directPdfA);

						float weight = 1.f;
						if(depth > 0 && !lastSpecular)
							weight = PdfAtoW(directPdfA, eyeRayHit.t,
									Dot(-eyeRay.d, bsdf.shadeN));
						
						radiance += pathThrouput * weight * emittedRadiance;

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

					//--------------------------------------------------------------
					// Build the next vertex path ray
					//--------------------------------------------------------------

					float bsdfPdf;
					Vector sampledDir;
					BSDFEvent event;
					const Spectrum bsdfSample = bsdf.Sample(-eyeRay.d, &sampledDir,
							rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
							&bsdfPdf, &event);
					if ((bsdfPdf <= 0.f) || bsdfSample.Black())
						break;

					/*if (depth >= renderEngine->rrDepth) {
						// Russian Roulette
						const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
						if (prob >= rndGen->floatValue())
							bsdfPdf *= prob;
						else
							break;
					}*/

					lastSpecular = ((event & SPECULAR) != 0);
					pathThrouput *= Dot(sampledDir, bsdf.shadeN) * bsdfSample / bsdfPdf;
					assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

					eyeRay = Ray(bsdf.hitPoint, sampledDir);
					++depth;
			} else {
				break;
			}
		}

		film->AddSampleCount(1.f);
		film->SplatFiltered(screenX, screenY, radiance);
	}

	//SLG_LOG("[PathCPURenderEngine::" << renderThread->threadIndex << "] Rendering thread halted");
}
