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

	for (unsigned int i = 0; i < SPPM_DEVICE_RENDER_BUFFER_COUNT; ++i) {
		rayBuffersList.push_back(intersectionDevice->NewRayBuffer());
		rayBuffersList[i]->PushUserData(i);
		photonPathsList.push_back(new std::vector<PhotonPath>(rayBuffersList[i]->GetSize()));
	}
	rayBufferHitPoints = intersectionDevice->NewRayBuffer();
}

SPPMDeviceRenderThread::~SPPMDeviceRenderThread() {
	if (started)
		Stop();

	for (unsigned int i = 0; i < SPPM_DEVICE_RENDER_BUFFER_COUNT; ++i) {
		delete rayBuffersList[i];
		delete photonPathsList[i];
	}
	delete rayBufferHitPoints;
}

void SPPMDeviceRenderThread::Start() {
	SPPMRenderThread::Start();

	for (unsigned int i = 0; i < SPPM_DEVICE_RENDER_BUFFER_COUNT; ++i)
		rayBuffersList[i]->Reset();
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

void SPPMDeviceRenderThread::UpdateFilm(Film *film, boost::mutex *filmMutex,
		HitPoints *hitPoints, SampleBuffer *&sampleBuffer) {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

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
		rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
		&pdf, ray);
	photonPath->flux /= pdf * lpdf;
	photonPath->depth = 0;

	AtomicInc(photonTracedPass);
}

void SPPMDeviceRenderThread::AdvancePhotonPaths(
	SPPMRenderEngine *renderEngine, HitPoints *hitPoints,
	Scene *scene, RandomGenerator *rndGen,
	RayBuffer *rayBuffer, std::vector<PhotonPath> &photonPaths) {
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
			const unsigned int currentMeshIndex = scene->dataSet->GetMeshID(currentTriangleIndex);
			const Material *triMat = scene->objectMaterials[currentMeshIndex];

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
					hitPoints->AddFlux(hitPoint, -ray->d, photonPath->flux);

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
}

void SPPMDeviceRenderThread::RenderThreadImpl(SPPMDeviceRenderThread *renderThread) {
	cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	// Disable interruption exception, I'm going to handle interruption on
	// few fix checkpoints
	boost::this_thread::disable_interruption di;

	SPPMRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->scene;
	RandomGenerator *rndGen = new RandomGenerator();
	rndGen->init(renderThread->threadIndex + renderEngine->seedBase + 1);

	IntersectionDevice *device = renderThread->intersectionDevice;
	RayBuffer *rayBufferHitPoints = renderThread->rayBufferHitPoints;

	HitPoints *hitPoints = NULL;
	try {
		// First eye pass
		if (renderThread->threadIndex == 0) {
			// One thread initialize the EyePaths list
			renderEngine->hitPoints = new HitPoints(renderEngine);
		}

		// Wait for other threads
		renderEngine->barrier->wait();

		hitPoints = renderEngine->hitPoints;
		// Multi-threads calculate hit points
		hitPoints->SetHitPoints(rndGen, device, rayBufferHitPoints, renderThread->threadIndex, renderEngine->renderThreads.size());

		// Wait for other threads
		renderEngine->barrier->wait();

		if (renderThread->threadIndex == 0) {
			// One thread finish the initialization
			hitPoints->Init();
		}

		// Wait for other threads
		renderEngine->barrier->wait();

		// Last step of the initialization
		hitPoints->RefreshAccelParallel(renderThread->threadIndex, renderEngine->renderThreads.size());

		// Wait for other threads
		renderEngine->barrier->wait();

		//std::cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Tracing photon paths" << std::endl;

		// Generate photons from light sources
		for (unsigned int j = 0; j < SPPM_DEVICE_RENDER_BUFFER_COUNT; ++j) {
			std::vector<PhotonPath> &photonPaths = *(renderThread->photonPathsList[j]);
			RayBuffer *rayBuffer =  renderThread->rayBuffersList[j];
			Ray *rays = rayBuffer->GetRayBuffer();

			for (unsigned int i = 0; i < photonPaths.size(); ++i) {
				// Note: there is some assumption here about how the
				// rayBuffer->ReserveRay() work
				InitPhotonPath(scene, rndGen, &photonPaths[i], &rays[rayBuffer->ReserveRay()],
						&renderEngine->photonTracedPass);
			}
		}

		std::deque<RayBuffer *> todoBuffers;
		for(unsigned int i = 0; i < SPPM_DEVICE_RENDER_BUFFER_COUNT; i++)
			todoBuffers.push_back(renderThread->rayBuffersList[i]);

		double passStartTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			for (;;) {
				// Trace the rays
				while (todoBuffers.size() > 0) {
					RayBuffer *rayBuffer = todoBuffers.front();
					todoBuffers.pop_front();

					device->PushRayBuffer(rayBuffer);
				}

				RayBuffer *rayBuffer = device->PopRayBuffer();
				std::vector<PhotonPath> &photonPaths = *(renderThread->photonPathsList[rayBuffer->GetUserData()]);
				AdvancePhotonPaths(renderEngine, hitPoints, scene, rndGen, rayBuffer, photonPaths);
				todoBuffers.push_back(rayBuffer);

				// Check if it is time to do an eye pass
				if ((renderEngine->photonTracedPass > renderEngine->stochasticInterval) ||
					boost::this_thread::interruption_requested()) {
					// Ok, time to stop, finish current work
					const unsigned int pendingBuffers = SPPM_DEVICE_RENDER_BUFFER_COUNT - todoBuffers.size();
					for(unsigned int i = 0; i < pendingBuffers; i++) {
						RayBuffer *rayBuffer = device->PopRayBuffer();
						std::vector<PhotonPath> &photonPaths = *(renderThread->photonPathsList[rayBuffer->GetUserData()]);
						AdvancePhotonPaths(renderEngine, hitPoints, scene, rndGen, rayBuffer, photonPaths);
						todoBuffers.push_back(rayBuffer);
					}
					break;
				}
			}

			// Wait for other threads
			renderEngine->barrier->wait();

			const double t1 = WallClockTime();

			const long long count = renderEngine->photonTracedTotal + renderEngine->photonTracedPass;
			hitPoints->AccumulateFlux(count, renderThread->threadIndex, renderEngine->renderThreads.size());
			hitPoints->SetHitPoints(rndGen, device, rayBufferHitPoints, renderThread->threadIndex, renderEngine->renderThreads.size());

			// Wait for other threads
			renderEngine->barrier->wait();

			// The first thread has to do some special task for the eye pass
			if (renderThread->threadIndex == 0) {
				// First thread only tasks
				hitPoints->UpdatePointsInformation();
				hitPoints->IncPass();
				hitPoints->RefreshAccelMutex();

				// Update the frame buffer
				UpdateFilm(renderEngine->film, renderEngine->filmMutex, hitPoints, renderEngine->sampleBuffer);

				renderEngine->photonTracedTotal = count;
				renderEngine->photonTracedPass = 0;
			}

			// Wait for other threads
			renderEngine->barrier->wait();

			hitPoints->RefreshAccelParallel(renderThread->threadIndex, renderEngine->renderThreads.size());

			// Wait for other threads
			renderEngine->barrier->wait();

			if (renderThread->threadIndex == 0) {
				const double photonPassTime = t1 - passStartTime;
				std::cerr << "Photon pass time: " << photonPassTime << "secs" << std::endl;
				const double eyePassTime = WallClockTime() - t1;
				std::cerr << "Eye pass time: " << eyePassTime << "secs (" << 100.0 * eyePassTime / (eyePassTime + photonPassTime) << "%)" << std::endl;

				passStartTime = WallClockTime();
			}

			if (boost::this_thread::interruption_requested())
				break;

			//std::cerr << "[SPPMDeviceRenderThread::" << renderThread->threadIndex << "] Tracing photon paths" << std::endl;
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

SPPMRenderEngine::SPPMRenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
		vector<IntersectionDevice *> intersectionDev, const Properties &cfg) : RenderEngine(scn, flm, filmMutex) {
	intersectionDevices = intersectionDev;

	const int atype = cfg.GetInt("sppm.lookup.type", 2);
	switch (atype) {
		case 0:
			lookupAccelType = HASH_GRID;
			break;
		case 1:
			lookupAccelType = KD_TREE;
			break;
		case 2:
			lookupAccelType = HYBRID_HASH_GRID;
			break;
		default:
			throw runtime_error("Unknown value for SPPMRenderEngine property sppm.lookup.type");
	}

	seedBase = (unsigned long)(WallClockTime() / 1000.0);
	maxEyePathDepth = Max(2, cfg.GetInt("sppm.eyepath.maxdepth", 16));

	photonAlpha = Clamp(cfg.GetFloat("sppm.photon.alpha", 0.7f), 0.f, 1.f);
	photonStartRadiusScale = Max(0.f, cfg.GetFloat("sppm.photon.startradiusscale", 1.f));
	maxPhotonPathDepth = Max(2, cfg.GetInt("sppm.photon.maxdepth", 8));

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
	barrier = new boost::barrier(renderThreads.size());
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

	delete barrier;
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
