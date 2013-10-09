/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include "slg/slg.h"
#include "slg/engines/rtpathocl/rtpathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderEngine
//------------------------------------------------------------------------------

RTPathOCLRenderEngine::RTPathOCLRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
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

	minIterations = cfg.GetInt("rtpath.miniterations", 2);
	displayDeviceIndex = cfg.GetInt("rtpath.displaydevice.index", 0);
	if (displayDeviceIndex >= intersectionDevices.size())
		throw std::runtime_error("Not valid rtpath.displaydevice.index value: " + boost::lexical_cast<std::string>(displayDeviceIndex) +
				" >= " + boost::lexical_cast<std::string>(intersectionDevices.size()));

	PathOCLRenderEngine::StartLockLess();
}

void RTPathOCLRenderEngine::StopLockLess() {
	frameBarrier->wait();
	frameBarrier->wait();
	// All render threads are now suspended and I can set the interrupt signal
	for (size_t i = 0; i < renderThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderThreads[i])->renderThread->interrupt();
	frameBarrier->wait();
	// Render threads will now detect the interruption

	PathOCLRenderEngine::StopLockLess();
}

void RTPathOCLRenderEngine::UpdateFilmLockLess() {
	// Nothing to do: the display thread is in charge to update the film
}

bool RTPathOCLRenderEngine::WaitNewFrame() {
	// Threads do the rendering
	const double t0 = WallClockTime();
	frameBarrier->wait();

	// Display thread merges all frame buffers and does all frame post-processing steps 

	frameBarrier->wait();

	// Re-balance threads
	//SLG_LOG("[RTPathOCLRenderEngine] Load balancing:");
	const double targetFrameTime = renderConfig->GetScreenRefreshInterval() / 1000.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		RTPathOCLRenderThread *t = (RTPathOCLRenderThread *)renderThreads[i];
		if (t->GetFrameTime() > 0.0) {
			//SLG_LOG("[RTPathOCLRenderEngine] Device " << i << ":");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetAssignedIterations() << " assigned iterations");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetAssignedIterations() / t->GetFrameTime() << " iterations/sec");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetFrameTime() * 1000.0 << " msec");

			// Check how far I'm from target frame rate
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

	return true;
}

#endif
