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
	PathOCLBaseRenderThread(index, device, re) {
	initSeedKernel = NULL;
	initStatKernel = NULL;
	renderSampleKernel_MK_GENERATE_CAMERA_RAY = NULL;
	renderSampleKernel_MK_TRACE_EYE_RAY = NULL;
	renderSampleKernel_MK_ILLUMINATE_EYE_MISS = NULL;
	renderSampleKernel_MK_ILLUMINATE_EYE_HIT = NULL;
	renderSampleKernel_MK_DL_VERTEX_1 = NULL;
	renderSampleKernel_MK_BSDF_SAMPLE_DIFFUSE = NULL;
	renderSampleKernel_MK_BSDF_SAMPLE_GLOSSY = NULL;
	renderSampleKernel_MK_BSDF_SAMPLE_SPECULAR = NULL;
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
	delete renderSampleKernel_MK_GENERATE_CAMERA_RAY;
	delete renderSampleKernel_MK_TRACE_EYE_RAY;
	delete renderSampleKernel_MK_ILLUMINATE_EYE_MISS;
	delete renderSampleKernel_MK_ILLUMINATE_EYE_HIT;
	delete renderSampleKernel_MK_DL_VERTEX_1;
	delete renderSampleKernel_MK_BSDF_SAMPLE_DIFFUSE;
	delete renderSampleKernel_MK_BSDF_SAMPLE_GLOSSY;
	delete renderSampleKernel_MK_BSDF_SAMPLE_SPECULAR;
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
	*filmWidth = engine->tileRepository->tileWidth;
	*filmHeight = engine->tileRepository->tileHeight;
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
			" -D PARAM_TILE_WIDTH=" << engine->tileRepository->tileWidth <<
			" -D PARAM_TILE_HEIGHT=" << engine->tileRepository->tileHeight <<
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
			slg::ocl::KernelSource_biaspathocl_funcs <<
			slg::ocl::KernelSource_biaspathocl_kernels_micro;

	return ssKernel.str();
}

void BiasPathOCLRenderThread::CompileAdditionalKernels(cl::Program *program) {
	//--------------------------------------------------------------------------
	// InitSeed kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &initSeedKernel, &initSeedWorkGroupSize, "InitSeed");

	//--------------------------------------------------------------------------
	// InitStat kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &initStatKernel, &initStatWorkGroupSize, "InitStat");

	//--------------------------------------------------------------------------
	// RenderSample kernel (Micro-Kernels)
	//--------------------------------------------------------------------------

	CompileKernel(program, &renderSampleKernel_MK_GENERATE_CAMERA_RAY,
			&renderSampleWorkGroupSize, "RenderSample_MK_GENERATE_CAMERA_RAY");
	CompileKernel(program, &renderSampleKernel_MK_TRACE_EYE_RAY,
			&renderSampleWorkGroupSize, "RenderSample_MK_TRACE_EYE_RAY");
	CompileKernel(program, &renderSampleKernel_MK_ILLUMINATE_EYE_MISS,
			&renderSampleWorkGroupSize, "RenderSample_MK_ILLUMINATE_EYE_MISS");
	CompileKernel(program, &renderSampleKernel_MK_ILLUMINATE_EYE_HIT,
			&renderSampleWorkGroupSize, "RenderSample_MK_ILLUMINATE_EYE_HIT");
	CompileKernel(program, &renderSampleKernel_MK_DL_VERTEX_1,
			&renderSampleWorkGroupSize, "RenderSample_MK_DL_VERTEX_1");
	CompileKernel(program, &renderSampleKernel_MK_BSDF_SAMPLE_DIFFUSE,
			&renderSampleWorkGroupSize, "RenderSample_MK_BSDF_SAMPLE_DIFFUSE");
	CompileKernel(program, &renderSampleKernel_MK_BSDF_SAMPLE_GLOSSY,
			&renderSampleWorkGroupSize, "RenderSample_MK_BSDF_SAMPLE_GLOSSY");
	CompileKernel(program, &renderSampleKernel_MK_BSDF_SAMPLE_SPECULAR,
			&renderSampleWorkGroupSize, "RenderSample_MK_BSDF_SAMPLE_SPECULAR");

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
		// Additional micro-kernels field
		(sizeof(int) + sizeof(float) + sizeof(int) + sizeof(Spectrum) + sizeof(Ray) + sizeof(RayHit)) +
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

void BiasPathOCLRenderThread::SetRenderSampleKernelArgs(cl::Kernel *rsKernel, bool firstKernel) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	CompiledScene *cscene = engine->compiledScene;

	u_int argIndex = 0;
	if (firstKernel) {
		rsKernel->setArg(argIndex++, 0);
		rsKernel->setArg(argIndex++, 0);
	}
	rsKernel->setArg(argIndex++, engine->film->GetWidth());
	rsKernel->setArg(argIndex++, engine->film->GetHeight());
	rsKernel->setArg(argIndex++, *tasksBuff);
	rsKernel->setArg(argIndex++, *tasksDirectLightBuff);
	rsKernel->setArg(argIndex++, *tasksPathVertexNBuff);
	rsKernel->setArg(argIndex++, *taskStatsBuff);
	rsKernel->setArg(argIndex++, *taskResultsBuff);
	rsKernel->setArg(argIndex++, *pixelFilterBuff);

	// Film parameters
	argIndex = threadFilms[0]->SetFilmKernelArgs(*rsKernel, argIndex);

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

	if (renderSampleKernel_MK_GENERATE_CAMERA_RAY)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_GENERATE_CAMERA_RAY, true);
	if (renderSampleKernel_MK_TRACE_EYE_RAY)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_TRACE_EYE_RAY, false);
	if (renderSampleKernel_MK_ILLUMINATE_EYE_MISS)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_ILLUMINATE_EYE_MISS, false);
	if (renderSampleKernel_MK_ILLUMINATE_EYE_HIT)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_ILLUMINATE_EYE_HIT, false);
	if (renderSampleKernel_MK_DL_VERTEX_1)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_DL_VERTEX_1, false);
	if (renderSampleKernel_MK_BSDF_SAMPLE_DIFFUSE)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_BSDF_SAMPLE_DIFFUSE, false);
	if (renderSampleKernel_MK_BSDF_SAMPLE_GLOSSY)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_BSDF_SAMPLE_GLOSSY, false);
	if (renderSampleKernel_MK_BSDF_SAMPLE_SPECULAR)
		SetRenderSampleKernelArgs(renderSampleKernel_MK_BSDF_SAMPLE_SPECULAR, false);

	//--------------------------------------------------------------------------
	// mergePixelSamplesKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, 0);
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetWidth());
	mergePixelSamplesKernel->setArg(argIndex++, engine->film->GetHeight());
	mergePixelSamplesKernel->setArg(argIndex++, *taskResultsBuff);
	argIndex = threadFilms[0]->SetFilmKernelArgs(*mergePixelSamplesKernel, argIndex);
}

void BiasPathOCLRenderThread::EnqueueRenderSampleKernel(cl::CommandQueue &oclQueue) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

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
	oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_ILLUMINATE_EYE_HIT, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
			cl::NDRange(renderSampleWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_DL_VERTEX_1, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
			cl::NDRange(renderSampleWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_BSDF_SAMPLE_DIFFUSE, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
			cl::NDRange(renderSampleWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_BSDF_SAMPLE_GLOSSY, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
			cl::NDRange(renderSampleWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*renderSampleKernel_MK_BSDF_SAMPLE_SPECULAR, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
			cl::NDRange(renderSampleWorkGroupSize));
}

void BiasPathOCLRenderThread::UpdateKernelArgsForTile(const u_int xStart, const u_int yStart,
		const u_int filmIndex) {
	BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
	boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);
	
	// Update renderSampleKernel args
	renderSampleKernel_MK_GENERATE_CAMERA_RAY->setArg(0, xStart);
	renderSampleKernel_MK_GENERATE_CAMERA_RAY->setArg(1, yStart);

	// Update mergePixelSamplesKernel args
	mergePixelSamplesKernel->setArg(0, xStart);
	mergePixelSamplesKernel->setArg(1, yStart);
	threadFilms[filmIndex]->SetFilmKernelArgs(*mergePixelSamplesKernel, 5);
}

void BiasPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	try {
		//----------------------------------------------------------------------
		// Initialization
		//----------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)renderEngine;
		const u_int tileWidth = engine->tileRepository->tileWidth;
		const u_int tileHeight = engine->tileRepository->tileHeight;
		const u_int filmPixelCount = tileWidth * tileHeight;
		const u_int taskCount = engine->taskCount;

		// Initialize OpenCL structures
		oclQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initSeedWorkGroupSize)),
				cl::NDRange(initSeedWorkGroupSize));

		//----------------------------------------------------------------------
		// Extract the tile to render
		//----------------------------------------------------------------------

		vector<TileRepository::Tile *> tiles(1, NULL);
		while (!boost::this_thread::interruption_requested()) {
			const double t0 = WallClockTime();

			// Enqueue the rendering of all tiles

			// Initialize the statistics
			oclQueue.enqueueNDRangeKernel(*initStatKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initStatWorkGroupSize)),
				cl::NDRange(initStatWorkGroupSize));

			bool allTileDone = true;
			for (u_int i = 0; i < tiles.size(); ++i) {
				if (engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tiles[i], threadFilms[i]->film)) {
					//const u_int tileW = Min(engine->tileRepository->tileWidth, engine->film->GetWidth() - tiles[i]->xStart);
					//const u_int tileH = Min(engine->tileRepository->tileHeight, engine->film->GetHeight() - tiles[i]->yStart);
					//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile: "
					//		"(" << tiles[i]->xStart << ", " << tiles[i]->yStart << ") => " <<
					//		"(" << tiles[i]->tileWidth << ", " << tiles[i]->tileWidth << ")");

					threadFilms[i]->film->Reset();

					// Clear the frame buffer
					threadFilms[i]->ClearFilm(oclQueue, *filmClearKernel, filmClearWorkGroupSize);

					// Render the tile
					UpdateKernelArgsForTile(tiles[i]->xStart, tiles[i]->yStart, i);

					// Render all pixel samples
					EnqueueRenderSampleKernel(oclQueue);

					// Merge all pixel samples
					oclQueue.enqueueNDRangeKernel(*mergePixelSamplesKernel, cl::NullRange,
							cl::NDRange(RoundUp<u_int>(filmPixelCount, mergePixelSamplesWorkGroupSize)),
							cl::NDRange(mergePixelSamplesWorkGroupSize));
					
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
				sizeof(slg::ocl::biaspathocl::GPUTaskStats) * engine->taskCount,
				gpuTaskStats);

			oclQueue.finish();

			// In order to update the statistics
			u_int tracedRaysCount = 0;
			for (u_int i = 0; i < taskCount; ++i)
				tracedRaysCount += gpuTaskStats[i].raysCount;
			intersectionDevice->IntersectionKernelExecuted(tracedRaysCount);

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
	} catch (cl::Error err) {
		SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}
}

#endif
