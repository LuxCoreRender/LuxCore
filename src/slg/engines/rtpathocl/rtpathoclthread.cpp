/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/devices/ocldevice.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/linear.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/rtpathcpu/rtpathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderThread
//------------------------------------------------------------------------------

RTPathOCLRenderThread::RTPathOCLRenderThread(const u_int index,
	HardwareIntersectionDevice *device, TilePathOCLRenderEngine *re) : 
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
		RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

		intersectionDevice->EnqueueKernel(initSeedKernel,
				HardwareDeviceRange(engine->taskCount), HardwareDeviceRange(initWorkGroupSize));
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void RTPathOCLRenderThread::UpdateAllThreadsOCLBuffers() {
	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

	if (engine->updateActions.HasAnyAction()) {
		// Update all threads
		for (u_int i = 0; i < engine->renderOCLThreads.size(); ++i) {
			RTPathOCLRenderThread *thread = (RTPathOCLRenderThread *)(engine->renderOCLThreads[i]);
			thread->intersectionDevice->PushThreadCurrentDevice();
			thread->UpdateOCLBuffers(engine->updateActions);
			thread->intersectionDevice->PopThreadCurrentDevice();
		}

		// Reset updateActions
		engine->updateActions.Reset();
	}
}

void RTPathOCLRenderThread::UpdateCameraOCLBuffer() {
	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

	intersectionDevice->EnqueueWriteBuffer(cameraBuff, false, sizeof(slg::ocl::Camera),
			&engine->compiledScene->camera);
}

void RTPathOCLRenderThread::UpdateAllCameraThreadsOCLBuffers() {
	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

	// Update all threads
	for (u_int i = 0; i < engine->renderOCLThreads.size(); ++i) {
		RTPathOCLRenderThread *thread = (RTPathOCLRenderThread *)(engine->renderOCLThreads[i]);
		thread->intersectionDevice->PushThreadCurrentDevice();
		thread->UpdateCameraOCLBuffer();
		thread->intersectionDevice->PopThreadCurrentDevice();
	}
}

void RTPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	// Boost barriers are supposed to be not interruptible but they are
	// and seem to be missing a way to reset them. So better to disable
	// interruptions.
	boost::this_thread::disable_interruption di;

	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;
	boost::barrier *syncBarrier = engine->syncBarrier;
	boost::barrier *frameBarrier = engine->frameBarrier;

	intersectionDevice->PushThreadCurrentDevice();

	// To synchronize with RTPathOCLRenderEngine::StartLockLess()
	if (threadIndex == 0)
		syncBarrier->wait();

	// To synchronize the start of all threads. frameBarrier can be nullptr if
	// there is only one rendering threads.
	if (frameBarrier)
		frameBarrier->wait();

	const u_int taskCount = engine->taskCount;

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		// Initialize OpenCL structures
		intersectionDevice->EnqueueKernel(initSeedKernel,
				HardwareDeviceRange(taskCount), HardwareDeviceRange(initWorkGroupSize));

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		double frameStartTime = WallClockTime();
		u_int frameCounter = 0;
		tileWork.Reset();
		slg::ocl::TilePathSamplerSharedData samplerData;

		while (!boost::this_thread::interruption_requested()) {
			//------------------------------------------------------------------
			// Render the tile (there is only one tile for each device
			// in RTPATHOCL)
			//------------------------------------------------------------------

			engine->tileRepository->NextTile(engine->film, engine->filmMutex, tileWork, threadFilms[0]->film);

			// tileWork can be NULL after a scene edit
			if (tileWork.HasWork()) {
				//const double t0 = WallClockTime();
				//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] TileWork: " << tileWork);

				RenderTileWork(tileWork, samplerData, 0);

				// Async. transfer of GPU task statistics
				intersectionDevice->EnqueueReadBuffer(
					taskStatsBuff,
					CL_FALSE,
					sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount,
					gpuTaskStats);
				intersectionDevice->FinishQueue();

				engine->tileRepository->NextTile(engine->film, engine->filmMutex, tileWork, threadFilms[0]->film);

				// There is only one tile for each device in RTPATHOCL
				tileWork.Reset();

				//const double t1 = WallClockTime();
				//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Tile rendering time: " + ToString((u_int)((t1 - t0) * 1000.0)) + "ms");
			}

			//------------------------------------------------------------------
			if (frameBarrier)
				frameBarrier->wait();
			//------------------------------------------------------------------

			if (threadIndex == 0) {
				//--------------------------------------------------------------
				// Update frame time
				//--------------------------------------------------------------

				const double now = WallClockTime();
				engine->frameTime = now - frameStartTime;
				frameStartTime = now;
				
				//--------------------------------------------------------------
				// This is a special optimized path for CAMERA_EDIT action only
				// (when custom camera bokeh is not used)
				//--------------------------------------------------------------
				
				if (engine->useFastCameraEditPath) {
					engine->useFastCameraEditPath = false;

					// Update OpenCL camera buffer if there is only a CAMERA_EDIT. It
					// is done by thread #0 for all threads.
					UpdateAllCameraThreadsOCLBuffers();
					frameCounter = 0;
					engine->film->Reset(true);					
				}

				//--------------------------------------------------------------
				// Check if there is a sync. request from the main thread
				//--------------------------------------------------------------

				for (;;) {
					bool requestedStop = false;
					switch (engine->syncType) {
						case SYNCTYPE_NONE:
							break;
						case SYNCTYPE_BEGINFILMEDIT:
						case SYNCTYPE_STOP:
							syncBarrier->wait();
							// The main thread send an interrupt to all render threads
							syncBarrier->wait();
							requestedStop = true;
							break;
						case SYNCTYPE_ENDSCENEEDIT:
							syncBarrier->wait();

							// Engine thread compile the scene

							syncBarrier->wait();

							// Update OpenCL buffers if there is any edit action. It
							// is done by thread #0 for all threads.
							UpdateAllThreadsOCLBuffers();
							frameCounter = 0;
							engine->film->Reset(true);

							syncBarrier->wait();
							break;
						default:
							throw runtime_error("Unknown sync. type in RTPathOCLRenderThread::RenderThreadImpl(): " + ToString(engine->syncType));
					}

					// Check if we are in pause mode
					if (engine->pauseMode) {
						if (requestedStop)
							break;

						boost::this_thread::sleep(boost::posix_time::millisec(100));
					} else
						break;
				}

				// Re-initialize the tile queue for the next frame
				engine->tileRepository->Restart(engine->film, frameCounter++);
			}

			//------------------------------------------------------------------
			if (frameBarrier)
				frameBarrier->wait();
			//------------------------------------------------------------------

			// Time to render a new frame
		}
		//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	}

	intersectionDevice->FinishQueue();

	threadDone = true;
	
	intersectionDevice->PopThreadCurrentDevice();
}

#endif
