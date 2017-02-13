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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/slg.h"
#include "slg/engines/rtpathocl/rtpathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderEngine
//------------------------------------------------------------------------------

RTPathOCLRenderEngine::RTPathOCLRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		TilePathOCLRenderEngine(rcfg, flm, flmMutex) {
	frameBarrier = new boost::barrier(renderThreads.size() + 1);
	frameStartTime = 0.f;
	frameTime = 0.f;
}

RTPathOCLRenderEngine::~RTPathOCLRenderEngine() {
	delete frameBarrier;
}

PathOCLBaseRenderThread *RTPathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new RTPathOCLRenderThread(index, device, this);
}

void RTPathOCLRenderEngine::StartLockLess() {
	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	const Properties &cfg = renderConfig->cfg;

	previewResolutionReduction = RoundUpPow2(Min(Max(1, cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview")).Get<int>()), 64));
	previewResolutionReductionStep = Min(Max(1, cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview.step")).Get<int>()), 64);

	resolutionReduction = RoundUpPow2(Min(Max(1, cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction")).Get<int>()), 64));

	TilePathOCLRenderEngine::StartLockLess();

	tileRepository->enableRenderingDonePrint = false;
	frameCounter = 0;

	// To synchronize the start of all threads
	frameBarrier->wait();
}

void RTPathOCLRenderEngine::StopLockLess() {
	frameBarrier->wait();
	frameBarrier->wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderThreads[i])->renderThread->interrupt();

	frameBarrier->wait();

	// Render threads will now detect the interruption

	TilePathOCLRenderEngine::StopLockLess();
}

void RTPathOCLRenderEngine::EndSceneEdit(const EditActionList &editActions) {
	const bool requireSync = editActions.HasAnyAction() && !editActions.HasOnly(CAMERA_EDIT);

	if (requireSync) {
		// This is required to move the rendering thread forward
		frameBarrier->wait();
	}

	// While threads splat their tiles on the film I can finish the scene edit
	TilePathOCLRenderEngine::EndSceneEdit(editActions);
	updateActions.AddActions(editActions.GetActions());

	frameCounter = 0;

	if (requireSync) {
		// This is required to move the rendering thread forward
		frameBarrier->wait();

		// Re-initialize the tile queue for the next frame
		tileRepository->Restart(frameCounter);

		frameBarrier->wait();
	}
}

// A fast path for film resize
void RTPathOCLRenderEngine::BeginFilmEdit() {
	frameBarrier->wait();
	frameBarrier->wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderThreads[i])->renderThread->interrupt();

	frameBarrier->wait();

	// Render threads will now detect the interruption
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

// A fast path for film resize
void RTPathOCLRenderEngine::EndFilmEdit(Film *flm) {
	// Update the film pointer
	film = flm;
	InitFilm();

	frameCounter = 0;

	// Create a tile repository based on the new film
	InitTileRepository();
	tileRepository->enableRenderingDonePrint = false;

	// The camera has been updated too
	EditActionList a;
	a.AddActions(CAMERA_EDIT);
	compiledScene->Recompile(a);

	// Re-start all rendering threads
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();

	// To synchronize the start of all threads
	frameBarrier->wait();
}

void RTPathOCLRenderEngine::UpdateFilmLockLess() {
	// Nothing to do: the display thread is in charge of updating the film
}

void RTPathOCLRenderEngine::WaitNewFrame() {
	// Avoid to move forward rendering threads if I'm in pause
	if (!pauseMode) {
		// Threads do the rendering

		frameBarrier->wait();

		// Threads splat their tiles on the film

		frameBarrier->wait();

		// Re-initialize the tile queue for the next frame
		tileRepository->Restart(frameCounter++);

		frameBarrier->wait();

		// Update the statistics
		UpdateCounters();

		const double currentTime = WallClockTime();
		frameTime = currentTime - frameStartTime;
		frameStartTime = currentTime;
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
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.total")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.devices.maxtiles")) <<
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview")) <<
			cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction.preview.step")) <<
			cfg.Get(GetDefaultProps().Get("rtpath.resolutionreduction"));
}

RenderEngine *RTPathOCLRenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new RTPathOCLRenderEngine(rcfg, flm, flmMutex);
}

const Properties &RTPathOCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			TilePathOCLRenderEngine::GetDefaultProps() <<
			//------------------------------------------------------------------
			// Overwrite some TilePathOCLRenderEngine property
			//------------------------------------------------------------------
			Property("renderengine.type")(GetObjectTag()) <<
			Property("tilepath.pathdepth.total")(5) <<
			Property("tilepath.pathdepth.diffuse")(3) <<
			Property("tilepath.pathdepth.glossy")(3) <<
			Property("tilepath.pathdepth.specular")(3) <<
			Property("tilepath.sampling.aa.size")(1) <<
			Property("tilepath.devices.maxtiles")(1) <<
			//------------------------------------------------------------------
			Property("rtpath.resolutionreduction.preview")(4) <<
			Property("rtpath.resolutionreduction.preview.step")(8) <<
			Property("rtpath.resolutionreduction")(4);

	return props;
}

#endif
