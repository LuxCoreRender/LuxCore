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
#include "slg/engines/rtpathocl/rtpathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderThread
//------------------------------------------------------------------------------

RTPathOCLRenderThread::RTPathOCLRenderThread(const u_int index,
	OpenCLIntersectionDevice *device, TilePathOCLRenderEngine *re) : 
	TilePathOCLRenderThread(index, device, re) {
}

RTPathOCLRenderThread::~RTPathOCLRenderThread() {
}

void RTPathOCLRenderThread::Interrupt() {
}

void RTPathOCLRenderThread::BeginSceneEdit() {
}

void RTPathOCLRenderThread::EndSceneEdit(const EditActionList &editActions) {
}

string RTPathOCLRenderThread::AdditionalKernelOptions() {
	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			TilePathOCLRenderThread::AdditionalKernelOptions() <<
			" -D PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION=" << engine->previewResolutionReduction <<
			" -D PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP=" << engine->previewResolutionReductionStep <<
			" -D PARAM_RTPATHOCL_RESOLUTION_REDUCTION=" << engine->resolutionReduction;

	return ss.str();
}

void RTPathOCLRenderThread::UpdateOCLBuffers(const EditActionList &updateActions) {
	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

	//--------------------------------------------------------------------------
	// Update OpenCL buffers
	//--------------------------------------------------------------------------

	CompiledScene *cscene = engine->compiledScene;

	if (cscene->wasCameraCompiled) {
		// Update Camera
		InitCamera();
	}

	if (cscene->wasGeometryCompiled) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (cscene->wasImageMapsCompiled) {
		// Update Image Maps
		InitImageMaps();
	}

	if (cscene->wasMaterialsCompiled) {
		// Update Scene Textures and Materials
		InitTextures();
		InitMaterials();
	}

	if (cscene->wasSceneObjectsCompiled) {
		// Update Mesh <=> Material relation
		InitSceneObjects();
	}

	if  (cscene->wasLightsCompiled) {
		// Update Scene Lights
		InitLights();
	}

	// A material types edit can enable/disable PARAM_HAS_PASSTHROUGH parameter
	// and change the size of the structure allocated
	if (updateActions.Has(MATERIAL_TYPES_EDIT))
		AdditionalInit();

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	// The following actions can require a kernel re-compilation:
	// - Dynamic code generation of textures and materials;
	// - Material types edit;
	// - Light types edit;
	// - Image types edit;
	// - Geometry type edit;
	// - etc.
	InitKernels();

	SetKernelArgs();
	
	if (updateActions.Has(MATERIAL_TYPES_EDIT) ||
			updateActions.Has(LIGHT_TYPES_EDIT)) {
		// Execute initialization kernels. Initialize OpenCL structures.
		// NOTE: I can only after having compiled and set arguments.
		cl::CommandQueue &initQueue = intersectionDevice->GetOpenCLQueue();
		RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

		initQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(engine->taskCount), cl::NDRange(initWorkGroupSize));
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void RTPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	// Boost barriers are supposed to be not interruptible but they are
	// and seem to be missing a way to reset them. So better to disable
	// interruptions.
	boost::this_thread::disable_interruption di;

	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;
	boost::barrier *frameBarrier = engine->frameBarrier;
	// To synchronize the start of all threads
	frameBarrier->wait();

	const u_int taskCount = engine->taskCount;

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		cl::CommandQueue &initQueue = intersectionDevice->GetOpenCLQueue();

		// Initialize OpenCL structures
		initQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(taskCount), cl::NDRange(initWorkGroupSize));

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		bool pendingFilmClear = false;
		tile = NULL;
		while (!boost::this_thread::interruption_requested()) {
			cl::CommandQueue &currentQueue = intersectionDevice->GetOpenCLQueue();

			//------------------------------------------------------------------
			// Render the tile (there is only one tile for each device
			// in RTPATHOCL)
			//------------------------------------------------------------------

			engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tile, threadFilms[0]->film);

			// tile can be NULL after a scene edit
			if (tile) {
				//const double t0 = WallClockTime();
				//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Tile: "
				//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
				//		"(" << tile->tileWidth << ", " << tile->tileHeight << ")");

				RenderTile(tile, 0);

				// Async. transfer of GPU task statistics
				currentQueue.enqueueReadBuffer(
					*(taskStatsBuff),
					CL_FALSE,
					0,
					sizeof(slg::ocl::pathoclstatebase::GPUTaskStats) * taskCount,
					gpuTaskStats);

				currentQueue.finish();

				//const double t1 = WallClockTime();
				//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Tile rendering time: " + ToString((u_int)((t1 - t0) * 1000.0)) + "ms");
			}

			//------------------------------------------------------------------
			frameBarrier->wait();
			//------------------------------------------------------------------

			if (threadIndex == 0) {
				//const double t0 = WallClockTime();

				if (pendingFilmClear) {
					boost::unique_lock<boost::mutex> lock(*(engine->filmMutex));
					engine->film->Reset();
					pendingFilmClear = false;
				}

				// Now I can splat the tile on the tile repository. It is done now to
				// not obliterate the CHANNEL_IMAGEPIPELINE while the screen refresh
				// thread is probably using it to draw the screen.
				// It is also done by thread #0 for all threads.
				for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
					RTPathOCLRenderThread *thread = (RTPathOCLRenderThread *)(engine->renderThreads[i]);

					if (thread->tile) {
						engine->tileRepository->NextTile(engine->film, engine->filmMutex, &thread->tile, thread->threadFilms[0]->film);

						// There is only one tile for each device in RTPATHOCL
						thread->tile = NULL;
					}
				}

				//const double t1 = WallClockTime();
				//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Tile splatting time: " + ToString((u_int)((t1 - t0) * 1000.0)) + "ms");
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
						RTPathOCLRenderThread *thread = (RTPathOCLRenderThread *)(engine->renderThreads[i]);
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
		//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error &err) {
		SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}

	intersectionDevice->GetOpenCLQueue().finish();
}

#endif
