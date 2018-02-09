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

#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/engines/tilepathocl/tilepathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathOCLRenderThread
//------------------------------------------------------------------------------

TilePathOCLRenderThread::TilePathOCLRenderThread(const u_int index,
	OpenCLIntersectionDevice *device, TilePathOCLRenderEngine *re) : 
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

void TilePathOCLRenderThread::RenderTile(const TileRepository::Tile *tile,
		const u_int filmIndex) {
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;

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

		SetInitKernelArgs(filmIndex);
		// Add TILEPATHOCL specific parameters
		u_int argIndex = initKernelArgsCount;
		initKernel->setArg(argIndex++, engine->film->GetWidth());
		initKernel->setArg(argIndex++, engine->film->GetHeight());
		initKernel->setArg(argIndex++, tile->coord.x);
		initKernel->setArg(argIndex++, tile->coord.y);
		initKernel->setArg(argIndex++, tile->coord.width);
		initKernel->setArg(argIndex++, tile->coord.height);
		initKernel->setArg(argIndex++, tile->pass);
		initKernel->setArg(argIndex++, engine->aaSamples);

		SetAllAdvancePathsKernelArgs(filmIndex);
	}

	// Initialize the tasks buffer
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(engine->taskCount), cl::NDRange(initWorkGroupSize));

	// There are 2 rays to trace for each path vertex (the last vertex traces only one ray)
	const u_int worstCaseIterationCount = (engine->pathTracer.maxPathDepth.depth == 1) ? 2 : (engine->pathTracer.maxPathDepth.depth * 2 - 1);
	for (u_int i = 0; i < worstCaseIterationCount; ++i) {
		// Trace rays
		intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
				*(hitsBuff), engine->taskCount, NULL, NULL);

		// Advance to next path state
		EnqueueAdvancePathsKernel(oclQueue);
	}

	// Async. transfer of the Film buffers
	threadFilms[filmIndex]->RecvFilm(oclQueue);
	threadFilms[filmIndex]->film->AddSampleCount(tile->coord.width * tile->coord.height *
			engine->aaSamples * engine->aaSamples);
}

void TilePathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	try {
		//----------------------------------------------------------------------
		// Initialization
		//----------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;
		const u_int taskCount = engine->taskCount;

		// Initialize random number generator seeds
		oclQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(taskCount), cl::NDRange(initWorkGroupSize));

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
					//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Tile: "
					//		"(" << tiles[i]->coord.x << ", " << tiles[i]->coord.y << ") => " <<
					//		"(" << tiles[i]->coord.width << ", " << tiles[i]->coord.height << ")");

					// Render the tile
					RenderTile(tiles[i], i);

					allTileDone = false;
				} else
					tiles[i] = NULL;
			}

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount,
				gpuTaskStats);

			oclQueue.finish();

			const double t1 = WallClockTime();
			const double renderingTime = t1 - t0;
			//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] " << tiles.size() << "xTile(s) rendering time: " << (u_int)(renderingTime * 1000.0) << "ms");

			if (allTileDone)
				break;

			// Check the the time spent, if it is too small (< 400ms) get more tiles
			// (avoid to increase the number of tiles on CPU devices, it is useless)
			if ((tiles.size() < engine->maxTilePerDevice) && (renderingTime < 0.4) && 
					(intersectionDevice->GetDeviceDesc()->GetType() != DEVICE_TYPE_OPENCL_CPU)) {
				IncThreadFilms();
				tiles.push_back(NULL);

				SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Increased the number of rendered tiles to: " << tiles.size());
			}
		}

		//SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error &err) {
		SLG_LOG("[TilePathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}
}

#endif
