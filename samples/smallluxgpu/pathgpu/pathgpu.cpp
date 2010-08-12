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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include "luxrays/core/pixel/samplebuffer.h"

#include "smalllux.h"
#include "pathgpu/pathgpu.h"
#include "renderconfig.h"
#include "displayfunc.h"

//------------------------------------------------------------------------------
// PathGPURenderThread
//------------------------------------------------------------------------------

PathGPURenderThread::PathGPURenderThread(unsigned int index, PathGPURenderEngine *re) {
	threadIndex = index;
	renderEngine = re;
	started = false;
}

PathGPURenderThread::~PathGPURenderThread() {
}

//------------------------------------------------------------------------------
// PathGPUDeviceRenderThread
//------------------------------------------------------------------------------

PathGPUDeviceRenderThread::PathGPUDeviceRenderThread(const unsigned int index, const unsigned long seedBase,
		const float samplStart,  const unsigned int samplePerPixel, OpenCLIntersectionDevice *device,
		PathGPURenderEngine *re) : PathGPURenderThread(index, re) {
	intersectionDevice = device;
	samplingStart = samplStart;
	reportedPermissionError = false;

	renderThread = NULL;
}

PathGPUDeviceRenderThread::~PathGPUDeviceRenderThread() {
	if (started)
		Stop();
}

void PathGPUDeviceRenderThread::Start() {
	PathGPURenderThread::Start();

	/*const unsigned int startLine = Clamp<unsigned int>(
			renderEngine->film->GetHeight() * samplingStart,
			0, renderEngine->film->GetHeight() - 1);*/

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(PathGPUDeviceRenderThread::RenderThreadImpl, this));

	// Set renderThread priority
	bool res = SetThreadRRPriority(renderThread);
	if (res && !reportedPermissionError) {
		cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
		reportedPermissionError = true;
	}
}

void PathGPUDeviceRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathGPUDeviceRenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	PathGPURenderThread::Stop();
}

void PathGPUDeviceRenderThread::ClearPaths() {
	assert (!started);
}

void PathGPUDeviceRenderThread::RenderThreadImpl(PathGPUDeviceRenderThread *renderThread) {
	cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	try {
		while (!boost::this_thread::interruption_requested()) {
		}

		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	catch (cl::Error err) {
		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
#endif
}

//------------------------------------------------------------------------------
// PathGPURenderEngine
//------------------------------------------------------------------------------

PathGPURenderEngine::PathGPURenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
		vector<IntersectionDevice *> intersectionDevices, const Properties &cfg) :
		RenderEngine(scn, flm, filmMutex) {
	samplePerPixel = max(1, cfg.GetInt("path.sampler.spp", cfg.GetInt("sampler.spp", 4)));
	maxPathDepth = cfg.GetInt("path.maxdepth", 3);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 2);
	rrProb = cfg.GetFloat("path.russianroulette.prob", 0.5f);

	// Look for OpenCL devices
	for (size_t i = 0; i < intersectionDevices.size(); ++i) {
		if (intersectionDevices[i]->GetType() == DEVICE_TYPE_OPENCL)
			oclIntersectionDevices.push_back((OpenCLIntersectionDevice *)intersectionDevices[i]);
	}

	cerr << "Found "<< oclIntersectionDevices.size() << " OpenCL intersection devices for PathGPU render engine" << endl;

	if (oclIntersectionDevices.size() < 1)
		throw runtime_error("Unable to find an OpenCL intersection device for PathGPU render engine");
}

PathGPURenderEngine::~PathGPURenderEngine() {
	if (started) {
		Interrupt();
		Stop();
	}
}

void PathGPURenderEngine::Start() {
	RenderEngine::Start();
}

void PathGPURenderEngine::Interrupt() {
}

void PathGPURenderEngine::Stop() {
	RenderEngine::Stop();
}

void PathGPURenderEngine::Reset() {
	assert (!started);
}

unsigned int PathGPURenderEngine::GetPass() const {
	return 0;
}

unsigned int PathGPURenderEngine::GetThreadCount() const {
	return oclIntersectionDevices.size();
}

#endif
