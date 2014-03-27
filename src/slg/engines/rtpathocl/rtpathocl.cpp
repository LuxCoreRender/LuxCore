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
#include "slg/engines/rtpathocl/rtpathocl.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderEngine
//------------------------------------------------------------------------------

RTPathOCLRenderEngine::RTPathOCLRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		PathOCLRenderEngine(rcfg, flm, flmMutex, true) {
	frameBarrier = new boost::barrier(renderThreads.size() + 1);
	frameTime = 0.f;
}

RTPathOCLRenderEngine::~RTPathOCLRenderEngine() {
	delete frameBarrier;
}

PathOCLRenderThread *RTPathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new RTPathOCLRenderThread(index, device, this);
}

void RTPathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	minIterations = cfg.Get(Property("rtpath.miniterations")(2)).Get<u_int>();
	displayDeviceIndex = cfg.Get(Property("rtpath.displaydevice.index")(0)).Get<u_int>();
	if (displayDeviceIndex >= intersectionDevices.size())
		throw std::runtime_error("Not valid rtpath.displaydevice.index value: " + boost::lexical_cast<std::string>(displayDeviceIndex) +
				" >= " + boost::lexical_cast<std::string>(intersectionDevices.size()));

	blurTimeWindow = Max(0.f, cfg.Get(Property("rtpath.blur.timewindow")(3.f)).Get<float>());
	blurMinCap =  Max(0.f, cfg.Get(Property("rtpath.blur.mincap")(0.01f)).Get<float>());
	blurMaxCap =  Max(0.f, cfg.Get(Property("rtpath.blur.maxcap")(0.2f)).Get<float>());

	ghostEffect = 1.f - Clamp(cfg.Get(Property("rtpath.ghosteffect.intensity")(0.05f)).Get<float>(), 0.f, 1.f);

	PathOCLRenderEngine::StartLockLess();
}

void RTPathOCLRenderEngine::StopLockLess() {
	frameBarrier->wait();
	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderThreads[i])->renderThread->interrupt();
	frameBarrier->wait();
	// Render threads will now detect the interruption

	PathOCLRenderEngine::StopLockLess();
}

void RTPathOCLRenderEngine::BeginSceneEdit() {
	// NOTE: this is a huge trick, the LuxRays context is stopped by RenderEngine
	// but the threads are still using the intersection devices in RTPATHOCL.
	// The result is that stuff like geometry edit will not work.
	editMutex.lock();

	PathOCLRenderEngine::BeginSceneEdit();
}

void RTPathOCLRenderEngine::EndSceneEdit(const EditActionList &editActions) {
	PathOCLRenderEngine::EndSceneEdit(editActions);

	updateActions.AddActions(editActions.GetActions());
	editMutex.unlock();
}

void RTPathOCLRenderEngine::UpdateFilmLockLess() {
	// Nothing to do: the display thread is in charge of updating the film
}

void RTPathOCLRenderEngine::WaitNewFrame() {
	// Threads do the rendering
	const double t0 = WallClockTime();
	frameBarrier->wait();

	// Display thread merges all frame buffers and does all frame post-processing steps 

	// Re-balance threads
	//SLG_LOG("[RTPathOCLRenderEngine] Load balancing:");
	const double targetFrameTime = renderConfig->GetProperty("screen.refresh.interval").Get<u_int>() / 1000.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		RTPathOCLRenderThread *t = (RTPathOCLRenderThread *)renderThreads[i];
		if (t->GetFrameTime() > 0.0) {
			//SLG_LOG("[RTPathOCLRenderEngine] Device " << i << ":");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetAssignedIterations() << " assigned iterations");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetAssignedIterations() / t->GetFrameTime() << " iterations/sec");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetFrameTime() * 1000.0 << " msec");

			// Check how far I am from target frame rate
			if (t->GetFrameTime() < targetFrameTime) {
				// Too fast, increase the number of iterations
				t->SetAssignedIterations(Max(t->GetAssignedIterations() + 1, minIterations));
			} else {
				// Too slow, decrease the number of iterations
				t->SetAssignedIterations(Max(t->GetAssignedIterations() - 1, minIterations));
			}

			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetAssignedIterations() << " iterations");
		}
	}

	frameBarrier->wait();

	// Update the statistics
	UpdateCounters();

	frameTime = WallClockTime() - t0;
}

#endif
