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

void RTPathOCLRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	const u_int imgWidth = film->GetWidth();
	const u_int imgHeight = film->GetHeight();

	film->Reset();

	RTPathOCLRenderThread *renderThread = (RTPathOCLRenderThread *)renderThreads[displayDeviceIndex];
	for (u_int y = 0; y < imgHeight; ++y) {
		u_int pGPU = 1 + (y + 1) * (imgWidth + 2);

		for (u_int x = 0; x < imgWidth; ++x) {
			Spectrum radiance;
			float alpha = 0.0f;
			float count = 0.f;

			if (renderThread->frameBuffer) {
				radiance.r = renderThread->frameBuffer[pGPU].c.r;
				radiance.g = renderThread->frameBuffer[pGPU].c.g;
				radiance.b = renderThread->frameBuffer[pGPU].c.b;
				count = renderThread->frameBuffer[pGPU].count;
			}

			if (renderThread->alphaFrameBuffer)
				alpha = renderThread->alphaFrameBuffer[pGPU].alpha;

			if ((count > 0) && !radiance.IsNaN()) {
				film->AddSampleCount(1.f);
				// -.5f is to align correctly the pixel after the splat
				film->SplatFiltered(PER_PIXEL_NORMALIZED, x - .5f, y - .5f,
						radiance / count, isnan(alpha) ? 0.f : alpha / count, count);
			}

			++pGPU;
		}
	}
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

	UpdateFilm();

	frameBarrier->wait();

	frameTime = WallClockTime() - startTime;

	return true;
}

}

#endif
