/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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
#include <memory>
#include "slg/engines/rtpathcpu/rtpathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathCPURenderEngine
//------------------------------------------------------------------------------

RTPathCPURenderEngine::RTPathCPURenderEngine(RenderConfigRef rcfg) :
		PathCPURenderEngine(rcfg) {
	threadsSyncBarrier = new std::barrier(renderThreads.size() + 1, completion_t());
}

RTPathCPURenderEngine::~RTPathCPURenderEngine() {
	delete threadsSyncBarrier;
}

void RTPathCPURenderEngine::StartLockLess() {
	auto& cfg = renderConfig.GetConfig();
	zoomFactor = (u_int)Max(1, cfg.Get(GetDefaultProps()->Get("rtpathcpu.zoomphase.size")).Get<int>());
	zoomWeight = Max(0.0001, cfg.Get(GetDefaultProps()->Get("rtpathcpu.zoomphase.weight")).Get<double>());

	threadsPauseMode = false;
	firstFrameDone = false;
	firstFrameThreadDoneCount = 0;

	PathCPURenderEngine::StartLockLess();
}

void RTPathCPURenderEngine::StopLockLess() {
	// We need to resume paused sessions first,
	// otherwise it will hang on a thread barrier.
	if(pauseMode)
		Resume();
	PathCPURenderEngine::StopLockLess();
}

void RTPathCPURenderEngine::WaitNewFrame() {
	if (!firstFrameDone && !pauseMode && !editMode) {
		// Wait for the signal from all the rendering threads
		std::unique_lock<std::mutex> lock(firstFrameMutex);
		while (firstFrameThreadDoneCount < renderThreads.size())
			firstFrameCondition.wait(lock);

		firstFrameDone = true;
	}
}

void RTPathCPURenderEngine::PauseThreads() {
	// Tell the threads to pause the rendering
	threadsPauseMode = true;

	// Wait for the threads
	threadsSyncBarrier->arrive_and_wait();
}

void RTPathCPURenderEngine::ResumeThreads() {
	threadsPauseMode = false;
	firstFrameDone = false;
	firstFrameThreadDoneCount = 0;

	// Let's the threads to resume the rendering
	threadsSyncBarrier->arrive_and_wait();
}

void RTPathCPURenderEngine::Pause() {
	PathCPURenderEngine::Pause();

	PauseThreads();
}

void RTPathCPURenderEngine::Resume() {
	PathCPURenderEngine::Resume();

	ResumeThreads();	
}

void RTPathCPURenderEngine::BeginSceneEditLockLess() {
	// Check if the threads are already suspended for pause
	if (!pauseMode)
		PauseThreads();
}

void RTPathCPURenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	GetFilm().Reset();
	samplerSharedData->Reset();

	// Check if the threads were already suspended for pause
	if (!pauseMode)
		ResumeThreads();
}

void RTPathCPURenderEngine::UpdateFilmLockLess() {
	// Nothing to do because render threads uses directly the engine Film
}

// A fast path for film resize
void RTPathCPURenderEngine::BeginFilmEdit() {
	// Check if the threads are already suspended for pause
	if (!pauseMode)
		PauseThreads();
}

// A fast path for film resize
void RTPathCPURenderEngine::EndFilmEdit(FilmRef flm, std::mutex *flmMutex) {
	// Update the film pointer
	film = &flm;
	filmMutex = flmMutex;
	InitFilm();

	static_cast<RTPathCPUSamplerSharedData *>(samplerSharedData.get())->Reset( GetFilmPtr());

	// Check if the threads were already suspended for pause
	if (!pauseMode)
		ResumeThreads();
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

PropertiesUPtr RTPathCPURenderEngine::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = PathCPURenderEngine::ToProperties(cfg);
	
	*props <<
				//------------------------------------------------------------------
			// Overwrite some PathCPURenderEngine property
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps()->Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.total")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.specular")) <<
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps()->Get("rtpathcpu.zoomphase.size")) <<
			cfg.Get(GetDefaultProps()->Get("rtpathcpu.zoomphase.weight"));
	
	return props;
}

RenderEngine *RTPathCPURenderEngine::FromProperties(RenderConfigRef rcfg) {
	return new RTPathCPURenderEngine(rcfg);
}

PropertiesUPtr RTPathCPURenderEngine::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			PathCPURenderEngine::GetDefaultProps() <<
			//------------------------------------------------------------------
			// Overwrite some PathCPURenderEngine property
			//------------------------------------------------------------------
			Property("renderengine.type")(GetObjectTag()) <<
			Property("path.pathdepth.total")(5) <<
			Property("path.pathdepth.diffuse")(3) <<
			Property("path.pathdepth.glossy")(3) <<
			Property("path.pathdepth.specular")(3) <<
			//------------------------------------------------------------------
			Property("rtpathcpu.zoomphase.size")(4) <<
			Property("rtpathcpu.zoomphase.weight")(.1f);

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
