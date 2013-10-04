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
	taskResultsBuff = NULL;
	pixelFilterBuff = NULL;
	lightSamplesBuff = NULL;
	
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

	delete[] gpuTaskStats;
}

void BiasPathOCLRenderThread::Stop() {
	PathOCLBaseRenderThread::Stop();

	FreeOCLBuffer(&tasksBuff);
	FreeOCLBuffer(&taskStatsBuff);
	FreeOCLBuffer(&taskResultsBuff);
	FreeOCLBuffer(&pixelFilterBuff);
	FreeOCLBuffer(&lightSamplesBuff);
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
			(engine->tileRepository->enableProgressiveRefinement ?
				" -D PARAM_TILE_PROGRESSIVE_REFINEMENT" : "") <<
			((engine->lightSamplingStrategyONE) ? " -D PARAM_DIRECT_LIGHT_ONE_STRATEGY" : " -D PARAM_DIRECT_LIGHT_ALL_STRATEGY") <<
			" -D PARAM_RADIANCE_CLAMP_MAXVALUE=" << engine->radianceClampMaxValue << "f" <<
			" -D PARAM_AA_SAMPLES=" << engine->aaSamples <<
			" -D PARAM_DIRECT_LIGHT_SAMPLES=" << engine->directLightSamples <<
			" -D PARAM_DIFFUSE_SAMPLES=" << engine->diffuseSamples <<
			" -D PARAM_GLOSSY_SAMPLES=" << engine->glossySamples <<
			" -D PARAM_SPECULAR_SAMPLES=" << engine->specularSamples <<
			" -D PARAM_DEPTH_MAX=" << engine->maxPathDepth.depth <<
			" -D PARAM_DEPTH_DIFFUSE_MAX=" << engine->maxPathDepth.diffuseDepth <<
			" -D PARAM_DEPTH_GLOSSY_MAX=" << engine->maxPathDepth.glossyDepth <<
			" -D PARAM_DEPTH_SPECULAR_MAX=" << engine->maxPathDepth.specularDepth;

	return ss.str();
}

std::string BiasPathOCLRenderThread::AdditionalKernelDefinitions() {
	return "#define CAMERA_GENERATERAY_PARAM_MEM_SPACE_PRIVATE\n"
			"#define BSDF_INIT_PARAM_MEM_SPACE_PRIVATE\n";
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

	const size_t GPUTaskSize =
		// Add Seed memory size
		sizeof(slg::ocl::Seed) +
	
		// Spectrum (throughputPathVertex1) size
		sizeof(Spectrum) +
		// BSDF (bsdfPathVertex1) size
		GetOpenCLBSDFSize() +

		// u_int (lightIndex and lightSampleIndex) size
		((engine->lightSamplingStrategyONE) ? 0 : 2 * sizeof(u_int)) +

		// Spectrum (directLightThroughput) size
		sizeof(Spectrum) +
		// BSDF (directLightBSDF) size
		GetOpenCLBSDFSize() +
		// HitPoint (directLightHitPoint) size
		((engine->compiledScene->triLightDefs.size() > 0) ? GetOpenCLHitPointSize() : 0) +

		// Spectrum (lightRadiance) size
		sizeof(Spectrum) +
		// u_int (lightID) size
		sizeof(u_int) +
	
		// BSDF (tmpBSDF) size
		GetOpenCLBSDFSize() +
		// Spectrum (tmpThroughput) size
		sizeof(Spectrum) +

		// BSDFEvent (vertex1SampleComponent) size
		sizeof(BSDFEvent) +
		// u_int (vertex1SampleIndex) size
		sizeof(u_int) +

		// Spectrum (throughputPathVertexN) size
		sizeof(Spectrum) +
		// BSDF (bsdfPathVertexN) size
		GetOpenCLBSDFSize();
	SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] GPUTask size: " << GPUTaskSize << "bytes");

	AllocOCLBufferRW(&tasksBuff, GPUTaskSize * engine->taskCount, "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&taskStatsBuff, sizeof(slg::ocl::biaspathocl::GPUTaskStats) * engine->taskCount, "GPUTask Stats");

	//--------------------------------------------------------------------------
	// Allocate GPU task SampleResult
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&taskResultsBuff, GetOpenCLSampleResultSize() * engine->taskCount, "GPUTask SampleResult");

	//--------------------------------------------------------------------------
	// Allocate GPU pixel filter distribution
	//--------------------------------------------------------------------------

	AllocOCLBufferRO(&pixelFilterBuff, engine->pixelFilterDistribution,
			sizeof(float) * engine->pixelFilterDistributionSize, "Pixel Filter Distribution");

	//--------------------------------------------------------------------------
	// Allocate GPU light samples count
	//--------------------------------------------------------------------------

	AllocOCLBufferRO(&lightSamplesBuff, &engine->compiledScene->lightSamples[0],
			sizeof(int) * engine->compiledScene->lightSamples.size(), "Light Samples");
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
	renderSampleKernel->setArg(argIndex++, *taskResultsBuff);
	renderSampleKernel->setArg(argIndex++, *pixelFilterBuff);
	renderSampleKernel->setArg(argIndex++, *lightSamplesBuff);

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
	// mergePixelSamplesKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetWidth());
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetHeight());
	mergePixelSamplesKernel->setArg(argIndex++, *taskResultsBuff);
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
				
			// Clear the frame buffer
			oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
				cl::NDRange(filmClearWorkGroupSize));

			// Initialize the statistics
			oclQueue.enqueueNDRangeKernel(*initStatKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initStatWorkGroupSize)),
				cl::NDRange(initStatWorkGroupSize));

			// Render the tile
			{
				boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);
				renderSampleKernel->setArg(0, tile->xStart);
				renderSampleKernel->setArg(1, tile->yStart);
				renderSampleKernel->setArg(2, tile->sampleIndex);
				if (!engine->tileRepository->enableProgressiveRefinement) {
					mergePixelSamplesKernel->setArg(0, tile->xStart);
					mergePixelSamplesKernel->setArg(1, tile->yStart);
				}
			}

			// Render all pixel samples
			oclQueue.enqueueNDRangeKernel(*renderSampleKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
					cl::NDRange(renderSampleWorkGroupSize));
			if (!engine->tileRepository->enableProgressiveRefinement) {
				// Merge all pixel samples
				oclQueue.enqueueNDRangeKernel(*mergePixelSamplesKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(filmPixelCount, mergePixelSamplesWorkGroupSize)),
						cl::NDRange(mergePixelSamplesWorkGroupSize));
			}

			// Async. transfer of the Film buffers
			TransferFilm(oclQueue);
			threadFilm->AddSampleCount(taskCount);

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
			for (u_int i = 0; i < taskCount; ++i)
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
