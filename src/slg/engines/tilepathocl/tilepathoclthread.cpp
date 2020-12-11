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

#include "luxrays/devices/ocldevice.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/engines/tilepathocl/tilepathocl.h"

#include <boost/bind.hpp>

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathOCLRenderThread
//------------------------------------------------------------------------------

TilePathOCLRenderThread::TilePathOCLRenderThread(const u_int index,
	HardwareIntersectionDevice *device, TilePathOCLRenderEngine *re) : 
	PathOCLBaseOCLRenderThread(index, device, re) {
}

TilePathOCLRenderThread::~TilePathOCLRenderThread() {
}

void TilePathOCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight,
		u_int *filmSubRegion) {
	TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;
	*filmWidth = engine->tileRepository->tileWidth;
	*filmHeight = engine->tileRepository->tileHeight;
	filmSubRegion[0] = 0; 
	filmSubRegion[1] = engine->tileRepository->tileWidth - 1;
	filmSubRegion[2] = 0;
	filmSubRegion[3] = engine->tileRepository->tileHeight - 1;
}

void TilePathOCLRenderThread::UpdateSamplerData(const TileWork &tileWork,
		slg::ocl::TilePathSamplerSharedData &sharedData) {
	TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;
	if (engine->oclSampler->type != slg::ocl::TILEPATHSAMPLER)
		throw runtime_error("Wrong sampler in PathOCLBaseRenderThread::UpdateSamplesBuffer(): " +
						ToString(engine->oclSampler->type));

	// Update samplerSharedDataBuff

	// To have an asynchronous EnqueueWriteBuffer(), I need to keep a copy of
	// sharedData around.
	sharedData.cameraFilmWidth = engine->film->GetWidth();
	sharedData.cameraFilmHeight =  engine->film->GetHeight();
	sharedData.tileStartX =  tileWork.GetCoord().x;
	sharedData.tileStartY =  tileWork.GetCoord().y;
	sharedData.tileWidth =  tileWork.GetCoord().width;
	sharedData.tileHeight =  tileWork.GetCoord().height;
	sharedData.tilePass =  tileWork.passToRender;
	sharedData.aaSamples =  engine->aaSamples;
	sharedData.multipassIndexToRender = tileWork.multipassIndexToRender;

	intersectionDevice->EnqueueWriteBuffer(samplerSharedDataBuff, CL_FALSE,
			sizeof(slg::ocl::TilePathSamplerSharedData), &sharedData);
}

void TilePathOCLRenderThread::RenderTileWork(const TileWork &tileWork,
		slg::ocl::TilePathSamplerSharedData &sharedData,
		const u_int filmIndex) {
	TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;

	threadFilms[filmIndex]->film->Reset();
	if (threadFilms[filmIndex]->film->GetDenoiser().IsEnabled())
		threadFilms[filmIndex]->film->GetDenoiser().CopyReferenceFilm(engine->film);

	// Clear the frame buffer
	threadFilms[filmIndex]->ClearFilm(intersectionDevice, filmClearKernel, filmClearWorkGroupSize);

	// Clear the frame buffer
	const u_int filmPixelCount = threadFilms[filmIndex]->film->GetWidth() * threadFilms[filmIndex]->film->GetHeight();
	intersectionDevice->EnqueueKernel(filmClearKernel,
		HardwareDeviceRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
		HardwareDeviceRange(filmClearWorkGroupSize));

	// Update all kernel args
	{
		boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);

		SetInitKernelArgs(filmIndex);

		SetAllAdvancePathsKernelArgs(filmIndex);
	}

	// Update Sampler shared data
	UpdateSamplerData(tileWork, sharedData);

	// Initialize the tasks buffer
	intersectionDevice->EnqueueKernel(initKernel,
			HardwareDeviceRange(engine->taskCount), HardwareDeviceRange(initWorkGroupSize));

	// There are 2 rays to trace for each path vertex (the last vertex traces only one ray)
	const u_int worstCaseIterationCount = (engine->pathTracer.maxPathDepth.depth == 1) ? 2 : (engine->pathTracer.maxPathDepth.depth * 2 - 1);
	for (u_int i = 0; i < worstCaseIterationCount; ++i) {
		// Trace rays
		intersectionDevice->EnqueueTraceRayBuffer(raysBuff, hitsBuff, engine->taskCount);

		// Advance to next path state
		EnqueueAdvancePathsKernel();
	}

	// Async. transfer of the Film buffers
	threadFilms[filmIndex]->RecvFilm(intersectionDevice);
	threadFilms[filmIndex]->film->AddSampleCount(0,
			tileWork.GetCoord().width * tileWork.GetCoord().height *
			engine->aaSamples * engine->aaSamples, 0.0);
}

static void PGICUpdateCallBack(CompiledScene *compiledScene) {
	compiledScene->RecompilePhotonGI();
}

void TilePathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;

	intersectionDevice->PushThreadCurrentDevice();

	try {
		//----------------------------------------------------------------------
		// Initialization
		//----------------------------------------------------------------------

		const u_int taskCount = engine->taskCount;

		// Initialize random number generator seeds
		intersectionDevice->EnqueueKernel(initSeedKernel,
				HardwareDeviceRange(taskCount), HardwareDeviceRange(initWorkGroupSize));

		//----------------------------------------------------------------------
		// Extract the tile to render
		//----------------------------------------------------------------------

		vector<TileWork> tileWorks(1);
		vector<slg::ocl::TilePathSamplerSharedData> samplerDatas(1);
		const boost::function<void()> pgicUpdateCallBack = boost::bind(PGICUpdateCallBack, engine->compiledScene);
		while (!boost::this_thread::interruption_requested()) {
			// Check if we are in pause mode
			if (engine->pauseMode) {
				// Check every 100ms if I have to continue the rendering
				while (!boost::this_thread::interruption_requested() && engine->pauseMode)
					boost::this_thread::sleep(boost::posix_time::millisec(100));

				if (boost::this_thread::interruption_requested())
					break;
			}

			const double t0 = WallClockTime();

			// Enqueue the rendering of all tiles

			bool allTileDone = true;
			for (u_int i = 0; i < tileWorks.size(); ++i) {
				if (engine->tileRepository->NextTile(engine->film, engine->filmMutex, tileWorks[i], threadFilms[i]->film)) {
					//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] TileWork: " << tileWork);

					// Render the tile
					RenderTileWork(tileWorks[i], samplerDatas[i], i);

					allTileDone = false;
				} else
					tileWorks[i].Reset();
			}

			// Async. transfer of GPU task statistics
			intersectionDevice->EnqueueReadBuffer(
				taskStatsBuff,
				CL_FALSE,
				sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount,
				gpuTaskStats);

			intersectionDevice->FinishQueue();

			const double t1 = WallClockTime();
			const double renderingTime = t1 - t0;
			//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] " << tiles.size() << "xTile(s) rendering time: " << (u_int)(renderingTime * 1000.0) << "ms");

			if (allTileDone)
				break;

			// Check the the time spent, if it is too small (< 400ms) get more tiles
			// (avoid to increase the number of tiles on CPU devices, it is useless)
			if ((tileWorks.size() < engine->maxTilePerDevice) && (renderingTime < 0.4) && 
					(intersectionDevice->GetDeviceDesc()->GetType() != DEVICE_TYPE_OPENCL_CPU)) {
				IncThreadFilms();
				tileWorks.resize(tileWorks.size() + 1);
				samplerDatas.resize(samplerDatas.size() + 1);

				SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Increased the number of rendered tiles to: " << tileWorks.size());
			}

			if (engine->photonGICache) {
				try {
					const u_int spp = engine->film->GetTotalEyeSampleCount() / engine->film->GetPixelCount();

					if (engine->photonGICache->Update(threadIndex, spp, pgicUpdateCallBack)) {
						InitPhotonGI();
						SetKernelArgs();
					}
				} catch (boost::thread_interrupted &ti) {
					// I have been interrupted, I must stop
					break;
				}
			}
		}

		//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	}

	threadDone = true;

	// This is done to interrupt thread pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	for (u_int i = 0; i < engine->renderOCLThreads.size(); ++i)
		engine->renderOCLThreads[i]->Interrupt();
	for (u_int i = 0; i < engine->renderNativeThreads.size(); ++i)
		engine->renderNativeThreads[i]->Interrupt();
	
	intersectionDevice->PopThreadCurrentDevice();
}

#endif
