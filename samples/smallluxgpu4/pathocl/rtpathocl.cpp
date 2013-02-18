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

bool RTPathOCLRenderEngine::WaitNewFrame()
{
	frameBarrier->wait();
	// re-balance threads
	double minSampleTime = 1000;
	size_t fatest_dev = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		RTPathOCLRenderThread *t = (RTPathOCLRenderThread *)renderThreads[i];
		t->balance_lock();
		const double sampleTime = t->GetFrameTime() * taskCount / t->GetAssignedTaskCount();
		if (minSampleTime > sampleTime) {
			minSampleTime = sampleTime;
			fatest_dev = i;
		}
	}
	//printf("balance:");
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		RTPathOCLRenderThread *t = (RTPathOCLRenderThread *)renderThreads[i];
		if (i == fatest_dev) {
			t->SetAssignedTaskCount(taskCount);
			//printf(" %d(%.3f)", t->GetAssignedTaskCount(), t->GetFrameTime());
			continue;
			}
			double sampleTime = t->GetFrameTime() * taskCount / t->GetAssignedTaskCount();
			t->SetAssignedTaskCount(taskCount * minSampleTime / sampleTime);
		//printf(" %d(%.3f)", t->GetAssignedTaskCount(), t->GetFrameTime());
	}
	//printf("\n");

	UpdateFilm();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		((RTPathOCLRenderThread *)renderThreads[i])->balance_unlock();

	return true;
}

}

#endif
