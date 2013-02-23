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

#include "slg.h"
#include "pathocl/rtpathocl.h"

using namespace std;

namespace slg {

//------------------------------------------------------------------------------
// RTPathOCLRenderEngine
//------------------------------------------------------------------------------

RTPathOCLRenderEngine::RTPathOCLRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		PathOCLRenderEngine(rcfg, flm, flmMutex) {
	// Re-initialize the file in order to not have any sample buffer
	film->SetPerPixelNormalizedBufferFlag(false);
	film->SetPerScreenNormalizedBufferFlag(false);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->Init();

	frameBarrier = new boost::barrier(renderThreads.size() + 1);
}

RTPathOCLRenderEngine::~RTPathOCLRenderEngine() {
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

	// To start all threads at the same time
	frameBarrier->wait();
}

void RTPathOCLRenderEngine::EndEditLockLess(const EditActionList &editActions) {
	PathOCLRenderEngine::EndEditLockLess(editActions);

	if (editActions.Has(FILM_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT)) {
		// In this particular case I have really stopped all threads
		frameBarrier->wait();
	}
}

void RTPathOCLRenderEngine::UpdateFilmLockLess() {
	// Nothing to do: the display thread is in charge to update the film
}

bool RTPathOCLRenderEngine::WaitNewFrame() {
	// Threads do the rendering
	const double startTime = WallClockTime();
	frameBarrier->wait();

	// Display thread merges all frame buffers and do all frame post-processing steps 

	frameBarrier->wait();

	// Re-balance threads
	//SLG_LOG("[RTPathOCLRenderEngine] Load balancing:");
	const double targetFrameTime = renderConfig->GetScreenRefreshInterval() / 1000.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		RTPathOCLRenderThread *t = (RTPathOCLRenderThread *)renderThreads[i];
		if (t->GetFrameTime() > 0.0) {
			//SLG_LOG("[RTPathOCLRenderEngine] Device " << i << ":");
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

	frameTime = WallClockTime() - startTime;

	return true;
}

}

#endif
