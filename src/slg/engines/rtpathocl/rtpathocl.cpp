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

#include "luxrays/utils/properties.h"
#include <memory>
#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/slg.h"
#include "slg/cameras/perspective.h"
#include "slg/engines/rtpathocl/rtpathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderEngine
//------------------------------------------------------------------------------

RTPathOCLRenderEngine::RTPathOCLRenderEngine(RenderConfigRef rcfg) :
		TilePathOCLRenderEngine(rcfg, false) {
	if (nativeRenderThreadCount > 0)
		throw runtime_error("opencl.native.threads.count must be 0 for RTPATHOCL");

	syncBarrier = new std::barrier(2, completion_t());
	if (renderOCLThreads.size() > 1)
		frameBarrier = new std::barrier(renderOCLThreads.size(), completion_t());
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
	HardwareIntersectionDeviceRef device) {
	return new RTPathOCLRenderThread(index, device, this);
}

void RTPathOCLRenderEngine::StartLockLess() {
	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------
	
	// Disable denoiser statistics collection
	GetFilm().GetDenoiser().SetEnabled(false);

	auto& cfg = renderConfig.GetConfig();

	previewResolutionReduction = RoundUpPow2(Min(Max(1, cfg.Get(GetDefaultProps()->Get("rtpath.resolutionreduction.preview")).Get<int>()), 64));
	previewResolutionReductionStep = Min(Max(1, cfg.Get(GetDefaultProps()->Get("rtpath.resolutionreduction.preview.step")).Get<int>()), 64);

	resolutionReduction = RoundUpPow2(Min(Max(1, cfg.Get(GetDefaultProps()->Get("rtpath.resolutionreduction")).Get<int>()), 64));

	TilePathOCLRenderEngine::StartLockLess();

	// Force to use only 1 tile for each device
	maxTilePerDevice = 1;

	tileRepository->enableRenderingDonePrint = false;
	tileRepository->enableFirstPassClear = true;

	updateActions.Reset();
	useFastCameraEditPath = false;
	cameraIsUsingCustomBokeh = (renderConfig.GetScene().GetCamera().GetType() == Camera::PERSPECTIVE) &&
			(dynamic_cast<const PerspectiveCamera*>(&renderConfig.GetScene().GetCamera()))->bokehDistributionImageMap;

	if (compileOnly)
		// The render threads are not started (so no one will arrive at
		// the barrier): I can not use syncBarrier here
		return;

	// To synchronize the start of all threads
	syncType = SYNCTYPE_NONE;
	syncBarrier->arrive_and_wait();
}

void RTPathOCLRenderEngine::StopLockLess() {
	if (compileOnly) {
		// The render threads have not been started: there is nothing to
		// synchronize (and no renderThread to stop)
		TilePathOCLRenderEngine::StopLockLess();
		return;
	}

	syncType = SYNCTYPE_STOP;
	syncBarrier->arrive_and_wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderOCLThreads[i])->renderThread->request_stop();

	syncType = SYNCTYPE_NONE;
	syncBarrier->arrive_and_wait();

	// Render threads will now detect the interruption

	TilePathOCLRenderEngine::StopLockLess();
}

void RTPathOCLRenderEngine::EndSceneEdit(const EditActionList &editActions) {
	// Check if I can use the fast camera edit path
	if (editActions.HasOnly(CAMERA_EDIT) &&
			(renderConfig.GetScene().GetCamera().GetType() == Camera::PERSPECTIVE) &&
			// Camera is not using custom bokeh
			!(dynamic_cast<const PerspectiveCamera*>(&renderConfig.GetScene().GetCamera()))->bokehDistributionImageMap &&
			// Camera was not using custom bokeh
			!cameraIsUsingCustomBokeh) {
		TilePathOCLRenderEngine::EndSceneEdit(editActions);
		useFastCameraEditPath = true;
	} else {
		syncType = SYNCTYPE_ENDSCENEEDIT;
		updateActions.AddActions(editActions.GetActions());
		syncBarrier->arrive_and_wait();

		TilePathOCLRenderEngine::EndSceneEdit(editActions);
		cameraIsUsingCustomBokeh = (renderConfig.GetScene().GetCamera().GetType() == Camera::PERSPECTIVE) &&
				(dynamic_cast<const PerspectiveCamera*>(&renderConfig.GetScene().GetCamera()))->bokehDistributionImageMap;
		syncBarrier->arrive_and_wait();
		
		// Here, rendering thread 0 will update all OpenCL buffers here

		syncType = SYNCTYPE_NONE;
		syncBarrier->arrive_and_wait();
	}
}

// A fast path for film resize
void RTPathOCLRenderEngine::BeginFilmEdit() {
	syncType = SYNCTYPE_BEGINFILMEDIT;
	syncBarrier->arrive_and_wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderOCLThreads[i])->renderThread->request_stop();

	syncType = SYNCTYPE_NONE;
	syncBarrier->arrive_and_wait();

	// Render threads will now detect the interruption
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->Stop();
}

// A fast path for film resize
void RTPathOCLRenderEngine::EndFilmEdit(FilmRef flm, std::mutex *flmMutex) {
	// Update the film pointer
	film = &flm;
	filmMutex = flmMutex;
	InitFilm();

	// Disable denoiser statistics collection
	GetFilm().GetDenoiser().SetEnabled(false);

	// Create a tile repository based on the new film
	InitTileRepository();
	tileRepository->enableRenderingDonePrint = false;
	tileRepository->enableFirstPassClear = true;

	// The camera has been updated too
	EditActionList a;
	a.AddActions(CAMERA_EDIT);
	compiledScene->Recompile(a);

	// Re-start all rendering threads
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->Start();

	// To synchronize the start of all threads
	syncType = SYNCTYPE_NONE;
	syncBarrier->arrive_and_wait();
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

PropertiesUPtr RTPathOCLRenderEngine::ToProperties(const Properties &cfg) {
	auto props_ptr = std::make_unique<Properties>();
	auto& props = *props_ptr;

	props <<
			TilePathOCLRenderEngine::ToProperties(cfg) <<
			//------------------------------------------------------------------
			// Overwrite some TilePathOCLRenderEngine property
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps()->Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.total")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps()->Get("path.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps()->Get("tilepath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps()->Get("tilepathocl.devices.maxtiles")) <<
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps()->Get("rtpath.resolutionreduction.preview")) <<
			cfg.Get(GetDefaultProps()->Get("rtpath.resolutionreduction.preview.step")) <<
			cfg.Get(GetDefaultProps()->Get("rtpath.resolutionreduction"));
	return props_ptr;
}

RenderEngine *RTPathOCLRenderEngine::FromProperties(RenderConfigRef rcfg) {
	return new RTPathOCLRenderEngine(rcfg);
}

PropertiesUPtr RTPathOCLRenderEngine::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
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
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
