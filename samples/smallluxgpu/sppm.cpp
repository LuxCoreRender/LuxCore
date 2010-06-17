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
#include <string>
#include <sstream>
#include <stdexcept>

#include "luxrays/core/pixel/samplebuffer.h"

#include "smalllux.h"
#include "sppm.h"
#include "renderconfig.h"
#include "displayfunc.h"
#include "hitpoints.h"

//------------------------------------------------------------------------------
// SPPMRenderThread
//------------------------------------------------------------------------------

SPPMRenderThread::SPPMRenderThread(unsigned int index, SPPMRenderEngine *re) {
	threadIndex = index;
	renderEngine = re;
	started = false;
}

SPPMRenderThread::~SPPMRenderThread() {
}

//------------------------------------------------------------------------------
// SPPMDeviceRenderThread
//------------------------------------------------------------------------------

SPPMDeviceRenderThread::SPPMDeviceRenderThread(const unsigned int index, const unsigned long seedBase,
		IntersectionDevice *device,	SPPMRenderEngine *re) : SPPMRenderThread(index, re) {
	intersectionDevice = device;
	reportedPermissionError = false;

	renderThread = NULL;

	rayBuffer = intersectionDevice->NewRayBuffer();
	if (threadIndex == 0)
		rayBufferHitPoints = intersectionDevice->NewRayBuffer();
	else
		rayBufferHitPoints = NULL;
}

SPPMDeviceRenderThread::~SPPMDeviceRenderThread() {
	if (started)
		Stop();

	delete rayBuffer;
	delete rayBufferHitPoints;
}

void SPPMDeviceRenderThread::Start() {
	SPPMRenderThread::Start();

	rayBuffer->Reset();
	if (threadIndex == 0)
		rayBufferHitPoints->Reset();

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(SPPMDeviceRenderThread::RenderThreadImpl, this));

	// Set renderThread priority
	bool res = SetThreadRRPriority(renderThread);
	if (res && !reportedPermissionError) {
		cerr << "[SPPMDeviceRenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
		reportedPermissionError = true;
	}
}

void SPPMDeviceRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void SPPMDeviceRenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	SPPMRenderThread::Stop();
}

void SPPMDeviceRenderThread::UpdateFilm(Film *film, HitPoints *hitPoints, SampleBuffer *&sampleBuffer) {
	film->Reset();

	const unsigned int imgWidth = film->GetWidth();
	for (unsigned int i = 0; i < hitPoints->GetSize(); ++i) {
		HitPoint *hp = hitPoints->GetHitPoint(i);
		const float scrX = i % imgWidth;
		const float scrY = i / imgWidth;
		sampleBuffer->SplatSample(scrX, scrY, hp->radiance);

		if (sampleBuffer->IsFull()) {
			// Splat all samples on the film
			film->SplatSampleBuffer(true, sampleBuffer);
			sampleBuffer = film->GetFreeSampleBuffer();
		}
	}

	if (sampleBuffer->GetSampleCount() > 0) {
		// Splat all samples on the film
		film->SplatSampleBuffer(true, sampleBuffer);
		sampleBuffer = film->GetFreeSampleBuffer();
	}
}

void SPPMDeviceRenderThread::InitPhotonPath(Scene *scene, RandomGenerator *rndGen,
	PhotonPath *photonPath, Ray *ray, unsigned int *photonTracedPass) {
	// Select one light source
	float lpdf;
	const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lpdf);

	// Initialize the photon path
	float pdf;
	photonPath->flux = light->Sample_L(scene,
		rndGen->floatValue(), rndGen->floatValue(),
		rndGen->floatValue(), rndGen->floatValue(),
		&pdf, ray);
	photonPath->flux /= pdf * lpdf;
	photonPath->depth = 0;

	AtomicInc(photonTracedPass);
}

void SPPMDeviceRenderThread::RenderThreadImpl(SPPMDeviceRenderThread *renderThread) {
	cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	SPPMRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->scene;
	RandomGenerator *rndGen = new RandomGenerator();
	rndGen->init(renderThread->threadIndex + renderEngine->seedBase + 1);

	IntersectionDevice *device = renderThread->intersectionDevice;
	RayBuffer *rayBuffer = renderThread->rayBuffer;
	RayBuffer *rayBufferHitPoints = renderThread->rayBufferHitPoints;

	if (renderThread->threadIndex == 0) {
		// Build the EyePaths list
		renderEngine->hitPoints = new HitPoints(
				scene, rndGen, device,
				rayBufferHitPoints, renderEngine->photonAlpha,
				renderEngine->maxEyePathDepth,
				renderEngine->film->GetWidth(), renderEngine->film->GetHeight(),
				renderEngine->accelType);
	}

	HitPoints *hitPoints = NULL;
	try {
		// Wait for other threads
		renderEngine->barrierStart->wait();

		hitPoints = renderEngine->hitPoints;

		//std::cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Tracing photon paths" << std::endl;

		// Generate photons from light sources
		std::vector<PhotonPath> photonPaths(rayBuffer->GetSize());
		Ray *rays = rayBuffer->GetRayBuffer();
		for (unsigned int i = 0; i < photonPaths.size(); ++i) {
			// Note: there is some assumption here about how the
			// rayBuffer->ReserveRay() work
			InitPhotonPath(scene, rndGen, &photonPaths[i], &rays[rayBuffer->ReserveRay()],
					&renderEngine->photonTracedPass);
		}

		while (!boost::this_thread::interruption_requested()) {
			// Trace the rays
			device->PushRayBuffer(rayBuffer);
			rayBuffer = device->PopRayBuffer();

			for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i) {
				PhotonPath *photonPath = &photonPaths[i];
				Ray *ray = &rayBuffer->GetRayBuffer()[i];
				const RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];

				if (rayHit->Miss()) {
					// Re-initialize the photon path
					InitPhotonPath(scene, rndGen, photonPath, ray,
							&renderEngine->photonTracedPass);
				} else {
					// Something was hit
					Point hitPoint;
					Spectrum surfaceColor;
					Normal N, shadeN;
					if (GetHitPointInformation(scene, rndGen, ray, rayHit, hitPoint,
							surfaceColor, N, shadeN))
						continue;

					// Get the material
					const unsigned int currentTriangleIndex = rayHit->index;
					const Material *triMat = scene->triangleMaterials[currentTriangleIndex];

					if (triMat->IsLightSource()) {
						// Re-initialize the photon path
						InitPhotonPath(scene, rndGen, photonPath, ray,
								&renderEngine->photonTracedPass);
					} else {
						const SurfaceMaterial *triSurfMat = (SurfaceMaterial *)triMat;

						float fPdf;
						Vector wi;
						bool specularBounce;
						const Spectrum f = triSurfMat->Sample_f(-ray->d, &wi, N, shadeN,
								rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
								false, &fPdf, specularBounce) * surfaceColor;

						if (!specularBounce)
							hitPoints->AddFlux(hitPoint, shadeN, -ray->d, photonPath->flux);

						// Check if we reached the max. depth
						if (photonPath->depth < renderEngine->maxPhotonPathDepth) {
							// Build the next vertex path ray
							if ((fPdf <= 0.f) || f.Black()) {
								// Re-initialize the photon path
								InitPhotonPath(scene, rndGen, photonPath, ray,
										&renderEngine->photonTracedPass);
							} else {
								photonPath->depth++;
								photonPath->flux *= f / fPdf;

								// Russian Roulette
								const float p = 0.7f;
								if ((photonPath->depth < 2) || (specularBounce)) {
									*ray = Ray(hitPoint, wi);
								} else if (rndGen->floatValue() < p) {
									photonPath->flux /= p;
									*ray = Ray(hitPoint, wi);
								} else {
									// Re-initialize the photon path
									InitPhotonPath(scene, rndGen, photonPath, ray,
											&renderEngine->photonTracedPass);
								}
							}
						} else {
							// Re-initialize the photon path
							InitPhotonPath(scene, rndGen, photonPath, ray,
									&renderEngine->photonTracedPass);
						}
					}
				}
			}

			// Check if it is time to do an eye pass
			// TODO: add a parameter to tune rehashing intervals
			if (renderEngine->photonTracedPass > renderEngine->stochasticInterval) {
				// Wait for other threads
				renderEngine->barrierStop->wait();

				// The first thread does the eye pass
				if (renderThread->threadIndex == 0) {
					const long long count = renderEngine->photonTracedTotal + renderEngine->photonTracedPass;
					hitPoints->Recast(rndGen, rayBufferHitPoints, count);

					// Update the frame buffer
					UpdateFilm(renderEngine->film, hitPoints, renderEngine->sampleBuffer);

					renderEngine->photonTracedTotal = count;
					renderEngine->photonTracedPass = 0;
				}

				// Wait for other threads
				renderEngine->barrierStart->wait();
				//std::cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Tracing photon paths" << std::endl;
			}
		}

		cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	catch (cl::Error err) {
		cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
#endif

	// Wait for other threads
	renderEngine->barrierExit->wait();

	if (renderThread->threadIndex == 0) {
		renderEngine->hitPoints = NULL;
		delete hitPoints;
	}
}

//------------------------------------------------------------------------------
// SPPMRenderEngine
//------------------------------------------------------------------------------

SPPMRenderEngine::SPPMRenderEngine(SLGScene *scn, Film *flm,
		vector<IntersectionDevice *> intersectionDev,
		const Properties &cfg) : RenderEngine(scn, flm) {
	intersectionDevices = intersectionDev;

	const int atype = cfg.GetInt("sppm.lookup.type", 2);
	switch (atype) {
		case 0:
			accelType = HASH_GRID;
			break;
		case 1:
			accelType = KD_TREE;
			break;
		case 2:
			accelType = HYBRID_HASH_GRID;
			break;
		default:
			throw runtime_error("Unknown value for SPPMRenderEngine property sppm.lookup.type");
	}

	seedBase = (unsigned long)(WallClockTime() / 1000.0);
	photonAlpha = 0.7f;
	maxEyePathDepth = 16;
	maxPhotonPathDepth = 8;
	stochasticInterval = cfg.GetInt("sppm.stochastic.count", 5000000);

	startTime = 0.0;
	hitPoints = NULL;
	sampleBuffer = film->GetFreeSampleBuffer();

	// Create and start render threads
	const size_t renderThreadCount = intersectionDevices.size();
	cerr << "Starting "<< renderThreadCount << " SPPM render threads" << endl;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		SPPMDeviceRenderThread *t = new SPPMDeviceRenderThread(i, seedBase, intersectionDevices[i], this);
		renderThreads.push_back(t);
	}
}

SPPMRenderEngine::~SPPMRenderEngine() {
	if (started) {
		Interrupt();
		Stop();
	}

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	film->FreeSampleBuffer(sampleBuffer);
}

void SPPMRenderEngine::Start() {
	RenderEngine::Start();

	startTime = WallClockTime();
	photonTracedTotal = 0;
	photonTracedPass = 0;

	// Create synchronization barriers
	barrierStop = new boost::barrier(renderThreads.size());
	barrierStart = new boost::barrier(renderThreads.size());
	barrierExit = new boost::barrier(renderThreads.size());

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void SPPMRenderEngine::Interrupt() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
}

void SPPMRenderEngine::Stop() {
	RenderEngine::Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	delete barrierStop;
	delete barrierStart;
	delete barrierExit;
}

void SPPMRenderEngine::Reset() {
	assert (!started);
}

unsigned int SPPMRenderEngine::GetPass() const {
	return (hitPoints) ? hitPoints->GetPassCount() : 0;
}

unsigned int SPPMRenderEngine::GetThreadCount() const {
	return renderThreads.size();
}
