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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <string>
#include <sstream>
#include <stdexcept>

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

#include <boost/thread/mutex.hpp>

#include "smalllux.h"

#include "lightcpu/lightcpu.h"
#include "renderconfig.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/pixel/samplebuffer.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sdl/bsdf.h"

//------------------------------------------------------------------------------
// LightCPURenderThread
//------------------------------------------------------------------------------

LightCPURenderThread::LightCPURenderThread(const unsigned int index, const unsigned int seedBase, LightCPURenderEngine *re) {
	threadIndex = index;
	seed = seedBase;
	renderEngine = re;
	threadFilm = NULL;
}

LightCPURenderThread::~LightCPURenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete threadFilm;
}

void LightCPURenderThread::Start() {
	started = true;

	StartRenderThread();
}

void LightCPURenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void LightCPURenderThread::Stop() {
	StopRenderThread();

	started = false;
}

void LightCPURenderThread::StartRenderThread() {
	delete threadFilm;
	threadFilm = new NativeFilm(renderEngine->film->GetWidth(), renderEngine->film->GetHeight(), true);
	threadFilm->Init(renderEngine->film->GetWidth(), renderEngine->film->GetHeight());

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(LightCPURenderThread::RenderThreadImpl, this));
}

void LightCPURenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void LightCPURenderThread::BeginEdit() {
	StopRenderThread();
}

void LightCPURenderThread::EndEdit(const EditActionList &editActions) {
	StartRenderThread();
}

void LightCPURenderThread::RenderThreadImpl(LightCPURenderThread *renderThread) {
	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *renderEngine = renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	while (!boost::this_thread::interruption_requested()) {
		renderThread->threadFilm->AddSampleCount(1);

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

		// Trace the light path
		int depth = 0;
		BSDF bsdf;
		do {
			//--------------------------------------------------------------
			// Try to connect the light path vertex with the eye
			//--------------------------------------------------------------

			Vector eyeDir(scene->camera->orig - nextEventRay.o);
			const float cameraDistance = eyeDir.Length();
			eyeDir /= cameraDistance;

			// Check if the current vertex is the first one
			Spectrum bsdfEval;
			Normal currentVertexN;
			if (bsdf.IsEmpty()) {
				currentVertexN = lightN;
				if (Dot(eyeDir, lightN) > 0.f)
					bsdfEval = Spectrum(1.f, 1.f, 1.f);
				else
					bsdfEval = Spectrum(0.f);
			} else {
				currentVertexN = bsdf.shadeN;
				bsdfEval = bsdf.Evaluate(-nextEventRay.d, eyeDir);
			}

			if (!bsdfEval.Black()) {
				Ray eyeRay(nextEventRay.o, eyeDir);
				eyeRay.maxt = cameraDistance;

				float scrX, scrY;
				if (scene->camera->GetSamplePosition(eyeRay.o, -eyeRay.d, cameraDistance, &scrX, &scrY)) {
					RayHit eyeRayHit;
					if (!scene->dataSet->Intersect(&eyeRay, &eyeRayHit)) {
						// Nothing was hit, the light path vertex is visible
						const float cosToCamera = Dot(currentVertexN, eyeDir);
						const float cosAtCamera = Dot(scene->camera->GetDir(), -eyeDir);

						const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
							scene->camera->GetPixelArea());
						const float cameraPdfA = PdfWtoA(cameraPdfW, cameraDistance, cosToCamera);
						const float fluxToRadianceFactor = cameraPdfA;

						renderThread->threadFilm->SplatFiltered(scrX, scrY, lightPathFlux * fluxToRadianceFactor * bsdfEval);
					}
				}
				// TODO: NULL material and alpha channel support
			}

			RayHit nextEventRayHit;
			if (scene->dataSet->Intersect(&nextEventRay, &nextEventRayHit)) {
				// Something was hit
				bsdf.Init(true, *scene, nextEventRay, nextEventRayHit, rndGen->floatValue());

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
					++depth;
					continue;
				}

				///--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				float bsdfPdf;
				Vector wo;
				const Spectrum bsdfSample = bsdf.Sample(-nextEventRay.d, &wo,
						rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
						&bsdfPdf);
				if ((bsdfPdf <= 0.f) || bsdfSample.Black())
					break;

				lightPathFlux *= Dot(bsdf.shadeN, wo) * bsdfSample / bsdfPdf;
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				/*if (depth > renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = Max(lightPathRadiance.Filter(), renderEngine->rrImportanceCap);
					if (prob >= rndGen->floatValue())
						lightPathRadiance /= prob;
					else
						break;
				}*/

				nextEventRay = Ray(bsdf.hitPoint, wo);

				++depth;
			} else {
				// Ray lost in space...
				break;
			}
		} while (depth < renderEngine->maxPathDepth);
	}

	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
