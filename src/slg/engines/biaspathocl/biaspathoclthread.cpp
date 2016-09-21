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

#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/engines/biaspathocl/biaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathOCLRenderThread
//------------------------------------------------------------------------------

BiasPathOCLRenderThread::BiasPathOCLRenderThread(const u_int index,
	OpenCLIntersectionDevice *device, BiasPathOCLRenderEngine *re) : 
	PathOCLStateKernelBaseRenderThread(index, device, re) {
}

BiasPathOCLRenderThread::~BiasPathOCLRenderThread() {
}

void BiasPathOCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight,
		u_int *filmSubRegion) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	*filmWidth = engine->tileRepository->tileWidth;
	*filmHeight = engine->tileRepository->tileHeight;
	filmSubRegion[0] = 0; 
	filmSubRegion[1] = engine->tileRepository->tileWidth - 1;
	filmSubRegion[2] = 0;
	filmSubRegion[3] = engine->tileRepository->tileHeight - 1;
}

string BiasPathOCLRenderThread::AdditionalKernelOptions() {
	return  " -D PARAM_DISABLE_PATH_RESTART " +
			PathOCLStateKernelBaseRenderThread::AdditionalKernelOptions();
}

void BiasPathOCLRenderThread::RenderTile(const TileRepository::Tile *tile,
		const u_int filmIndex) {
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;

	threadFilms[filmIndex]->film->Reset();

	// Clear the frame buffer
	threadFilms[filmIndex]->ClearFilm(oclQueue, *filmClearKernel, filmClearWorkGroupSize);

	// Clear the frame buffer
	const u_int filmPixelCount = threadFilms[filmIndex]->film->GetWidth() * threadFilms[filmIndex]->film->GetHeight();
	oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
		cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
		cl::NDRange(filmClearWorkGroupSize));

	// Update all kernel args
	{
		boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);

		SetInitKernelArgs(filmIndex, engine->film->GetWidth(), engine->film->GetHeight(),
				tile->xStart, tile->yStart);
		SetAllAdvancePathsKernelArgs(filmIndex);
	}

	// Initialize the tasks buffer
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(engine->taskCount), cl::NDRange(initWorkGroupSize));

	// There are 2 rays to trace for each path vertex
	const u_int worstCaseIterationCount = engine->maxPathDepth.depth * 2;
	for (u_int i = 0; i < worstCaseIterationCount; ++i) {
		// Trace rays
		intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
				*(hitsBuff), engine->taskCount, NULL, NULL);

		// Advance to next path state
		EnqueueAdvancePathsKernel(oclQueue);
	}

	// Async. transfer of the Film buffers
	threadFilms[filmIndex]->TransferFilm(oclQueue);
	
oclQueue.finish();
}

void BiasPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	try {
		//----------------------------------------------------------------------
		// Initialization
		//----------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
		const u_int taskCount = engine->taskCount;

		//----------------------------------------------------------------------
		// Extract the tile to render
		//----------------------------------------------------------------------

		vector<TileRepository::Tile *> tiles(1, NULL);
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
			for (u_int i = 0; i < tiles.size(); ++i) {
				if (engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tiles[i], threadFilms[i]->film)) {
					//const u_int tileW = Min(engine->tileRepository->tileWidth, engine->film->GetWidth() - tiles[i]->xStart);
					//const u_int tileH = Min(engine->tileRepository->tileHeight, engine->film->GetHeight() - tiles[i]->yStart);
					//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile: "
					//		"(" << tiles[i]->xStart << ", " << tiles[i]->yStart << ") => " <<
					//		"(" << tiles[i]->tileWidth << ", " << tiles[i]->tileWidth << ")");

					// Render the tile
					RenderTile(tiles[i], i);

					allTileDone = false;
				} else
					tiles[i] = NULL;
			}

			// Transfer all the results
			for (u_int i = 0; i < tiles.size(); ++i) {
				if (tiles[i]) {
					// Async. transfer of the Film buffers
					threadFilms[i]->TransferFilm(oclQueue);
					threadFilms[i]->film->AddSampleCount(taskCount);
				}
			}

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(slg::ocl::pathoclstatebase::GPUTaskStats) * taskCount,
				gpuTaskStats);

			oclQueue.finish();

			// In order to update the statistics
//			u_int tracedRaysCount = 0;
//			for (u_int i = 0; i < taskCount; ++i)
//				tracedRaysCount += gpuTaskStats[i].raysCount;
//			intersectionDevice->IntersectionKernelExecuted(tracedRaysCount);

			const double t1 = WallClockTime();
			const double renderingTime = t1 - t0;
			//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] " << tiles.size() << "xTile(s) rendering time: " << (u_int)(renderingTime * 1000.0) << "ms");

			if (allTileDone)
				break;

			// Check the the time spent, if it is too small (< 100ms) get more tiles
			// (avoid to increase the number of tiles on CPU devices, it is useless)
			if ((tiles.size() < engine->maxTilePerDevice) && (renderingTime < 0.1) && 
					(intersectionDevice->GetDeviceDesc()->GetType() != DEVICE_TYPE_OPENCL_CPU)) {
				IncThreadFilms();
				tiles.push_back(NULL);

				SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Increased the number of rendered tiles to: " << tiles.size());
			}
		}

		//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error &err) {
		SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}
}

#endif
