/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/devices/ocldevice.h"
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
		HardwareIntersectionDevice *device, PathOCLRenderEngine *re) :
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

static void PGICUpdateCallBack(CompiledScene *compiledScene) {
	compiledScene->RecompilePhotonGI();
}

void PathOCLOpenCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	intersectionDevice->PushThreadCurrentDevice();

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		// Clear the frame buffer
		const u_int filmPixelCount = threadFilms[0]->film->GetWidth() * threadFilms[0]->film->GetHeight();
		intersectionDevice->EnqueueKernel(filmClearKernel,
			HardwareDeviceRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			HardwareDeviceRange(filmClearWorkGroupSize));

		// Initialize random number generator seeds
		intersectionDevice->EnqueueKernel(initSeedKernel,
				HardwareDeviceRange(engine->taskCount), HardwareDeviceRange(initWorkGroupSize));

		// Initialize the tasks buffer
		intersectionDevice->EnqueueKernel(initKernel,
				HardwareDeviceRange(engine->taskCount), HardwareDeviceRange(initWorkGroupSize));

		// Check if I have to load the start film
		if (engine->hasStartFilm && (threadIndex == 0))
			threadFilms[0]->SendFilm(intersectionDevice);

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		// The film refresh time target
		const double targetTime = 0.2; // 200ms

		u_int iterations = 4;
		u_int totalIterations = 0;

		double totalTransferTime = 0.0;
		double totalKernelTime = 0.0;

		const boost::function<void()> pgicUpdateCallBack = boost::bind(PGICUpdateCallBack, engine->compiledScene);

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
				threadFilms[0]->RecvFilm(intersectionDevice);

				// Async. transfer of GPU task statistics
				intersectionDevice->EnqueueReadBuffer(
					taskStatsBuff,
					CL_FALSE,
					sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount,
					gpuTaskStats);

				intersectionDevice->FinishQueue();
				
				// I need to update the film samples count
				
				double totalCount = 0.0;
				for (size_t i = 0; i < taskCount; ++i)
					totalCount += gpuTaskStats[i].sampleCount;
				threadFilms[0]->film->SetSampleCount(totalCount, totalCount, 0.0);

				//SLG_LOG("[DEBUG] film transferred");
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
				intersectionDevice->EnqueueTraceRayBuffer(raysBuff, hitsBuff, taskCount);

				// Advance to next path state
				EnqueueAdvancePathsKernel();
			}
			totalIterations += iterations;

			intersectionDevice->FinishQueue();
			const double timeKernelEnd = WallClockTime();
			totalKernelTime += timeKernelEnd - timeKernelStart;

			/*if (threadIndex == 0)
				SLG_LOG("[DEBUG] transfer time: " << (timeTransferEnd - timeTransferStart) * 1000.0 << "ms "
						"kernel time: " << (timeKernelEnd - timeKernelStart) * 1000.0 << "ms "
						"iterations: " << iterations << " #"<< taskCount << ")");*/

			// Check if I have to adjust the number of kernel enqueued
			if (timeKernelEnd - timeKernelStart > targetTime)
				iterations = Max<u_int>(iterations - 1, 1);
			else
				iterations = Min<u_int>(iterations + 1, 128);

			// Check halt conditions
			if (engine->film->GetConvergence() == 1.f)
				break;

			if (engine->photonGICache) {
				try {
					if (engine->photonGICache->Update(threadIndex, engine->GetTotalEyeSPP(), pgicUpdateCallBack)) {
						InitPhotonGI();
						SetKernelArgs();
					}
				} catch (boost::thread_interrupted &ti) {
					// I have been interrupted, I must stop
					break;
				}
			}
		}

		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	}

	threadFilms[0]->RecvFilm(intersectionDevice);
	intersectionDevice->FinishQueue();
	
	threadDone = true;

	// This is done to stop threads pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	if (engine->photonGICache)
		engine->photonGICache->FinishUpdate(threadIndex);
	
	intersectionDevice->PopThreadCurrentDevice();
}

#endif
