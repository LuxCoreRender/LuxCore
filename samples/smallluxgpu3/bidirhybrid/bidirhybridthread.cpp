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

#include "smalllux.h"
#include "renderconfig.h"
#include "bidirhybrid/bidirhybrid.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/opencl/utils.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

//------------------------------------------------------------------------------
// BiDirHybridRenderThread
//------------------------------------------------------------------------------

BiDirHybridRenderThread::BiDirHybridRenderThread(const unsigned int index,
		const unsigned int seedBase, IntersectionDevice *device,
		BiDirHybridRenderEngine *re) {
	intersectionDevice = device;
	seed = seedBase;

	renderThread = NULL;
	threadFilm = NULL;

	threadIndex = index;
	renderEngine = re;

	samplesCount = 0.0;

	pendingRayBuffers = 0;
	currentRayBufferToSend = NULL;
	currentReiceivedRayBuffer = NULL;

	started = false;
	editMode = false;
}

BiDirHybridRenderThread::~BiDirHybridRenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete threadFilm;
}

void BiDirHybridRenderThread::Start() {
	started = true;
	StartRenderThread();
}

void BiDirHybridRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void BiDirHybridRenderThread::Stop() {
	StopRenderThread();
	started = false;
}

void BiDirHybridRenderThread::StartRenderThread() {
	const unsigned int filmWidth = renderEngine->film->GetWidth();
	const unsigned int filmHeight = renderEngine->film->GetHeight();

	delete threadFilm;
	threadFilm = new Film(filmWidth, filmHeight, true, true, false);
	threadFilm->CopyDynamicSettings(*(renderEngine->film));
	threadFilm->Init(filmWidth, filmHeight);

	samplesCount = 0.0;

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(BiDirHybridRenderThread::RenderThreadImpl, this));
}

void BiDirHybridRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void BiDirHybridRenderThread::BeginEdit() {
	StopRenderThread();
}

void BiDirHybridRenderThread::EndEdit(const EditActionList &editActions) {
	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

size_t BiDirHybridRenderThread::PushRay(const Ray &ray) {
	// Check if I have a valid currentRayBuffer
	if (!currentRayBufferToSend) {
		if (freeRayBuffers.size() == 0) {
			// I have to allocate a new RayBuffer
			currentRayBufferToSend = intersectionDevice->NewRayBuffer();
		} else {
			// I can reuse one in queue
			currentRayBufferToSend = freeRayBuffers.front();
			freeRayBuffers.pop_front();
		}
	}

	const size_t index = currentRayBufferToSend->AddRay(ray);

	// Check if the buffer is now full
	if (currentRayBufferToSend->IsFull()) {
		// Send the work to the device
		intersectionDevice->PushRayBuffer(currentRayBufferToSend);
		currentRayBufferToSend = NULL;
		++pendingRayBuffers;
	}

	return index;
}

void BiDirHybridRenderThread::PopRay(const Ray **ray, const RayHit **rayHit) {
	// Check if I have to get  the results out of intersection device
	if (!currentReiceivedRayBuffer) {
		currentReiceivedRayBuffer = intersectionDevice->PopRayBuffer();
		--pendingRayBuffers;
		currentReiceivedRayBufferIndex = 0;
	} else if (currentReiceivedRayBufferIndex >= currentReiceivedRayBuffer->GetSize()) {
		// All the results in the RayBuffer has been elaborated
		currentReiceivedRayBuffer->Reset();
		freeRayBuffers.push_back(currentReiceivedRayBuffer);

		// Get a new buffer
		currentReiceivedRayBuffer = intersectionDevice->PopRayBuffer();
		--pendingRayBuffers;
		currentReiceivedRayBufferIndex = 0;
	}

	*ray = currentReiceivedRayBuffer->GetRay(currentReiceivedRayBufferIndex);
	*rayHit = currentReiceivedRayBuffer->GetRayHit(currentReiceivedRayBufferIndex++);
}

void BiDirHybridRenderThread::RenderThreadImpl(BiDirHybridRenderThread *renderThread) {
	//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread started");
	boost::this_thread::disable_interruption di;

	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);

	const u_int incrementStep = 4096;
	vector<BiDirState *> states(incrementStep);

	try {
		// Initialize the first states
		for (u_int i = 0; i < states.size(); ++i)
			states[i] = new BiDirState(renderEngine, renderThread->threadFilm, rndGen);

		u_int generateIndex = 0;
		u_int collectIndex = 0;
		while (!boost::this_thread::interruption_requested()) {
			// Generate new rays up to the point to have 2 pending buffers
			while (renderThread->pendingRayBuffers < 2) {
				states[generateIndex]->GenerateRays(renderThread);

				generateIndex = (generateIndex + 1) % states.size();
				if (generateIndex == collectIndex) {
					//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Increasing states size by " << incrementStep);
					//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] State size: " << states.size());

					// Insert a set of new states and continue
					states.insert(states.begin() + generateIndex, incrementStep, NULL);
					for (u_int i = generateIndex; i < generateIndex + incrementStep; ++i)
						states[i] = new BiDirState(renderEngine, renderThread->threadFilm, rndGen);
					collectIndex += incrementStep;
				}
			}
			
			//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] State size: " << states.size());
			//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] generateIndex: " << generateIndex);
			//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] collectIndex: " << collectIndex);
			//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] pendingRayBuffers: " << renderThread->pendingRayBuffers);

			// Collect rays up to the point to have only 1 pending buffer
			while (renderThread->pendingRayBuffers > 1) {
				states[collectIndex]->CollectResults(renderThread);
				renderThread->samplesCount += 1.0;

				const u_int newCollectIndex = (collectIndex + 1) % states.size();
				// A safety-check, it should never happen
				if (newCollectIndex == generateIndex)
					break;
				collectIndex = newCollectIndex;
			}
		}

		//SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		SLG_LOG("[BiDirHybridRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}

	// Clean current ray buffers
	if (renderThread->currentRayBufferToSend) {
		renderThread->currentRayBufferToSend->Reset();
		renderThread->freeRayBuffers.push_back(renderThread->currentRayBufferToSend);
		renderThread->currentRayBufferToSend = NULL;
	}
	if (renderThread->currentReiceivedRayBuffer) {
		renderThread->currentReiceivedRayBuffer->Reset();
		renderThread->freeRayBuffers.push_back(renderThread->currentReiceivedRayBuffer);
		renderThread->currentReiceivedRayBuffer = NULL;
	}

	// Free all states
	for (u_int i = 0; i < states.size(); ++i)
		delete states[i];
	delete rndGen;

	// Remove all pending ray buffers
	while (renderThread->pendingRayBuffers > 0) {
		RayBuffer *rayBuffer = renderThread->intersectionDevice->PopRayBuffer();
		--(renderThread->pendingRayBuffers);
		rayBuffer->Reset();
		renderThread->freeRayBuffers.push_back(rayBuffer);
	}
}
