/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/slg.h"
#include "slg/engines/rtpathocl/rtpathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderEngine
//------------------------------------------------------------------------------

RTPathOCLRenderEngine::RTPathOCLRenderEngine(const RenderConfig *rcfg) :
		TilePathOCLRenderEngine(rcfg, false) {
	if (nativeRenderThreadCount > 0)
		throw runtime_error("opencl.native.threads.count must be 0 for RTPATHOCL");

	syncBarrier = new boost::barrier(2);
	if (renderOCLThreads.size() > 1)
		frameBarrier = new boost::barrier(renderOCLThreads.size());
	else
		frameBarrier = nullptr;

	frameTime = 0.f;
}

RTPathOCLRenderEngine::~RTPathOCLRenderEngine() {
	delete frameBarrier;
}

void RTPathOCLRenderEngine::InitGPUTaskConfiguration() {
	TilePathOCLRenderEngine::InitGPUTaskConfiguration();

	taskConfig.renderEngine.rtpathocl.previewResolutionReduction = previewResolutionReduction;
	taskConfig.renderEngine.rtpathocl.previewResolutionReductionStep = previewResolutionReductionStep;
	taskConfig.renderEngine.rtpathocl.resolutionReduction = resolutionReduction;
}

PathOCLBaseOCLRenderThread *RTPathOCLRenderEngine::CreateOCLThread(const u_int index,
	HardwareIntersectionDevice *device) {
	return new RTPathOCLRenderThread(index, device, this);
}

void RTPathOCLRenderEngine::StartLockLess() {
	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------
	
	// Disable denoiser statistics collection
	film->GetDenoiser().SetEnabled(false);

	const Properties &cfg = renderConfig->cfg;

	previewResolutionReduction = RoundUpPow2(Min(Max(1, cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview")).Get<int>()), 64));
	previewResolutionReductionStep = Min(Max(1, cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview.step")).Get<int>()), 64);

	resolutionReduction = RoundUpPow2(Min(Max(1, cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction")).Get<int>()), 64));

	TilePathOCLRenderEngine::StartLockLess();

	// Force to use only 1 tile for each device
	maxTilePerDevice = 1;

	tileRepository->enableRenderingDonePrint = false;
	tileRepository->enableFirstPassClear = true;

	// To synchronize the start of all threads
	syncType = SYNCTYPE_NONE;
	syncBarrier->wait();
}

void RTPathOCLRenderEngine::StopLockLess() {
	syncType = SYNCTYPE_STOP;
	syncBarrier->wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderOCLThreads[i])->renderThread->interrupt();

	syncType = SYNCTYPE_NONE;
	syncBarrier->wait();

	// Render threads will now detect the interruption

	TilePathOCLRenderEngine::StopLockLess();
}

void RTPathOCLRenderEngine::PauseThreads() {
	syncType = SYNCTYPE_PAUSEMODE;
	syncBarrier->wait();
}

void RTPathOCLRenderEngine::ResumeThreads() {
	syncType = SYNCTYPE_NONE;
	syncBarrier->wait();
}

void RTPathOCLRenderEngine::Pause() {
	TilePathOCLRenderEngine::Pause();

	PauseThreads();
}

void RTPathOCLRenderEngine::Resume() {
	TilePathOCLRenderEngine::Resume();

	ResumeThreads();	
}

void RTPathOCLRenderEngine::EndSceneEdit(const EditActionList &editActions) {
	TilePathOCLRenderEngine::EndSceneEdit(editActions);
	updateActions.AddActions(editActions.GetActions());

	const bool requireSync = editActions.HasAnyAction() && !editActions.HasOnly(CAMERA_EDIT);
	
	if (requireSync) {
		syncType = SYNCTYPE_ENDSCENEEDIT;
		syncBarrier->wait();

		syncType = SYNCTYPE_NONE;
		syncBarrier->wait();
	}
}

// A fast path for film resize
void RTPathOCLRenderEngine::BeginFilmEdit() {
	syncType = SYNCTYPE_BEGINFILMEDIT;
	syncBarrier->wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderOCLThreads[i])->renderThread->interrupt();

	syncType = SYNCTYPE_NONE;
	syncBarrier->wait();

	// Render threads will now detect the interruption
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->Stop();
}

// A fast path for film resize
void RTPathOCLRenderEngine::EndFilmEdit(Film *flm, boost::mutex *flmMutex) {
	// Update the film pointer
	film = flm;
	filmMutex = flmMutex;
	InitFilm();

	// Disable denoiser statistics collection
	film->GetDenoiser().SetEnabled(false);

	// Create a tile repository based on the new film
	InitTileRepository();
	tileRepository->enableRenderingDonePrint = false;

	// The camera has been updated too
	EditActionList a;
	a.AddActions(CAMERA_EDIT);
	compiledScene->Recompile(a);

	// Re-start all rendering threads
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->Start();

	// To synchronize the start of all threads
	syncType = SYNCTYPE_NONE;
	syncBarrier->wait();
}

void RTPathOCLRenderEngine::UpdateFilmLockLess() {
	// Nothing to do: the render threads are in charge of updating the film
}

void RTPathOCLRenderEngine::WaitNewFrame() {
	// Avoid to move forward rendering threads if I'm in pause
	if (!pauseMode) {
		// Update the statistics
		UpdateCounters();
	}
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties RTPathOCLRenderEngine::ToProperties(const Properties &cfg) {
	return TilePathOCLRenderEngine::ToProperties(cfg) <<
			//------------------------------------------------------------------
			// Overwrite some TilePathOCLRenderEngine property
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("path.pathdepth.total")) <<
			cfg.Get(GetDefaultProps().Get("path.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps().Get("path.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps().Get("path.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepathocl.devices.maxtiles")) <<
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview")) <<
			cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview.step")) <<
			cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction"));
}

RenderEngine *RTPathOCLRenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new RTPathOCLRenderEngine(rcfg);
}

const Properties &RTPathOCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			TilePathOCLRenderEngine::GetDefaultProps() <<
			//------------------------------------------------------------------
			// Overwrite some TilePathOCLRenderEngine property
			//------------------------------------------------------------------
			Property("renderengine.type")(GetObjectTag()) <<
			Property("path.pathdepth.total")(5) <<
			Property("path.pathdepth.diffuse")(3) <<
			Property("path.pathdepth.glossy")(3) <<
			Property("path.pathdepth.specular")(3) <<
			Property("tilepath.sampling.aa.size")(1) <<
			Property("tilepathocl.devices.maxtiles")(1) <<
			//------------------------------------------------------------------
			Property("rtpath.resolutionreduction.preview")(4) <<
			Property("rtpath.resolutionreduction.preview.step")(8) <<
			Property("rtpath.resolutionreduction")(4);

	return props;
}

#endif
