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
	PathOCLBaseRenderThread(index, device, re) {
	initSeedKernel = NULL;
	initStatKernel = NULL;
	renderSampleKernel = NULL;
	mergePixelSamplesKernel = NULL;

	tasksBuff = NULL;
	taskStatsBuff = NULL;
	
	gpuTaskStats = NULL;
}

BiasPathOCLRenderThread::~BiasPathOCLRenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete initSeedKernel;
	delete initStatKernel;
	delete renderSampleKernel;
	delete mergePixelSamplesKernel;

	delete tasksBuff;
	delete[] gpuTaskStats;
}

void BiasPathOCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	*filmWidth = engine->tileRepository->tileSize;
	*filmHeight = engine->tileRepository->tileSize;
}

string BiasPathOCLRenderThread::AdditionalKernelOptions() {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_TASK_COUNT=" <<  engine->taskCount <<
			" -D PARAM_TILE_SIZE=" << engine->tileRepository->tileSize <<
			" -D PARAM_AA_SAMPLES=" << engine->aaSamples;

	return ss.str();
}

string BiasPathOCLRenderThread::AdditionalKernelSources() {
	stringstream ssKernel;
	ssKernel <<
		intersectionDevice->GetIntersectionKernelSource() <<
		slg::ocl::KernelSource_biaspathocl_datatypes <<
		slg::ocl::KernelSource_biaspathocl_kernels;

	return ssKernel.str();
}

void BiasPathOCLRenderThread::CompileAdditionalKernels(cl::Program *program) {
	//----------------------------------------------------------------------
	// InitSeed kernel
	//----------------------------------------------------------------------

	CompileKernel(program, &initSeedKernel, &initSeedWorkGroupSize, "InitSeed");

	//----------------------------------------------------------------------
	// InitSeed kernel
	//----------------------------------------------------------------------

	CompileKernel(program, &initStatKernel, &initStatWorkGroupSize, "InitStat");

	//----------------------------------------------------------------------
	// RenderSample kernel
	//----------------------------------------------------------------------

	CompileKernel(program, &renderSampleKernel, &renderSampleWorkGroupSize, "RenderSample");

	//----------------------------------------------------------------------
	// MergePixelSamples kernel
	//----------------------------------------------------------------------

	CompileKernel(program, &mergePixelSamplesKernel, &mergePixelSamplesWorkGroupSize, "MergePixelSamples");
}

void BiasPathOCLRenderThread::AdditionalInit() {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;

	// In case renderEngine->taskCount has changed
	delete[] gpuTaskStats;
	gpuTaskStats = new slg::ocl::biaspathocl::GPUTaskStats[engine->taskCount];

	//--------------------------------------------------------------------------
	// Allocate GPU task buffers
	//--------------------------------------------------------------------------

	const size_t GPUTaskSize = sizeof(slg::ocl::Seed) + GetSampleResultSize();
	AllocOCLBufferRW(&tasksBuff, GPUTaskSize * engine->taskCount, "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&taskStatsBuff, sizeof(slg::ocl::biaspathocl::GPUTaskStats) * engine->taskCount, "GPUTask Stats");
}

void BiasPathOCLRenderThread::SetAdditionalKernelArgs() {
	// Set OpenCL kernel arguments

	// OpenCL kernel setArg() is the only no thread safe function in OpenCL 1.1 so
	// I need to use a mutex here
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);

	//--------------------------------------------------------------------------
	// initSeedKernel
	//--------------------------------------------------------------------------

	u_int argIndex = 0;
	initSeedKernel->setArg(argIndex++, engine->seedBase + threadIndex * engine->taskCount);
	initSeedKernel->setArg(argIndex++, *tasksBuff);

	//--------------------------------------------------------------------------
	// initStatKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	initStatKernel->setArg(argIndex++, *taskStatsBuff);

	//--------------------------------------------------------------------------
	// renderSampleKernel
	//--------------------------------------------------------------------------

	CompiledScene *cscene = engine->compiledScene;

	argIndex = 0;
	renderSampleKernel->setArg(argIndex++, 0);
	renderSampleKernel->setArg(argIndex++, 0);
	renderSampleKernel->setArg(argIndex++, 0);
	renderSampleKernel->setArg(argIndex++, engine->film->GetWidth());
	renderSampleKernel->setArg(argIndex++, engine->film->GetHeight());
	renderSampleKernel->setArg(argIndex++, *tasksBuff);
	renderSampleKernel->setArg(argIndex++, *taskStatsBuff);

	// Film parameters
	argIndex = SetFilmKernelArgs(*renderSampleKernel, argIndex);

	// Scene parameters
	renderSampleKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
	renderSampleKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
	renderSampleKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
	renderSampleKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	renderSampleKernel->setArg(argIndex++, *materialsBuff);
	renderSampleKernel->setArg(argIndex++, *texturesBuff);
	renderSampleKernel->setArg(argIndex++, *meshMatsBuff);
	renderSampleKernel->setArg(argIndex++, *meshDescsBuff);
	renderSampleKernel->setArg(argIndex++, *vertsBuff);
	if (normalsBuff)
		renderSampleKernel->setArg(argIndex++, *normalsBuff);
	if (uvsBuff)
		renderSampleKernel->setArg(argIndex++, *uvsBuff);
	if (colsBuff)
		renderSampleKernel->setArg(argIndex++, *colsBuff);
	if (alphasBuff)
		renderSampleKernel->setArg(argIndex++, *alphasBuff);
	renderSampleKernel->setArg(argIndex++, *trianglesBuff);
	renderSampleKernel->setArg(argIndex++, *cameraBuff);
	renderSampleKernel->setArg(argIndex++, *lightsDistributionBuff);
	if (infiniteLightBuff) {
		renderSampleKernel->setArg(argIndex++, *infiniteLightBuff);
		renderSampleKernel->setArg(argIndex++, *infiniteLightDistributionBuff);
	}
	if (sunLightBuff)
		renderSampleKernel->setArg(argIndex++, *sunLightBuff);
	if (skyLightBuff)
		renderSampleKernel->setArg(argIndex++, *skyLightBuff);
	if (triLightDefsBuff) {
		renderSampleKernel->setArg(argIndex++, *triLightDefsBuff);
		renderSampleKernel->setArg(argIndex++, *meshTriLightDefsOffsetBuff);
	}
	if (imageMapDescsBuff) {
		renderSampleKernel->setArg(argIndex++, *imageMapDescsBuff);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			renderSampleKernel->setArg(argIndex++, *(imageMapsBuff[i]));
	}

	argIndex = intersectionDevice->SetIntersectionKernelArgs(*renderSampleKernel, argIndex);

	//--------------------------------------------------------------------------
	// initSeedKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetWidth());
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetHeight());
	mergePixelSamplesKernel->setArg(argIndex++, *tasksBuff);
	argIndex = SetFilmKernelArgs(*mergePixelSamplesKernel, argIndex);
}

void BiasPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	try {
		//----------------------------------------------------------------------
		// Initialization
		//----------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
		const u_int tileSize = engine->tileRepository->tileSize;
		const u_int filmPixelCount = tileSize * tileSize;
		const u_int taskCount = engine->taskCount;

		// Initialize OpenCL structures
		oclQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initSeedWorkGroupSize)),
				cl::NDRange(initSeedWorkGroupSize));

		//----------------------------------------------------------------------
		// Extract the tile to render
		//----------------------------------------------------------------------

		TileRepository::Tile *tile = NULL;
		while (engine->NextTile(&tile, threadFilm) && !boost::this_thread::interruption_requested()) {
			//const double t0 = WallClockTime();
			threadFilm->Reset();
			//const u_int tileWidth = Min(engine->tileRepository->tileSize, threadFilm->GetWidth() - tile->xStart);
			//const u_int tileHeight = Min(engine->tileRepository->tileSize, threadFilm->GetHeight() - tile->yStart);
			//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile: "
			//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
			//		"(" << tileWidth << ", " << tileHeight << ") [" << tile->sampleIndex << "]");

			u_int tileTaskCount =  (tile->sampleIndex < 0) ? taskCount : filmPixelCount;
				
			// Clear the frame buffer
			oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
				cl::NDRange(filmClearWorkGroupSize));

			// Initialize the statistics
			oclQueue.enqueueNDRangeKernel(*initStatKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(tileTaskCount, initStatWorkGroupSize)),
				cl::NDRange(initStatWorkGroupSize));

			// Render the tile
			{
				boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);
				renderSampleKernel->setArg(0, tile->xStart);
				renderSampleKernel->setArg(1, tile->yStart);
				renderSampleKernel->setArg(2, tile->sampleIndex);
				if (tile->sampleIndex < 0) {
					mergePixelSamplesKernel->setArg(0, tile->xStart);
					mergePixelSamplesKernel->setArg(1, tile->yStart);
				}
			}

			if (tile->sampleIndex < 0) {
				// Render all pixel samples
				oclQueue.enqueueNDRangeKernel(*renderSampleKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(tileTaskCount, renderSampleWorkGroupSize)),
						cl::NDRange(renderSampleWorkGroupSize));
				// Merge all pixel samples
				oclQueue.enqueueNDRangeKernel(*mergePixelSamplesKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(filmPixelCount, mergePixelSamplesWorkGroupSize)),
						cl::NDRange(mergePixelSamplesWorkGroupSize));
			} else {
				oclQueue.enqueueNDRangeKernel(*renderSampleKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(tileTaskCount, renderSampleWorkGroupSize)),
						cl::NDRange(renderSampleWorkGroupSize));
			}

			// Async. transfer of the Film buffers
			TransferFilm(oclQueue);
			threadFilm->AddSampleCount(tileTaskCount);

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(slg::ocl::biaspathocl::GPUTaskStats) * engine->taskCount,
				gpuTaskStats);

			oclQueue.finish();

			// In order to update the statistics
			u_int tracedRaysCount = 0;
			for (uint i = 0; i < tileTaskCount; ++i)
				tracedRaysCount += gpuTaskStats[i].raysCount;
			intersectionDevice->IntersectionKernelExecuted(tracedRaysCount);

			//const double t1 = WallClockTime();
			//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile rendering time: " + ToString((u_int)((t1 - t0) * 1000.0)) + "ms");
		}
		//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}
}

#endif
