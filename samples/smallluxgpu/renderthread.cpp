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

#include <cstring>

#include "renderthread.h"
#include "samplebuffer.h"
#include "renderconfig.h"
#include "path.h"


RenderThread::RenderThread(unsigned int index, Scene *scn) {
	threadIndex = index;
	scene = scn;
	started = false;
}

RenderThread::~RenderThread() {
}

//------------------------------------------------------------------------------
// NativeRenderThread
//------------------------------------------------------------------------------

NativeRenderThread::NativeRenderThread(unsigned int index,  const unsigned long seedBase,
		const float samplStart, NativeThreadIntersectionDevice *device,
		Scene *scn, const bool lowLatency) : RenderThread(index, scn) {
	intersectionDevice = device;
	samplingStart = samplStart;

	// Allocate buffers

	// Sample buffer
	const size_t sampleBufferSize = lowLatency ? (SAMPLE_BUFFER_SIZE / 4) : SAMPLE_BUFFER_SIZE;
	sampleBuffer = new SampleBuffer(sampleBufferSize);

	// Ray buffer (small buffers work well with CPU)
	const size_t rayBufferSize = 1024;
	const unsigned int startLine = Clamp<unsigned int>(
		scene->camera->film->GetHeight() * samplingStart,
			0, scene->camera->film->GetHeight() - 1);
	sampler = new RandomSampler(lowLatency, seedBase + threadIndex + 1,
		scene->camera->film->GetWidth(), scene->camera->film->GetHeight(),
		startLine);

	pathIntegrator = new PathIntegrator(scene, sampler, sampleBuffer);
	rayBuffer = new RayBuffer(rayBufferSize);

	renderThread = NULL;
}

NativeRenderThread::~NativeRenderThread() {
	if (started)
		Stop();

	delete rayBuffer;
	delete pathIntegrator;
	delete sampler;
	delete sampleBuffer;
}

void NativeRenderThread::Start() {
	RenderThread::Start();

	const unsigned int startLine = Clamp<unsigned int>(
		scene->camera->film->GetHeight() * samplingStart,
			0, scene->camera->film->GetHeight() - 1);
	sampler->Init(scene->camera->film->GetWidth(), scene->camera->film->GetHeight(), startLine);
	sampleBuffer->Reset();
	rayBuffer->Reset();
	pathIntegrator->ReInit();

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(NativeRenderThread::RenderThreadImpl, this));
}

void NativeRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void NativeRenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	RenderThread::Stop();
}

void NativeRenderThread::ClearPaths() {
	pathIntegrator->ClearPaths();
}

void NativeRenderThread::RenderThreadImpl(NativeRenderThread *renderThread) {
	cerr << "[NativeRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	try {
		RayBuffer *rayBuffer = renderThread->rayBuffer;
		PathIntegrator *pathIntegrator = renderThread->pathIntegrator;
		NativeThreadIntersectionDevice *intersectionDevice = renderThread->intersectionDevice;

		while (!boost::this_thread::interruption_requested()) {
			rayBuffer->Reset();
			pathIntegrator->FillRayBuffer(rayBuffer);
			intersectionDevice->PushRayBuffer(rayBuffer);
			rayBuffer = intersectionDevice->PopRayBuffer();
			pathIntegrator->AdvancePaths(rayBuffer);
		}

		cerr << "[NativeRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[NativeRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	catch (cl::Error err) {
		cerr << "[NativeRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
#endif
}

//------------------------------------------------------------------------------
// DeviceRenderThread
//------------------------------------------------------------------------------

DeviceRenderThread::DeviceRenderThread(const unsigned int index, const unsigned long seedBase,
		const float samplStart, IntersectionDevice *device,
		Scene *scn, const bool lowLatency) : RenderThread(index, scn) {
	intersectionDevice = device;
	samplingStart = samplStart;
	reportedPermissionError = false;

	// Allocate buffers

	// Sample buffer
	const size_t sampleBufferSize = lowLatency ? (SAMPLE_BUFFER_SIZE / 4) : SAMPLE_BUFFER_SIZE;
	sampleBuffer = new SampleBuffer(sampleBufferSize);

	// Ray buffer
	// TODO: cross check RAY_BUFFER_SIZE with the Intersection device
	const size_t rayBufferSize = lowLatency ? (RAY_BUFFER_SIZE / 8) : RAY_BUFFER_SIZE;
	const unsigned int startLine = Clamp<unsigned int>(
		scene->camera->film->GetHeight() * samplingStart,
			0, scene->camera->film->GetHeight() - 1);
	sampler = new RandomSampler(lowLatency, seedBase + threadIndex + 1,
		scene->camera->film->GetWidth(), scene->camera->film->GetHeight(),
		startLine);
	for(size_t i = 0; i < DEVICE_RENDER_BUFFER_COUNT; i++) {
		pathIntegrators[i] = new PathIntegrator(scene, sampler, sampleBuffer);
		rayBuffers[i] = new RayBuffer(rayBufferSize);
		rayBuffers[i]->PushUserData(i);
	}

	renderThread = NULL;
}

DeviceRenderThread::~DeviceRenderThread() {
	if (started)
		Stop();

	for(size_t i = 0; i < DEVICE_RENDER_BUFFER_COUNT; i++) {
		delete rayBuffers[i];
		delete pathIntegrators[i];
	}
	delete sampler;
	delete sampleBuffer;
}

void DeviceRenderThread::Start() {
	RenderThread::Start();

		const unsigned int startLine = Clamp<unsigned int>(
		scene->camera->film->GetHeight() * samplingStart,
			0, scene->camera->film->GetHeight() - 1);
	sampler->Init(scene->camera->film->GetWidth(), scene->camera->film->GetHeight(), startLine);
	sampleBuffer->Reset();
	for(size_t i = 0; i < DEVICE_RENDER_BUFFER_COUNT; i++) {
		rayBuffers[i]->Reset();
		rayBuffers[i]->ResetUserData();
		rayBuffers[i]->PushUserData(i);
		pathIntegrators[i]->ReInit();
	}

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(DeviceRenderThread::RenderThreadImpl, this));

	// Set renderThread priority
	bool res = SetThreadRRPriority(renderThread);
	if (res && !reportedPermissionError) {
		cerr << "[NativeRenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
		reportedPermissionError = true;
	}
}

void DeviceRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void DeviceRenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	RenderThread::Stop();
}

void DeviceRenderThread::ClearPaths() {
	for(size_t i = 0; i < DEVICE_RENDER_BUFFER_COUNT; i++)
		pathIntegrators[i]->ClearPaths();
}

void DeviceRenderThread::RenderThreadImpl(DeviceRenderThread *renderThread) {
	cerr << "[DeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	std::deque<RayBuffer *> todoBuffers;
	for(size_t i = 0; i < DEVICE_RENDER_BUFFER_COUNT; i++)
		todoBuffers.push_back(renderThread->rayBuffers[i]);

	try {
		while (!boost::this_thread::interruption_requested()) {
			// Produce buffers to trace
			while (todoBuffers.size() > 0) {
				RayBuffer *rayBuffer = todoBuffers.front();
				todoBuffers.pop_front();

				rayBuffer->Reset();
				renderThread->pathIntegrators[rayBuffer->GetUserData()]->FillRayBuffer(rayBuffer);
				renderThread->intersectionDevice->PushRayBuffer(rayBuffer);
			}

			RayBuffer *rayBuffer = renderThread->intersectionDevice->PopRayBuffer();
			renderThread->pathIntegrators[rayBuffer->GetUserData()]->AdvancePaths(rayBuffer);
			todoBuffers.push_back(rayBuffer);
		}

		cerr << "[DeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[DeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	catch (cl::Error err) {
		cerr << "[DeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
#endif
}
