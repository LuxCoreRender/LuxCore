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

//------------------------------------------------------------------------------
// LightCPURenderThread
//------------------------------------------------------------------------------

LightCPURenderThread::LightCPURenderThread(const unsigned int index, const unsigned int seedBase, LightCPURenderEngine *re) {
	threadIndex = index;
	seed = seedBase;
	renderEngine = re;
}

LightCPURenderThread::~LightCPURenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();
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

void LightCPURenderThread::SplatSample(const float scrX, const float scrY, const Spectrum &radiance) {
	assert (!radiance.IsNaN() && !radiance.IsInf());

	sampleBuffer->SplatSample(scrX, scrY, radiance);

	if (sampleBuffer->IsFull()) {
		NativeFilm *film = renderEngine->film;

		// Film::SplatSampleBuffer() is not thread safe
		boost::unique_lock<boost::mutex> lock(*(renderEngine->filmMutex));
		film->SplatSampleBuffer(false, sampleBuffer);

		// Get a new empty buffer
		sampleBuffer = film->GetFreeSampleBuffer();
	}
}

void LightCPURenderThread::RenderThreadImpl(LightCPURenderThread *renderThread) {
	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCPURenderEngine *renderEngine = renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;
	NativeFilm *film = renderEngine->film;

	{
		// Film::GetFreeSampleBuffer() is not thread safe
		boost::unique_lock<boost::mutex> lock(*(renderEngine->filmMutex));
		renderThread->sampleBuffer = film->GetFreeSampleBuffer();
	}

	//--------------------------------------------------------------------------
	// Trace light paths
	//--------------------------------------------------------------------------

	while (!boost::this_thread::interruption_requested()) {
		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lightPickPdf);

		// Initialize the light path
		float lightEmitPdf;
		Ray nextEventRay;
		Normal shadeN;
		Spectrum lightPathFlux = light->Emit(scene,
			rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
			&nextEventRay.o, &nextEventRay.d, &shadeN, &lightEmitPdf);
		if ((lightEmitPdf == 0.f) || lightPathFlux.Black())
			continue;
		lightPathFlux /= lightEmitPdf * lightPickPdf;
		assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

		// Trace the light path
		int depth = 0;
		SurfaceMaterial *surfMat = NULL;
		do {
			//--------------------------------------------------------------
			// Try to connect the light path vertex with the eye
			//--------------------------------------------------------------

			Vector eyeDir(scene->camera->orig - nextEventRay.o);
			const float cameraDistance = eyeDir.Length();
			eyeDir /= cameraDistance;

			Spectrum bsdfEval;
			if (surfMat == NULL) {
				if (Dot(eyeDir, shadeN) > 0.f)
					bsdfEval = Spectrum(1.f, 1.f, 1.f);
				else
					bsdfEval = Spectrum(0.f);
			} else
				bsdfEval = surfMat->Evaluate(-nextEventRay.d, eyeDir, shadeN);

			if (!bsdfEval.Black()) {
				Ray eyeRay(nextEventRay.o, eyeDir);
				eyeRay.maxt = cameraDistance;

				float scrX, scrY;
				if (scene->camera->GetSamplePosition(eyeRay.o, -eyeRay.d, cameraDistance, &scrX, &scrY)) {
					RayHit eyeRayHit;
					if (!scene->dataSet->Intersect(&eyeRay, &eyeRayHit)) {
						// Nothing was hit, the light path vertex is visible
						const float cosToCamera = Dot(shadeN, eyeDir);
						const float cosAtCamera = Dot(scene->camera->GetDir(), -eyeDir);

						const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
							scene->camera->GetPixelArea());
						const float cameraPdfA = PdfWtoA(cameraPdfW, cameraDistance, cosToCamera);
						const float fluxToRadianceFactor = cameraPdfA;

						renderThread->SplatSample(scrX, scrY, lightPathFlux * fluxToRadianceFactor * bsdfEval);
					}
				}
				// TODO: NULL material and alpha channel support
			}

			RayHit nextEventRayHit;
			if (scene->dataSet->Intersect(&nextEventRay, &nextEventRayHit)) {
				// Something was hit

				const unsigned int currentTriangleIndex = nextEventRayHit.index;
				const unsigned int currentMeshIndex = scene->dataSet->GetMeshID(currentTriangleIndex);

				// Get the triangle
				const ExtMesh *mesh = scene->objects[currentMeshIndex];
				const unsigned int triIndex = scene->dataSet->GetMeshTriangleID(currentTriangleIndex);

				// Get the material
				const Material *triMat = scene->objectMaterials[currentMeshIndex];

				// Check if it is a light source
				if (triMat->IsLightSource()) {
					// SLG light sources are like black bodies
					break;
				}

				// Interpolate face normal
				Normal N = mesh->InterpolateTriNormal(triIndex, nextEventRayHit.b1, nextEventRayHit.b2);

				surfMat = (SurfaceMaterial *)triMat;
				const Point hitPoint = nextEventRay(nextEventRayHit.t);

				Spectrum surfaceColor;
				if (mesh->HasColors())
					surfaceColor = mesh->InterpolateTriColor(triIndex, nextEventRayHit.b1, nextEventRayHit.b2);
				else
					surfaceColor = Spectrum(1.f, 1.f, 1.f);

				// Check if I have to apply texture mapping or normal mapping
				TexMapInstance *tm = scene->objectTexMaps[currentMeshIndex];
				BumpMapInstance *bm = scene->objectBumpMaps[currentMeshIndex];
				NormalMapInstance *nm = scene->objectNormalMaps[currentMeshIndex];
				if (tm || bm || nm) {
					// Interpolate UV coordinates if required
					const UV triUV = mesh->InterpolateTriUV(triIndex, nextEventRayHit.b1, nextEventRayHit.b2);

					// Check if there is an assigned texture map
					if (tm) {
						const TextureMap *map = tm->GetTexMap();

						// Apply texture mapping
						surfaceColor *= map->GetColor(triUV);

						// Check if the texture map has an alpha channel
						if (map->HasAlpha()) {
							const float alpha = map->GetAlpha(triUV);

							if ((alpha == 0.0f) || ((alpha < 1.f) && (rndGen->floatValue() > alpha))) {
								// It is a pass-through material, continue to trace the ray
								nextEventRay.mint = nextEventRayHit.t + MachineEpsilon::E(nextEventRayHit.t);
								nextEventRay.maxt = std::numeric_limits<float>::infinity();
								++depth;
								continue;
							}
						}
					}

					// Check if there is an assigned bump/normal map
					if (bm || nm) {
						if (nm) {
							// Apply normal mapping
							const Spectrum color = nm->GetTexMap()->GetColor(triUV);

							const float x = 2.f * (color.r - 0.5f);
							const float y = 2.f * (color.g - 0.5f);
							const float z = 2.f * (color.b - 0.5f);

							Vector v1, v2;
							CoordinateSystem(Vector(N), &v1, &v2);
							N = Normalize(Normal(
									v1.x * x + v2.x * y + N.x * z,
									v1.y * x + v2.y * y + N.y * z,
									v1.z * x + v2.z * y + N.z * z));
						}

						if (bm) {
							// Apply bump mapping
							const TextureMap *map = bm->GetTexMap();
							const UV &dudv = map->GetDuDv();

							const float b0 = map->GetColor(triUV).Filter();

							const UV uvdu(triUV.u + dudv.u, triUV.v);
							const float bu = map->GetColor(uvdu).Filter();

							const UV uvdv(triUV.u, triUV.v + dudv.v);
							const float bv = map->GetColor(uvdv).Filter();

							const float scale = bm->GetScale();
							const Vector bump(scale * (bu - b0), scale * (bv - b0), 1.f);

							Vector v1, v2;
							CoordinateSystem(Vector(N), &v1, &v2);
							N = Normalize(Normal(
									v1.x * bump.x + v2.x * bump.y + N.x * bump.z,
									v1.y * bump.x + v2.y * bump.y + N.y * bump.z,
									v1.z * bump.x + v2.z * bump.y + N.z * bump.z));
						}
					}
				}

				// Flip the normal if required
				shadeN = (Dot(-nextEventRay.d, N) > 0.f) ? N : -N;

				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				float fPdf;
				Vector wo;
				const Spectrum f = surfMat->Sample(-nextEventRay.d, &wo, N, shadeN,
						rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
						&fPdf);
				if ((fPdf <= 0.f) || f.Black())
					break;

				lightPathFlux *= Dot(shadeN, wo) * f / fPdf;
				assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

				/*if (depth > renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = Max(lightPathRadiance.Filter(), renderEngine->rrImportanceCap);
					if (prob >= rndGen->floatValue())
						lightPathRadiance /= prob;
					else
						break;
				}*/

				nextEventRay = Ray(hitPoint, wo);

				++depth;
			} else {
				// Ray lost in space...
				break;
			}
		} while (depth < renderEngine->maxPathDepth);
	}

	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");

	if (renderThread->sampleBuffer)	{
		// Film::GetFreeSampleBuffer() is not thread safe
		boost::unique_lock<boost::mutex> lock(*(renderEngine->filmMutex));
		film->FreeSampleBuffer(renderThread->sampleBuffer);
	}
}
