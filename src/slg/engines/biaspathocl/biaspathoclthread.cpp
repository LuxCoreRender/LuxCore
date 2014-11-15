/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
	PathOCLBaseRenderThread(index, device, re) {
	initSeedKernel = NULL;
	initStatKernel = NULL;
	renderSampleKernel = NULL;
	renderSampleKernel_MK_GENERATE_CAMERA_RAY = NULL;
	renderSampleKernel_MK_TRACE_EYE_RAY = NULL;
	renderSampleKernel_MK_ILLUMINATE_EYE_MISS = NULL;
	mergePixelSamplesKernel = NULL;

	tasksBuff = NULL;
	tasksDirectLightBuff = NULL;
	tasksPathVertexNBuff = NULL;
	taskStatsBuff = NULL;
	taskResultsBuff = NULL;
	pixelFilterBuff = NULL;
	
	gpuTaskStats = NULL;
}

BiasPathOCLRenderThread::~BiasPathOCLRenderThread() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	delete initSeedKernel;
	delete initStatKernel;
	delete renderSampleKernel;
	delete renderSampleKernel_MK_GENERATE_CAMERA_RAY;
	delete renderSampleKernel_MK_TRACE_EYE_RAY;
	delete renderSampleKernel_MK_ILLUMINATE_EYE_MISS;
	delete mergePixelSamplesKernel;

	delete[] gpuTaskStats;
}

void BiasPathOCLRenderThread::Stop() {
	PathOCLBaseRenderThread::Stop();

	FreeOCLBuffer(&tasksBuff);
	FreeOCLBuffer(&tasksDirectLightBuff);
	FreeOCLBuffer(&tasksPathVertexNBuff);
	FreeOCLBuffer(&taskStatsBuff);
	FreeOCLBuffer(&taskResultsBuff);
	FreeOCLBuffer(&pixelFilterBuff);
}

void BiasPathOCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	*filmWidth = engine->tileRepository->tileSize;
	*filmHeight = engine->tileRepository->tileSize;
}

string BiasPathOCLRenderThread::AdditionalKernelOptions() {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;

	const Filter *filter = engine->film->GetFilter();
	const float filterWidthX = filter ? filter->xWidth : 1.f;
	const float filterWidthY = filter ? filter->yWidth : 1.f;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_TASK_COUNT=" <<  engine->taskCount <<
			" -D PARAM_TILE_SIZE=" << engine->tileRepository->tileSize <<
			" -D PARAM_FIRST_VERTEX_DL_COUNT=" << engine->firstVertexLightSampleCount <<
			" -D PARAM_RADIANCE_CLAMP_MAXVALUE=" << engine->radianceClampMaxValue << "f" <<
			" -D PARAM_PDF_CLAMP_VALUE=" << engine->pdfClampValue << "f" <<
			" -D PARAM_AA_SAMPLES=" << engine->aaSamples <<
			" -D PARAM_DIRECT_LIGHT_SAMPLES=" << engine->directLightSamples <<
			" -D PARAM_DIFFUSE_SAMPLES=" << engine->diffuseSamples <<
			" -D PARAM_GLOSSY_SAMPLES=" << engine->glossySamples <<
			" -D PARAM_SPECULAR_SAMPLES=" << engine->specularSamples <<
			" -D PARAM_DEPTH_MAX=" << engine->maxPathDepth.depth <<
			" -D PARAM_DEPTH_DIFFUSE_MAX=" << engine->maxPathDepth.diffuseDepth <<
			" -D PARAM_DEPTH_GLOSSY_MAX=" << engine->maxPathDepth.glossyDepth <<
			" -D PARAM_DEPTH_SPECULAR_MAX=" << engine->maxPathDepth.specularDepth <<
			" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filterWidthX << "f" <<
			" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filterWidthY << "f" <<
			" -D PARAM_LOW_LIGHT_THREASHOLD=" << engine->lowLightThreashold << "f" <<
			" -D PARAM_NEAR_START_LIGHT=" << engine->nearStartLight << "f";

	if (engine->useMicroKernels)
		ss << " -D PARAM_MICROKERNELS";
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
		slg::ocl::KernelSource_biaspathocl_funcs;
	
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	if (engine->useMicroKernels)
		ssKernel << slg::ocl::KernelSource_biaspathocl_kernels_micro;
	else
		ssKernel << slg::ocl::KernelSource_biaspathocl_kernels_mega;

	return ssKernel.str();
}

void BiasPathOCLRenderThread::CompileAdditionalKernels(cl::Program *program) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;

	//--------------------------------------------------------------------------
	// InitSeed kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &initSeedKernel, &initSeedWorkGroupSize, "InitSeed");

	//--------------------------------------------------------------------------
	// InitStat kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &initStatKernel, &initStatWorkGroupSize, "InitStat");

	if (engine->useMicroKernels) {
		//----------------------------------------------------------------------
		// RenderSample kernel (Micro-Kernels)
		//----------------------------------------------------------------------

		CompileKernel(program, &renderSampleKernel_MK_GENERATE_CAMERA_RAY,
				&renderSampleWorkGroupSize, "RenderSample_MK_GENERATE_CAMERA_RAY");
		CompileKernel(program, &renderSampleKernel_MK_TRACE_EYE_RAY,
				&renderSampleWorkGroupSize, "RenderSample_MK_TRACE_EYE_RAY");
		CompileKernel(program, &renderSampleKernel_MK_ILLUMINATE_EYE_MISS,
				&renderSampleWorkGroupSize, "RenderSample_MK_ILLUMINATE_EYE_MISS");
	} else {
		//----------------------------------------------------------------------
		// RenderSample kernel (Mega-Kernel)
		//----------------------------------------------------------------------

		CompileKernel(program, &renderSampleKernel, &renderSampleWorkGroupSize, "RenderSample");
	}

	//--------------------------------------------------------------------------
	// MergePixelSamples kernel
	//--------------------------------------------------------------------------

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
		// Additional micro-kenrels field
		(engine->useMicroKernels ?
			(sizeof(int) + sizeof(float) + sizeof(Spectrum) + sizeof(Ray) + sizeof(RayHit)) : 0) +
		// Add Seed memory size
		sizeof(slg::ocl::Seed) +	
		// BSDF (bsdfPathVertex1) size
		GetOpenCLBSDFSize() +
		// PathVolumeInfo (volInfoPathVertex1) size
		(engine->compiledScene->HasVolumes() ? sizeof(slg::ocl::PathVolumeInfo) : 0) +
		// HitPoint (tmpHitPoint) size
		GetOpenCLHitPointSize();
	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] BSDF size: " << GetOpenCLBSDFSize() << "bytes");
	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] HitPoint size: " << GetOpenCLHitPointSize() << "bytes");
	SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] GPUTask size: " << GPUTaskSize << "bytes");

	AllocOCLBufferRW(&tasksBuff, GPUTaskSize * engine->taskCount, "GPUTask");

	const size_t GPUTaskDirectLightSize =
		// BSDF (directLightBSDF) size
		GetOpenCLBSDFSize() +
		// PathVolumeInfo (directLightVolInfo) size
		(engine->compiledScene->HasVolumes() ? sizeof(slg::ocl::PathVolumeInfo) : 0);
	SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] GPUTask DirectLight size: " << GPUTaskDirectLightSize << "bytes");

	AllocOCLBufferRW(&tasksDirectLightBuff, GPUTaskDirectLightSize * engine->taskCount, "GPUTask DirectLight");

	const size_t GPUTaskPathVertexNSize =
		// BSDF (bsdfPathVertexN) size
		GetOpenCLBSDFSize() +
		// PathVolumeInfo (volInfoPathVertexN) size
		(engine->compiledScene->HasVolumes() ? sizeof(slg::ocl::PathVolumeInfo) : 0);
	SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] GPUTask PathVertexN size: " << GPUTaskPathVertexNSize << "bytes");

	AllocOCLBufferRW(&tasksPathVertexNBuff, GPUTaskPathVertexNSize * engine->taskCount, "GPUTask PathVertexN");

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&taskStatsBuff, sizeof(slg::ocl::biaspathocl::GPUTaskStats) * engine->taskCount, "GPUTask Stats");

	//--------------------------------------------------------------------------
	// Allocate GPU task SampleResult
	//--------------------------------------------------------------------------

	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] SampleResult size: " << GetOpenCLSampleResultSize() << "bytes");
	AllocOCLBufferRW(&taskResultsBuff, GetOpenCLSampleResultSize() * engine->taskCount, "GPUTask SampleResult");

	//--------------------------------------------------------------------------
	// Allocate GPU pixel filter distribution
	//--------------------------------------------------------------------------

	AllocOCLBufferRO(&pixelFilterBuff, engine->pixelFilterDistribution,
			sizeof(float) * engine->pixelFilterDistributionSize, "Pixel Filter Distribution");
}

void BiasPathOCLRenderThread::SetRenderSampleKernelArgs(cl::Kernel *rsKernel) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	CompiledScene *cscene = engine->compiledScene;

	u_int argIndex = 0;
	rsKernel->setArg(argIndex++, 0);
	rsKernel->setArg(argIndex++, 0);
	rsKernel->setArg(argIndex++, engine->film->GetWidth());
	rsKernel->setArg(argIndex++, engine->film->GetHeight());
	rsKernel->setArg(argIndex++, *tasksBuff);
	rsKernel->setArg(argIndex++, *tasksDirectLightBuff);
	rsKernel->setArg(argIndex++, *tasksPathVertexNBuff);
	rsKernel->setArg(argIndex++, *taskStatsBuff);
	rsKernel->setArg(argIndex++, *taskResultsBuff);
	rsKernel->setArg(argIndex++, *pixelFilterBuff);

	// Film parameters
	argIndex = SetFilmKernelArgs(*rsKernel, argIndex);

	// Scene parameters
	if (cscene->hasInfiniteLights) {
		rsKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
		rsKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
		rsKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
		rsKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	}
	rsKernel->setArg(argIndex++, *materialsBuff);
	rsKernel->setArg(argIndex++, *texturesBuff);
	rsKernel->setArg(argIndex++, *meshMatsBuff);
	rsKernel->setArg(argIndex++, *meshDescsBuff);
	rsKernel->setArg(argIndex++, *vertsBuff);
	if (normalsBuff)
		rsKernel->setArg(argIndex++, *normalsBuff);
	if (uvsBuff)
		rsKernel->setArg(argIndex++, *uvsBuff);
	if (colsBuff)
		rsKernel->setArg(argIndex++, *colsBuff);
	if (alphasBuff)
		rsKernel->setArg(argIndex++, *alphasBuff);
	rsKernel->setArg(argIndex++, *trianglesBuff);
	rsKernel->setArg(argIndex++, *cameraBuff);
	// Lights
	rsKernel->setArg(argIndex++, *lightsBuff);
	if (envLightIndicesBuff) {
		rsKernel->setArg(argIndex++, *envLightIndicesBuff);
		rsKernel->setArg(argIndex++, (u_int)cscene->envLightIndices.size());
	}
	rsKernel->setArg(argIndex++, *meshTriLightDefsOffsetBuff);
	if (infiniteLightDistributionsBuff)
		rsKernel->setArg(argIndex++, *infiniteLightDistributionsBuff);
	rsKernel->setArg(argIndex++, *lightsDistributionBuff);
	// Images
	if (imageMapDescsBuff) {
		rsKernel->setArg(argIndex++, *imageMapDescsBuff);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			rsKernel->setArg(argIndex++, *(imageMapsBuff[i]));
	}

	argIndex = intersectionDevice->SetIntersectionKernelArgs(*rsKernel, argIndex);
}

void BiasPathOCLRenderThread::SetAdditionalKernelArgs() {
	// Set OpenCL kernel arguments

	// OpenCL kernel setArg() is the only non thread safe function in OpenCL 1.1 so
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

	if (renderSampleKernel)
		SetRenderSampleKernelArgs(renderSampleKernel);
	if (renderSampleKernel_MK_GENERATE_CAMERA_RAY)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_GENERATE_CAMERA_RAY);
	if (renderSampleKernel_MK_TRACE_EYE_RAY)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_TRACE_EYE_RAY);
	if (renderSampleKernel_MK_ILLUMINATE_EYE_MISS)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_ILLUMINATE_EYE_MISS);

	//--------------------------------------------------------------------------
	// mergePixelSamplesKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetWidth());
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetHeight());
	mergePixelSamplesKernel->setArg(argIndex++, *taskResultsBuff);
	mergePixelSamplesKernel->setArg(argIndex++, *taskStatsBuff);
	argIndex = SetFilmKernelArgs(*mergePixelSamplesKernel, argIndex);
}

void BiasPathOCLRenderThread::EnqueueRenderSampleKernel(cl::CommandQueue &oclQueue) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	if (engine->useMicroKernels) {
		// Micro kernels version
		oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_GENERATE_CAMERA_RAY, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
				cl::NDRange(renderSampleWorkGroupSize));
		oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_TRACE_EYE_RAY, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
				cl::NDRange(renderSampleWorkGroupSize));
		oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_ILLUMINATE_EYE_MISS, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
				cl::NDRange(renderSampleWorkGroupSize));
	} else {
		// Mega kernel version
		oclQueue.enqueueNDRangeKernel(*renderSampleKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
				cl::NDRange(renderSampleWorkGroupSize));
	}
}

void BiasPathOCLRenderThread::UpdateRenderSampleKernelArgs(const u_int xStart, const u_int yStart) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);
	
	if (engine->useMicroKernels) {
		renderSampleKernel_MK_GENERATE_CAMERA_RAY->setArg(0, xStart);
		renderSampleKernel_MK_GENERATE_CAMERA_RAY->setArg(1, yStart);
		renderSampleKernel_MK_TRACE_EYE_RAY->setArg(0, xStart);
		renderSampleKernel_MK_TRACE_EYE_RAY->setArg(1, yStart);
		renderSampleKernel_MK_ILLUMINATE_EYE_MISS->setArg(0, xStart);
		renderSampleKernel_MK_ILLUMINATE_EYE_MISS->setArg(1, yStart);
	} else {
		renderSampleKernel->setArg(0, xStart);
		renderSampleKernel->setArg(1, yStart);
	}
	
	mergePixelSamplesKernel->setArg(0, xStart);
	mergePixelSamplesKernel->setArg(1, yStart);
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
		while (engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tile, threadFilm) &&
				!boost::this_thread::interruption_requested()) {
			//const double t0 = WallClockTime();
			threadFilm->Reset();
			//const u_int tileWidth = Min(engine->tileRepository->tileSize, engine->film->GetWidth() - tile->xStart);
			//const u_int tileHeight = Min(engine->tileRepository->tileSize, engine->film->GetHeight() - tile->yStart);
			//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile: "
			//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
			//		"(" << tileWidth << ", " << tileHeight << ")");
				
			// Clear the frame buffer
			oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
				cl::NDRange(filmClearWorkGroupSize));

			// Initialize the statistics
			oclQueue.enqueueNDRangeKernel(*initStatKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initStatWorkGroupSize)),
				cl::NDRange(initStatWorkGroupSize));

			// Render the tile
			UpdateRenderSampleKernelArgs(tile->xStart, tile->yStart);

			// Render all pixel samples
			EnqueueRenderSampleKernel(oclQueue);

			// Merge all pixel samples and accumulate statistics
			oclQueue.enqueueNDRangeKernel(*mergePixelSamplesKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(filmPixelCount, mergePixelSamplesWorkGroupSize)),
					cl::NDRange(mergePixelSamplesWorkGroupSize));

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
			// Statistics are accumulated by MergePixelSample kernel
			const u_int step = engine->tileRepository->totalSamplesPerPixel;
			for (u_int i = 0; i < taskCount; i += step)
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
