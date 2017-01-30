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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/randomgen.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathocl/pathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLRenderThread
//------------------------------------------------------------------------------

PathOCLRenderThread::PathOCLRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLRenderEngine *re) :
		PathOCLStateKernelBaseRenderThread(index, device, re) {
}

PathOCLRenderThread::~PathOCLRenderThread() {
}

void PathOCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight,
		u_int *filmSubRegion) {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const Film *engineFilm = engine->film;

	*filmWidth = engineFilm->GetWidth();
	*filmHeight = engineFilm->GetHeight();

	const u_int *subRegion = engineFilm->GetSubRegion();
	filmSubRegion[0] = subRegion[0];
	filmSubRegion[1] = subRegion[1];
	filmSubRegion[2] = subRegion[2];
	filmSubRegion[3] = subRegion[3];
}

void PathOCLRenderThread::StartRenderThread() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;

	// I have to load the start film otherwise it is overwritten at the first
	// merge of all thread films
	if (engine->hasStartFilm && (threadIndex == 0))
		threadFilms[0]->film->AddFilm(*engine->film);

	PathOCLStateKernelBaseRenderThread::StartRenderThread();
}

void PathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		// Clear the frame buffer
		const u_int filmPixelCount = threadFilms[0]->film->GetWidth() * threadFilms[0]->film->GetHeight();
		oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));

		// Initialize random number generator seeds
		oclQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(engine->taskCount), cl::NDRange(initWorkGroupSize));

		// Initialize the tasks buffer
		oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
				cl::NDRange(engine->taskCount), cl::NDRange(initWorkGroupSize));

		// Check if I have to load the start film
		if (engine->hasStartFilm && (threadIndex == 0))
			threadFilms[0]->SendFilm(oclQueue);

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		// signed int in order to avoid problems with underflows (when computing
		// iterations - 1)
		int iterations = 1;
		u_int totalIterations = 0;
		// I can not use engine->renderConfig->GetProperty() here because the
		// RenderConfig properties cache is not thread safe
		const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>();

		double startTime = WallClockTime();
		bool done = false;
		while (!boost::this_thread::interruption_requested() && !done) {
			//if (threadIndex == 0)
			//	SLG_LOG("[DEBUG] =================================");

			// Check if we are in pause mode
			if (engine->pauseMode) {
				// Check every 100ms if I have to continue the rendering
				while (!boost::this_thread::interruption_requested() && engine->pauseMode)
					boost::this_thread::sleep(boost::posix_time::millisec(100));

				if (boost::this_thread::interruption_requested())
					break;
			}

			// Async. transfer of the Film buffers
			threadFilms[0]->RecvFilm(oclQueue);

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(slg::ocl::pathoclstatebase::GPUTaskStats) * taskCount,
				gpuTaskStats);

			// Decide the target refresh time based on screen refresh interval

			// I can not use engine->renderConfig->GetProperty() here because the
			// RenderConfig properties cache is not thread safe
			const u_int screenRefreshInterval = engine->renderConfig->cfg.Get(
				Property("screen.refresh.interval")(100u)).Get<u_int>();
			double targetTime;
			if (screenRefreshInterval <= 100)
				targetTime = 0.025; // 25 ms
			else if (screenRefreshInterval <= 500)
				targetTime = 0.050; // 50 ms
			else
				targetTime = 0.075; // 75 ms

			for (;;) {
				cl::Event event;

				const double t1 = WallClockTime();
				for (int i = 0; i < iterations; ++i) {
					// Trace rays
					intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
							*(hitsBuff), taskCount, NULL, (i == 0) ? &event : NULL);

					// Advance to next path state
					EnqueueAdvancePathsKernel(oclQueue);
				}
				oclQueue.flush();
				totalIterations += (u_int)iterations;

				event.wait();
				const double t2 = WallClockTime();

				/*if (threadIndex == 0)
					SLG_LOG("[DEBUG] Delta time: " << (t2 - t1) * 1000.0 <<
							"ms (screenRefreshInterval: " << screenRefreshInterval <<
							" iterations: " << iterations << ")");*/

				// Check if I have to adjust the number of kernel enqueued (only
				// if haltDebug is not enabled)
				if (haltDebug == 0u) {
					if (t2 - t1 > targetTime)
						iterations = Max(iterations - 1, 1);
					else
						iterations = Min(iterations + 1, 128);
				}

				// Check if it is time to refresh the screen
				if (((t2 - startTime) * 1000.0 > (double)screenRefreshInterval) ||
						boost::this_thread::interruption_requested())
					break;

				if ((haltDebug > 0u) && (totalIterations >= haltDebug)) {
					done = true;
					break;
				}
			}

			startTime = WallClockTime();
		}

		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error &err) {
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}

	threadFilms[0]->RecvFilm(oclQueue);
	oclQueue.finish();
}

#endif
