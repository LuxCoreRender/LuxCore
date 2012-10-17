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
		float lpdf;
		const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lpdf);

		// Initialize the light path
		float pdf;
		Ray nextEventRay;
		Spectrum flux = light->Sample_L(scene,
			rndGen->floatValue(), rndGen->floatValue(),
			rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue(),
			&pdf, &nextEventRay);
		flux /= pdf * lpdf;
		//unsigned int depth = 0;

		{
			// Try to connect the light path vertex with the eye
			Vector eyeDir(scene->camera->orig - nextEventRay.o);
			const float distance = eyeDir.Length();
			eyeDir /= distance;

			//float eyePdf;
			//Spectrum eyeFlux = light->Sample_f(nextEventRay.o, eyeDir, &eyePdf);
			Spectrum eyeFlux(1.f, 1.f, 1.f);

			Ray eyeRay(nextEventRay.o, eyeDir);
			eyeRay.maxt = distance;

			RayHit eyeRayHit;
			if (!scene->dataSet->Intersect(&eyeRay, &eyeRayHit)) {
				// Nothing was hit, the light vertex is visible

				float scrX, scrY;
				if (scene->camera->GetSamplePosition(eyeRay.o, -eyeRay.d, distance, &scrX, &scrY))
					renderThread->SplatSample(scrX, scrY, eyeFlux);
			}
		}

		// Trace the path
		/*do {

			
			Ray eyeRay(nextEventRay.o, eyeDir);

			RayHit eyeRayHit;
			if (!scene->dataSet->Intersect(eyeRay, &eyeRayHit)) {
				// Nothing was hit, the light vertex is visible
			}
		} while (depth < renderEngine->maxPathDepth);*/
	}

	//SLG_LOG("[LightCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");

	if (renderThread->sampleBuffer)	{
		// Film::GetFreeSampleBuffer() is not thread safe
		boost::unique_lock<boost::mutex> lock(*(renderEngine->filmMutex));
		film->FreeSampleBuffer(renderThread->sampleBuffer);
	}
}
