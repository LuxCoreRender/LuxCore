/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include "slg/slg.h"
#include "slg/samplers/rtpathcpusampler.h"
#include "slg/engines/rtpathcpu/rtpathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathCPURenderEngine
//------------------------------------------------------------------------------

RTPathCPURenderEngine::RTPathCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		PathCPURenderEngine(rcfg, flm, flmMutex) {
	editSyncBarrier = new boost::barrier(renderThreads.size() + 1);
}

RTPathCPURenderEngine::~RTPathCPURenderEngine() {
	delete editSyncBarrier;
}

void RTPathCPURenderEngine::StartLockLess() {
	beginEditMode = false;
	firstFrameDone = false;
	firstFrameThreadDoneCount = 0;

	PathCPURenderEngine::StartLockLess();
}

void RTPathCPURenderEngine::StopLockLess() {
	PathCPURenderEngine::StopLockLess();
}

void RTPathCPURenderEngine::WaitNewFrame() {
	if (!firstFrameDone) {
		// Wait for the signal from all the rendering threads
		boost::unique_lock<boost::mutex> lock(firstFrameMutex);
		while (firstFrameThreadDoneCount < renderThreads.size())
			firstFrameCondition.wait(lock);

		firstFrameDone = true;
	}
}

void RTPathCPURenderEngine::BeginSceneEditLockLess() {
	// Tell the threads to pause the rendering
	beginEditMode = true;

	// Wait for the threads
	editSyncBarrier->wait();
}

void RTPathCPURenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	beginEditMode = false;
	firstFrameDone = false;
	firstFrameThreadDoneCount = 0;
	
	film->Reset();
	((RTPathCPUSamplerSharedData *)samplerSharedData)->Reset();

	// Let's the threads to resume the rendering
	editSyncBarrier->wait();
}

void RTPathCPURenderEngine::UpdateFilmLockLess() {
	// Nothing to do because render threads uses directly the engine Film
}

// A fast path for film resize
void RTPathCPURenderEngine::BeginFilmEdit() {
	// Tell the threads to pause the rendering
	beginEditMode = true;

	// Wait for the threads
	editSyncBarrier->wait();
}

// A fast path for film resize
void RTPathCPURenderEngine::EndFilmEdit(Film *flm) {
	// Update the film pointer
	film = flm;
	InitFilm();

	beginEditMode = false;
	firstFrameDone = false;
	firstFrameThreadDoneCount = 0;

	((RTPathCPUSamplerSharedData *)samplerSharedData)->Reset();

	// Let's the threads to resume the rendering
	editSyncBarrier->wait();
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties RTPathCPURenderEngine::ToProperties(const Properties &cfg) {
	return PathCPURenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type"));
}

RenderEngine *RTPathCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new RTPathCPURenderEngine(rcfg, flm, flmMutex);
}

const Properties &RTPathCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			PathCPURenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag());

	return props;
}
