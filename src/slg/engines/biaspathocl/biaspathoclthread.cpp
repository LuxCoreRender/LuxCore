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
	initKernel = NULL;
	renderSampleKernel = NULL;

	tasksBuff = NULL;
	taskStatsBuff = NULL;
	
	gpuTaskStats = NULL;
}

BiasPathOCLRenderThread::~BiasPathOCLRenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete initKernel;
	delete renderSampleKernel;

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
			" -D PARAM_AA_SAMPLES=" << engine->aaSamples <<
			" -D PARAM_USE_PIXEL_ATOMICS";

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
	// Init kernel
	//----------------------------------------------------------------------

	CompileKernel(program, &initKernel, &initWorkGroupSize, "Init");

	//----------------------------------------------------------------------
	// AdvancePaths kernel
	//----------------------------------------------------------------------

	CompileKernel(program, &renderSampleKernel, &renderSampleWorkGroupSize, "RenderSample");
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
	// renderSampleKernel
	//--------------------------------------------------------------------------

	CompiledScene *cscene = engine->compiledScene;

	u_int argIndex = 0;
	renderSampleKernel->setArg(argIndex++, 0);
	renderSampleKernel->setArg(argIndex++, 0);
	renderSampleKernel->setArg(argIndex++, 0);
	renderSampleKernel->setArg(argIndex++, engine->film->GetWidth());
	renderSampleKernel->setArg(argIndex++, engine->film->GetHeight());
	renderSampleKernel->setArg(argIndex++, *tasksBuff);
	renderSampleKernel->setArg(argIndex++, *taskStatsBuff);

	// Film parameters
	renderSampleKernel->setArg(argIndex++, threadFilm->GetWidth());
	renderSampleKernel->setArg(argIndex++, threadFilm->GetHeight());
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		renderSampleKernel->setArg(argIndex++, *(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]));
	if (threadFilm->HasChannel(Film::ALPHA))
		renderSampleKernel->setArg(argIndex++, *channel_ALPHA_Buff);
	if (threadFilm->HasChannel(Film::DEPTH))
		renderSampleKernel->setArg(argIndex++, *channel_DEPTH_Buff);
	if (threadFilm->HasChannel(Film::POSITION))
		renderSampleKernel->setArg(argIndex++, *channel_POSITION_Buff);
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		renderSampleKernel->setArg(argIndex++, *channel_GEOMETRY_NORMAL_Buff);
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		renderSampleKernel->setArg(argIndex++, *channel_SHADING_NORMAL_Buff);
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		renderSampleKernel->setArg(argIndex++, *channel_MATERIAL_ID_Buff);
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		renderSampleKernel->setArg(argIndex++, *channel_DIRECT_DIFFUSE_Buff);
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		renderSampleKernel->setArg(argIndex++, *channel_DIRECT_GLOSSY_Buff);
	if (threadFilm->HasChannel(Film::EMISSION))
		renderSampleKernel->setArg(argIndex++, *channel_EMISSION_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		renderSampleKernel->setArg(argIndex++, *channel_INDIRECT_DIFFUSE_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		renderSampleKernel->setArg(argIndex++, *channel_INDIRECT_GLOSSY_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		renderSampleKernel->setArg(argIndex++, *channel_INDIRECT_SPECULAR_Buff);
	if (threadFilm->HasChannel(Film::MATERIAL_ID_MASK))
		renderSampleKernel->setArg(argIndex++, *channel_MATERIAL_ID_MASK_Buff);
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		renderSampleKernel->setArg(argIndex++, *channel_DIRECT_SHADOW_MASK_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		renderSampleKernel->setArg(argIndex++, *channel_INDIRECT_SHADOW_MASK_Buff);
	if (threadFilm->HasChannel(Film::UV))
		renderSampleKernel->setArg(argIndex++, *channel_UV_Buff);

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
	// initKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	initKernel->setArg(argIndex++, engine->seedBase + threadIndex * engine->taskCount);
	initKernel->setArg(argIndex++, *tasksBuff);
	initKernel->setArg(argIndex++, *taskStatsBuff);
}

void BiasPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	const u_int tileSize = engine->tileRepository->tileSize;
	const u_int filmPixelCount = tileSize * tileSize;
	const u_int taskCount = engine->taskCount;

	// Initialize OpenCL structures
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, initWorkGroupSize)),
			cl::NDRange(initWorkGroupSize));

	//--------------------------------------------------------------------------
	// Extract the tile to render
	//--------------------------------------------------------------------------

	TileRepository::Tile *tile = NULL;
	while (engine->NextTile(&tile, threadFilm) && !boost::this_thread::interruption_requested()) {
		// Render the tile
		threadFilm->Reset();
		//const u_int tileWidth = Min(engine->tileRepository->tileSize, threadFilm->GetWidth() - tile->xStart);
		//const u_int tileHeight = Min(engine->tileRepository->tileSize, threadFilm->GetHeight() - tile->yStart);
		//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile: "
		//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
		//		"(" << tileWidth << ", " << tileHeight << ") [" << tile->sampleIndex << "]");

		// Clear the frame buffer
		oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));

		// Render the tile
		{
			boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);
			renderSampleKernel->setArg(0, tile->xStart);
			renderSampleKernel->setArg(1, tile->yStart);
			renderSampleKernel->setArg(2, tile->sampleIndex);
		}
		oclQueue.enqueueNDRangeKernel(*renderSampleKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
				cl::NDRange(renderSampleWorkGroupSize));

		// Async. transfer of the Film buffers
		TransferFilm(oclQueue);

		// Async. transfer of GPU task statistics
		oclQueue.enqueueReadBuffer(
			*(taskStatsBuff),
			CL_FALSE,
			0,
			sizeof(slg::ocl::biaspathocl::GPUTaskStats) * engine->taskCount,
			gpuTaskStats);

		oclQueue.finish();

		// In order to update the statistics
//		intersectionDevice->IntersectionKernelExecuted(taskCount);
	}

	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
}

#endif
