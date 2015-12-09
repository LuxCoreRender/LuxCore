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
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/linear.h"
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"
#include "slg/engines/biaspathocl/biaspathocl_datatypes.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTBiasPathOCLRenderThread
//------------------------------------------------------------------------------

RTBiasPathOCLRenderThread::RTBiasPathOCLRenderThread(const u_int index,
	OpenCLIntersectionDevice *device, BiasPathOCLRenderEngine *re) : 
	BiasPathOCLRenderThread(index, device, re) {
}

RTBiasPathOCLRenderThread::~RTBiasPathOCLRenderThread() {
}

void RTBiasPathOCLRenderThread::Interrupt() {
}

void RTBiasPathOCLRenderThread::BeginSceneEdit() {
}

void RTBiasPathOCLRenderThread::EndSceneEdit(const EditActionList &editActions) {
}

string RTBiasPathOCLRenderThread::AdditionalKernelOptions() {
	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			BiasPathOCLRenderThread::AdditionalKernelOptions() <<
			" -D PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION=" << engine->resolutionReduction;

	return ss.str();
}

void RTBiasPathOCLRenderThread::UpdateOCLBuffers(const EditActionList &updateActions) {
	//--------------------------------------------------------------------------
	// Update OpenCL buffers
	//--------------------------------------------------------------------------

	if (updateActions.Has(CAMERA_EDIT)) {
		// Update Camera
		InitCamera();
	}

	if (updateActions.Has(GEOMETRY_EDIT)) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (updateActions.Has(MATERIALS_EDIT) || updateActions.Has(MATERIAL_TYPES_EDIT)) {
		// Update Scene Textures and Materials
		InitTextures();
		InitMaterials();
	}

	if (updateActions.Has(GEOMETRY_EDIT) ||
			updateActions.Has(MATERIALS_EDIT) || updateActions.Has(MATERIAL_TYPES_EDIT)) {
		// Update Mesh <=> Material links
		InitMeshMaterials();
	}

	if (updateActions.Has(LIGHTS_EDIT)) {
		// Update Scene Lights
		InitLights();
	}

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	if (updateActions.Has(MATERIAL_TYPES_EDIT))
		InitKernels();

	SetKernelArgs();

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void RTBiasPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	// Boost barriers are supposed to be not interruptible but they are
	// and seem to be missing a way to reset them. So better to disable
	// interruptions.
	boost::this_thread::disable_interruption di;

	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;
	boost::barrier *frameBarrier = engine->frameBarrier;
	// To synchronize the start of all threads
	frameBarrier->wait();

	const u_int tileWidth = engine->tileRepository->tileWidth;
	const u_int tileHeight = engine->tileRepository->tileHeight;
	const u_int threadFilmPixelCount = tileWidth * tileHeight;
	const u_int taskCount = engine->taskCount;

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		cl::CommandQueue &initQueue = intersectionDevice->GetOpenCLQueue();

		// Initialize OpenCL structures
		initQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initSeedWorkGroupSize)),
				cl::NDRange(initSeedWorkGroupSize));

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		bool pendingFilmClear = false;
		tile = NULL;
		while (!boost::this_thread::interruption_requested()) {
			cl::CommandQueue &currentQueue = intersectionDevice->GetOpenCLQueue();

			//------------------------------------------------------------------
			// Render the tile (there is only one tile for each device
			// in RTBIASPATHOCL)
			//------------------------------------------------------------------

			engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tile, threadFilms[0]->film);

			// tile can be NULL after a scene edit
			if (tile) {
				//const double t0 = WallClockTime();
				threadFilms[0]->film->Reset();
				//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Tile: "
				//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
				//		"(" << tile->tileWidth << ", " << tile->tileHeight << ")");

				// Clear the frame buffer
				threadFilms[0]->ClearFilm(currentQueue, *filmClearKernel, filmClearWorkGroupSize);

				// Initialize the statistics
				currentQueue.enqueueNDRangeKernel(*initStatKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(taskCount, initStatWorkGroupSize)),
					cl::NDRange(initStatWorkGroupSize));

				// Render the tile
				UpdateKernelArgsForTile(tile, 0);

				// Render all pixel samples
				EnqueueRenderSampleKernel(currentQueue);

				// Merge all pixel samples and accumulate statistics
				currentQueue.enqueueNDRangeKernel(*mergePixelSamplesKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(threadFilmPixelCount, mergePixelSamplesWorkGroupSize)),
						cl::NDRange(mergePixelSamplesWorkGroupSize));

				// Async. transfer of the Film buffers
				threadFilms[0]->TransferFilm(currentQueue);
				threadFilms[0]->film->AddSampleCount(taskCount);

				// Async. transfer of GPU task statistics
				currentQueue.enqueueReadBuffer(
					*(taskStatsBuff),
					CL_FALSE,
					0,
					sizeof(slg::ocl::biaspathocl::GPUTaskStats) * engine->taskCount,
					gpuTaskStats);

				currentQueue.finish();

				// In order to update the statistics
				u_int tracedRaysCount = 0;
				// Statistics are accumulated by MergePixelSample kernel if not enableProgressiveRefinement
				const u_int step = engine->aaSamples * engine->aaSamples;
				for (u_int i = 0; i < taskCount; i += step)
					tracedRaysCount += gpuTaskStats[i].raysCount;

				intersectionDevice->IntersectionKernelExecuted(tracedRaysCount);

				//const double t1 = WallClockTime();
				//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Tile rendering time: " + ToString((u_int)((t1 - t0) * 1000.0)) + "ms");
			}

			//------------------------------------------------------------------
			frameBarrier->wait();
			//------------------------------------------------------------------

			if (threadIndex == 0) {
				// Clear the film if pendingFilmClear or I'm rendering the very first pass
				// without resolution reduction
				const u_int resolutionReduction = tile ? (engine->resolutionReduction >> Min(tile->pass, 16u)) : 1;
				if (pendingFilmClear || (resolutionReduction == 1)) {
					boost::unique_lock<boost::mutex> lock(*(engine->filmMutex));
					engine->film->Reset();
					pendingFilmClear = false;
				}

				// Now I can splat the tile on the tile repository. It is done now to
				// not obliterate the CHANNEL_RGB_TONEMAPPED while the screen refresh
				// thread is probably using it to draw the screen.
				// It is also done by thread #0 for all threads.
				for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
					RTBiasPathOCLRenderThread *thread = (RTBiasPathOCLRenderThread *)(engine->renderThreads[i]);

					if (thread->tile) {
						engine->tileRepository->NextTile(engine->film, engine->filmMutex, &thread->tile, thread->threadFilms[0]->film);

						// There is only one tile for each device in RTBIASPATHOCL
						thread->tile = NULL;
					}
				}
			}

			//------------------------------------------------------------------
			frameBarrier->wait();
			//------------------------------------------------------------------

			//------------------------------------------------------------------
			// Update OpenCL buffers if there is any edit action. It is done by
			// thread #0 for all threads.
			//------------------------------------------------------------------

			if (threadIndex == 0) {
				if (engine->updateActions.HasAnyAction()) {
					// Update all threads
					for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
						RTBiasPathOCLRenderThread *thread = (RTBiasPathOCLRenderThread *)(engine->renderThreads[i]);
						thread->UpdateOCLBuffers(engine->updateActions);
					}

					// Reset updateActions
					engine->updateActions.Reset();

					// Clear the film
					pendingFilmClear = true;
				}
			}

			//------------------------------------------------------------------
			frameBarrier->wait();
			//------------------------------------------------------------------

			// Time to render a new frame
		}
		//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error &err) {
		SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}

	intersectionDevice->GetOpenCLQueue().finish();
}

#endif
