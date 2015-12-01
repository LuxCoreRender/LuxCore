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
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTBiasPathOCLRenderEngine
//------------------------------------------------------------------------------

RTBiasPathOCLRenderEngine::RTBiasPathOCLRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		BiasPathOCLRenderEngine(rcfg, flm, flmMutex) {
	frameBarrier = new boost::barrier(renderThreads.size() + 1);
	frameStartTime = 0.f;
	frameTime = 0.f;
}

RTBiasPathOCLRenderEngine::~RTBiasPathOCLRenderEngine() {
	delete frameBarrier;
}

BiasPathOCLRenderThread *RTBiasPathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new RTBiasPathOCLRenderThread(index, device, this);
}

void RTBiasPathOCLRenderEngine::StartLockLess() {
	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	BiasPathOCLRenderEngine::StartLockLess();

	tileRepository->enableRenderingDonePrint = false;

	// To synchronize the start of all threads
	frameBarrier->wait();
}

void RTBiasPathOCLRenderEngine::StopLockLess() {
	frameBarrier->wait();
	frameBarrier->wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderThreads.size(); ++i)
		((RTBiasPathOCLRenderThread *)renderThreads[i])->renderThread->interrupt();

	frameBarrier->wait();

	// Render threads will now detect the interruption

	BiasPathOCLRenderEngine::StopLockLess();
}

void RTBiasPathOCLRenderEngine::EndSceneEdit(const EditActionList &editActions) {
	const bool requireSync = editActions.HasAnyAction() && !editActions.HasOnly(CAMERA_EDIT);

	if (requireSync) {
		// This is required to move the rendering thread forward
		frameBarrier->wait();
	}

	// While threads splat their tiles on the film I can finish the scene edit
	BiasPathOCLRenderEngine::EndSceneEdit(editActions);
	updateActions.AddActions(editActions.GetActions());

	if (requireSync) {
		// This is required to move the rendering thread forward
		frameBarrier->wait();

		// Re-initialize the tile queue for the next frame
		tileRepository->Restart();

		frameBarrier->wait();
	}
}

void RTBiasPathOCLRenderEngine::UpdateFilmLockLess() {
	// Nothing to do: the display thread is in charge of updating the film
}

void RTBiasPathOCLRenderEngine::WaitNewFrame() {
	// Threads do the rendering

	frameBarrier->wait();

	// Threads splat their tiles on the film

	frameBarrier->wait();

	// Re-initialize the tile queue for the next frame
	tileRepository->Restart();

	frameBarrier->wait();

	// Update the statistics
	UpdateCounters();

	const double currentTime = WallClockTime();
	frameTime = currentTime - frameStartTime;
	frameStartTime = currentTime;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties RTBiasPathOCLRenderEngine::ToProperties(const Properties &cfg) {
	return BiasPathOCLRenderEngine::ToProperties(cfg) <<
			//------------------------------------------------------------------
			// Overwrite some BiasPathOCLRenderEngine property
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.total")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.diffuse.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.glossy.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.specular.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.directlight.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.lights.firstvertexsamples")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.devices.maxtiles")) <<
			//------------------------------------------------------------------
			cfg.Get(GetDefaultProps().Get("rtpath.miniterations"));
}

RenderEngine *RTBiasPathOCLRenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new RTBiasPathOCLRenderEngine(rcfg, flm, flmMutex);
}

Properties RTBiasPathOCLRenderEngine::GetDefaultProps() {
	static Properties props = BiasPathOCLRenderEngine::GetDefaultProps() <<
			//------------------------------------------------------------------
			// Overwrite some BiasPathOCLRenderEngine property
			//------------------------------------------------------------------
			Property("renderengine.type")(GetObjectTag()) <<
			Property("biaspath.pathdepth.total")(5) <<
			Property("biaspath.pathdepth.diffuse")(3) <<
			Property("biaspath.pathdepth.glossy")(3) <<
			Property("biaspath.pathdepth.specular")(3) <<
			Property("biaspath.sampling.aa.size")(1) <<
			Property("biaspath.sampling.diffuse.size")(1) <<
			Property("biaspath.sampling.glossy.size")(1) <<
			Property("biaspath.sampling.specular.size")(1) <<
			Property("biaspath.sampling.directlight.size")(1) <<
			Property("biaspath.lights.firstvertexsamples")(1) <<
			Property("biaspath.devices.maxtiles")(1) <<
			//------------------------------------------------------------------
			Property("rtpath.miniterations")(2);

	return props;
}

#endif
