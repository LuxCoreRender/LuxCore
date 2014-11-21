/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
		BiasPathOCLRenderEngine(rcfg, flm, flmMutex, true) {
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
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	displayDeviceIndex = cfg.Get(Property("rtpath.displaydevice.index")(0)).Get<u_int>();
	if (displayDeviceIndex >= intersectionDevices.size())
		throw std::runtime_error("Not valid rtpath.displaydevice.index value: " + boost::lexical_cast<std::string>(displayDeviceIndex) +
				" >= " + boost::lexical_cast<std::string>(intersectionDevices.size()));

	blurTimeWindow = Max(0.f, cfg.Get(Property("rtpath.blur.timewindow")(3.f)).Get<float>());
	blurMinCap =  Max(0.f, cfg.Get(Property("rtpath.blur.mincap")(0.01f)).Get<float>());
	blurMaxCap =  Max(0.f, cfg.Get(Property("rtpath.blur.maxcap")(0.2f)).Get<float>());

	ghostEffect = 1.f - Clamp(cfg.Get(Property("rtpath.ghosteffect.intensity")(0.05f)).Get<float>(), 0.f, 1.f);

	BiasPathOCLRenderEngine::StartLockLess();

	tileRepository->enableRenderingDonePrint = false;
}

void RTBiasPathOCLRenderEngine::StopLockLess() {
	frameBarrier->wait();

	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderThreads.size(); ++i)
		((RTBiasPathOCLRenderThread *)renderThreads[i])->renderThread->interrupt();

	frameBarrier->wait();

	// Render threads will now detect the interruption

	BiasPathOCLRenderEngine::StopLockLess();
}

void RTBiasPathOCLRenderEngine::BeginSceneEdit() {
	BiasPathOCLRenderEngine::BeginSceneEdit();
}

void RTBiasPathOCLRenderEngine::EndSceneEdit(const EditActionList &editActions) {
	const bool requireSync = editActions.HasAnyAction() && !editActions.HasOnly(CAMERA_EDIT);

	editMutex.lock();
	if (requireSync) {
		// This is required to move the rendering thread forward
		frameBarrier->wait();
		editCanStart.wait(editMutex);
	}

	BiasPathOCLRenderEngine::EndSceneEdit(editActions);

	updateActions.AddActions(editActions.GetActions());
	editMutex.unlock();

	if (requireSync) {
		// This is required to move the rendering thread forward
		frameBarrier->wait();
	}
}

void RTBiasPathOCLRenderEngine::UpdateFilmLockLess() {
	// Nothing to do: the display thread is in charge of updating the film
}

void RTBiasPathOCLRenderEngine::WaitNewFrame() {
	// Threads do the rendering

	frameBarrier->wait();

	// Display thread does all frame post-processing steps 

	// Re-initialize the tile queue for the next frame
	tileRepository->InitTiles(film);

	frameBarrier->wait();

	// Update the statistics
	UpdateCounters();

	const double currentTime = WallClockTime();
	frameTime = currentTime - frameStartTime;
	frameStartTime = currentTime;
}

#endif
