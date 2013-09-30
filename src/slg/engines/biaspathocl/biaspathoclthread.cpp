/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include "slg/slg.h"
#include "slg/engines/biaspathocl/biaspathocl.h"
#include "luxrays/core/oclintersectiondevice.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathOCLRenderThread
//------------------------------------------------------------------------------

BiasPathOCLRenderThread::BiasPathOCLRenderThread(const u_int index,
	OpenCLIntersectionDevice *device, PathOCLRenderEngine *re) : 
	PathOCLRenderThread(index, device, re) {
}

BiasPathOCLRenderThread::~BiasPathOCLRenderThread() {
}

void BiasPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	Film *engineFilm = engine->film;
	const u_int filmWidth = engineFilm->GetWidth();
	const u_int filmHeight = engineFilm->GetHeight();

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	const u_int taskCount = engine->taskCount;

	//--------------------------------------------------------------------------
	// Extract the tile to render
	//--------------------------------------------------------------------------

	TileRepository::Tile *tile = NULL;
	while (engine->NextTile(&tile, film) && !boost::this_thread::interruption_requested()) {
		// Render the tile
		film->Reset();
		const u_int tileWidth = Min(engine->tileRepository->tileSize, filmWidth - tile->xStart);
		const u_int tileHeight = Min(engine->tileRepository->tileSize, filmHeight - tile->yStart);
		SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile: "
				"(" << tile->xStart << ", " << tile->yStart << ") => " <<
				"(" << tileWidth << ", " << tileHeight << ")");

		// Clear the frame buffer
		const u_int filmPixelCount = engine->film->GetWidth() * engine->film->GetHeight();
		oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));

		// Initialize the tasks buffer
		oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initWorkGroupSize)),
				cl::NDRange(initWorkGroupSize));

		// Render the tile
		oclQueue.enqueueNDRangeKernel(*advancePathsKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
				cl::NDRange(advancePathsWorkGroupSize));

		// Async. transfer of the Film buffers
		TransferFilm(oclQueue);

		// Async. transfer of GPU task statistics
		oclQueue.enqueueReadBuffer(
			*(taskStatsBuff),
			CL_FALSE,
			0,
			sizeof(slg::ocl::pathocl::GPUTaskStats) * taskCount,
			gpuTaskStats);

		oclQueue.finish();

		// In order to update the statistics
		intersectionDevice->IntersectionKernelExecuted(taskCount);
	}

	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
}

#endif
