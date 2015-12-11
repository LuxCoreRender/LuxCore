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
#include "slg/engines/pathocl/pathocl_datatypes.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathOCLRenderThread
//------------------------------------------------------------------------------

RTPathOCLRenderThread::RTPathOCLRenderThread(const u_int index,
	OpenCLIntersectionDevice *device, PathOCLRenderEngine *re) : 
	PathOCLRenderThread(index, device, re) {
	assignedIters = ((RTPathOCLRenderEngine*)renderEngine)->minIterations;
	frameTime = 0.0;
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

	if (updateActions.Has(CAMERA_EDIT)) {
		// Update Camera
		InitCamera();
	}

	if (updateActions.Has(GEOMETRY_EDIT)) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (updateActions.Has(IMAGEMAPS_EDIT)) {
		// Update Image Maps
		InitImageMaps();
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

	if (updateActions.Has(LIGHTS_EDIT) || updateActions.Has(LIGHT_TYPES_EDIT)) {
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

	if (updateActions.Has(MATERIAL_TYPES_EDIT) ||
			updateActions.Has(LIGHT_TYPES_EDIT))
		InitKernels();

	SetKernelArgs();

	//--------------------------------------------------------------------------
	// Execute initialization kernels
	//--------------------------------------------------------------------------

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear the thread film
	threadFilms[0]->ClearFilm(oclQueue, *filmClearKernel, filmClearWorkGroupSize);

	// Initialize the tasks buffer
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(engine->taskCount, initWorkGroupSize)),
			cl::NDRange(initWorkGroupSize));

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void RTPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	// Boost barriers are supposed to be non interruptible but they are
	// and seem to be missing a way to reset them. So better to disable
	// interruptions.
	boost::this_thread::disable_interruption di;

	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;
	boost::barrier *frameBarrier = engine->frameBarrier;

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		cl::CommandQueue &initQueue = intersectionDevice->GetOpenCLQueue();

		// Clear the frame buffer
		const u_int filmPixelCount = threadFilms[0]->film->GetWidth() * threadFilms[0]->film->GetHeight();
		initQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
				cl::NDRange(filmClearWorkGroupSize));

		// Initialize the tasks buffer
		initQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(engine->taskCount, initWorkGroupSize)),
				cl::NDRange(initWorkGroupSize));

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		bool pendingFilmClear = false;
		while (!boost::this_thread::interruption_requested()) {
			cl::CommandQueue &currentQueue = intersectionDevice->GetOpenCLQueue();

			//------------------------------------------------------------------
			// Render a frame (i.e. taskCount * assignedIters samples)
			//------------------------------------------------------------------
			const double startTime = WallClockTime();
			u_int iterations = assignedIters;

			for (u_int i = 0; i < iterations; ++i) {
				// Trace rays
				intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
					*(hitsBuff), engine->taskCount, NULL, NULL);

				// Advance to next path state
				EnqueueAdvancePathsKernel(currentQueue);
			}

			// I async. transfer the frame buffer even if I'm the display device in
			// order to support AOVs
			threadFilms[0]->TransferFilm(currentQueue);

			// Async. transfer of GPU task statistics
			currentQueue.enqueueReadBuffer(*(taskStatsBuff), CL_FALSE, 0,
				sizeof(slg::ocl::pathocl::GPUTaskStats) * engine->taskCount, gpuTaskStats);

			currentQueue.finish();
			frameTime = WallClockTime() - startTime;

			//------------------------------------------------------------------
			frameBarrier->wait();
			//------------------------------------------------------------------

			if (threadIndex == 0) {
				boost::unique_lock<boost::mutex> lock(*(engine->filmMutex));

				if (pendingFilmClear) {
					engine->film->Reset();
					pendingFilmClear = false;
				}

				// To merge all device films for AOVs support
				engine->MergeThreadFilms();
			}

			//------------------------------------------------------------------
			frameBarrier->wait();
			//------------------------------------------------------------------

			//------------------------------------------------------------------
			// Update OpenCL buffers if there is any edit action
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

					// Clear the film (on the next frame)
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
