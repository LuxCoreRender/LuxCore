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
	frameBarrier = new boost::barrier(renderThreads.size() + 1);
}

RTPathOCLRenderEngine::~RTPathOCLRenderEngine() {
}

PathOCLRenderThread *RTPathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new RTPathOCLRenderThread(index, device, this);
}

bool RTPathOCLRenderEngine::WaitNewFrame() {
	// Re-balance threads
	const double startTime = WallClockTime();
	frameBarrier->wait();

	//SLG_LOG("[RTPathOCLRenderEngine] Load balancing:");
	const double targetFrameTime = renderConfig->GetScreenRefreshInterval() / 1000.0;
	const u_int minIteration = renderConfig->GetMinIterationsToShow();
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		RTPathOCLRenderThread *t = (RTPathOCLRenderThread *)renderThreads[i];
		if (t->GetFrameTime() > 0.0) {
			//SLG_LOG("[RTPathOCLRenderEngine] Device " << i << ":");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetAssignedIterations() / t->GetFrameTime() << " iterations/sec");
			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetFrameTime() * 1000.0 << " msec");

			// Check how far I'm from target frame rate
			if (t->GetFrameTime() < targetFrameTime) {
				// Too fast, increase the number of iterations
				t->SetAssignedIterations(Max(t->GetAssignedIterations() + 1, minIteration));
			} else {
				// Too slow, decrease the number of iterations
				t->SetAssignedIterations(Max(t->GetAssignedIterations() - 1, minIteration));
			}

			//SLG_LOG("[RTPathOCLRenderEngine]   " << t->GetAssignedIterations() << " iterations");
		}
	}

	UpdateFilm();
	frameBarrier->wait();

	frameTime = WallClockTime() - startTime;

	return true;
}

}

#endif
