/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

PathOCLOpenCLRenderThread::PathOCLOpenCLRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLRenderEngine *re) :
		PathOCLBaseOCLRenderThread(index, device, re) {
}

PathOCLOpenCLRenderThread::~PathOCLOpenCLRenderThread() {
}

void PathOCLOpenCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight,
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

void PathOCLOpenCLRenderThread::StartRenderThread() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;

	// I have to load the start film otherwise it is overwritten at the first
	// merge of all thread films
	if (engine->hasStartFilm && (threadIndex == 0))
		threadFilms[0]->film->AddFilm(*engine->film);
	
	PathOCLBaseOCLRenderThread::StartRenderThread();
}

void PathOCLOpenCLRenderThread::RenderThreadImpl() {
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

		// I can not use engine->renderConfig->GetProperty() here because the
		// RenderConfig properties cache is not thread safe
		const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>();

		// The film refresh time target
		const double targetTime = 0.2; // 200ms

		u_int iterations = 4;
		u_int totalIterations = 0;

		double totalTransferTime = 0.0;
		double totalKernelTime = 0.0;

		while (!boost::this_thread::interruption_requested()) {
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

			//------------------------------------------------------------------

			const double timeTransferStart = WallClockTime();

			// Transfer the film only if I have already spent enough time running
			// rendering kernels. This is very important when rendering very high
			// resolution images (for instance at 4961x3508)

			if (totalTransferTime < totalKernelTime * (1.0 / 100.0)) {
				// Async. transfer of the Film buffers
				threadFilms[0]->RecvFilm(oclQueue);

				// Async. transfer of GPU task statistics
				oclQueue.enqueueReadBuffer(
					*(taskStatsBuff),
					CL_FALSE,
					0,
					sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount,
					gpuTaskStats);

				oclQueue.finish();
				
				// I need to update the film samples count
				
				double totalCount = 0.0;
				for (size_t i = 0; i < taskCount; ++i)
					totalCount += gpuTaskStats[i].sampleCount;
				threadFilms[0]->film->SetSampleCount(totalCount);

				//SLG_LOG("[DEBUG] film transfered");
			}
			const double timeTransferEnd = WallClockTime();
			totalTransferTime += timeTransferEnd - timeTransferStart;

			//------------------------------------------------------------------
			
			const double timeKernelStart = WallClockTime();

			// This is required for updating film denoiser parameter
			if (threadFilms[0]->film->GetDenoiser().IsEnabled()) {
				boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);
				SetAllAdvancePathsKernelArgs(0);
			}

			for (u_int i = 0; i < iterations; ++i) {
				// Trace rays
				intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
						*(hitsBuff), taskCount, NULL, NULL);

				// Advance to next path state
				EnqueueAdvancePathsKernel(oclQueue);
			}
			totalIterations += iterations;

			oclQueue.finish();
			const double timeKernelEnd = WallClockTime();
			totalKernelTime += timeKernelEnd - timeKernelStart;

			/*if (threadIndex == 0)
				SLG_LOG("[DEBUG] transfer time: " << (timeTransferEnd - timeTransferStart) * 1000.0 << "ms "
						"kernel time: " << (timeKernelEnd - timeKernelStart) * 1000.0 << "ms "
						"iterations: " << iterations << " #"<< taskCount << ")");*/

			// Check if I have to adjust the number of kernel enqueued (only
			// if haltDebug is not enabled)
			if (haltDebug == 0u) {
				if (timeKernelEnd - timeKernelStart > targetTime)
					iterations = Max<u_int>(iterations - 1, 1);
				else
					iterations = Min<u_int>(iterations + 1, 128);
			}

			// Check halt conditions
			if ((haltDebug > 0u) && (totalIterations >= haltDebug))
				break;
			if (engine->film->GetConvergence() == 1.f)
				break;
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
	
	threadDone = true;
}

#endif
