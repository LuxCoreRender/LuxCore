/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#include "slg.h"
#include "pathocl/rtpathocl.h"
#include "luxrays/opencl/intersectiondevice.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

//------------------------------------------------------------------------------
// RTPathOCLRenderThread
//------------------------------------------------------------------------------

RTPathOCLRenderThread::RTPathOCLRenderThread(const u_int index,
	OpenCLIntersectionDevice *device, PathOCLRenderEngine *re) : 
	PathOCLRenderThread(index, device, re) {
	tmpFrameBufferBuff = NULL;
	mergedFrameBufferBuff = NULL;
	screenBufferBuff = NULL;
	assignedIters = ((RTPathOCLRenderEngine*)renderEngine)->minIterations;
	frameTime = 0.0;
}

RTPathOCLRenderThread::~RTPathOCLRenderThread() {
}

void RTPathOCLRenderThread::Interrupt() {
}

void RTPathOCLRenderThread::Stop() {
	PathOCLRenderThread::Stop();

	FreeOCLBuffer(&tmpFrameBufferBuff);
	FreeOCLBuffer(&mergedFrameBufferBuff);
	FreeOCLBuffer(&screenBufferBuff);
}

void RTPathOCLRenderThread::BeginEdit() {
	editMutex.lock();
}

void RTPathOCLRenderThread::EndEdit(const EditActionList &editActions) {
	if (editActions.Has(FILM_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT)) {
		editMutex.unlock();
		StopRenderThread();

		updateActions.AddActions(editActions.GetActions());
		UpdateOCLBuffers();
		StartRenderThread();
	} else {
		updateActions.AddActions(editActions.GetActions());
		editMutex.unlock();
	}
}

void RTPathOCLRenderThread::InitDisplayThread() {
	frameBufferPixelCount =	(renderEngine->film->GetWidth() + 2) * (renderEngine->film->GetHeight() + 2);
	AllocOCLBufferRW(&tmpFrameBufferBuff, sizeof(luxrays::ocl::Pixel) * frameBufferPixelCount, "Tmp FrameBuffer");
	AllocOCLBufferRW(&mergedFrameBufferBuff, sizeof(luxrays::ocl::Pixel) * frameBufferPixelCount, "Merged FrameBuffer");

	AllocOCLBufferRW(&screenBufferBuff, sizeof(Spectrum) * renderEngine->film->GetWidth() *
			renderEngine->film->GetHeight(), "Screen FrameBuffer");
}

void RTPathOCLRenderThread::InitRender() {
	if (((RTPathOCLRenderEngine *)renderEngine)->displayDeviceIndex == threadIndex)
		InitDisplayThread();

	PathOCLRenderThread::InitRender();
}

void RTPathOCLRenderThread::SetKernelArgs() {
	PathOCLRenderThread::SetKernelArgs();

	if (((RTPathOCLRenderEngine *)renderEngine)->displayDeviceIndex == threadIndex) {
		boost::unique_lock<boost::mutex> lock(renderEngine->setKernelArgsMutex);

		u_int argIndex = 0;
		clearFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		normalizeFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		applyBlurLightFilterXR1Kernel->setArg(argIndex++, *mergedFrameBufferBuff);
		applyBlurLightFilterXR1Kernel->setArg(argIndex++, *tmpFrameBufferBuff);
		argIndex = 0;
		applyBlurLightFilterYR1Kernel->setArg(argIndex++, *tmpFrameBufferBuff);
		applyBlurLightFilterYR1Kernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		toneMapLinearKernel->setArg(argIndex++, *mergedFrameBufferBuff);
		toneMapLinearKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		updateScreenBufferKernel->setArg(argIndex++, *mergedFrameBufferBuff);
		updateScreenBufferKernel->setArg(argIndex++, *screenBufferBuff);

	}
}

void RTPathOCLRenderThread::UpdateOCLBuffers() {
	editMutex.lock();
	
	const bool amiDisplayThread = (((RTPathOCLRenderEngine *)renderEngine)->displayDeviceIndex == threadIndex);

	//--------------------------------------------------------------------------
	// Update OpenCL buffers
	//--------------------------------------------------------------------------

	if (updateActions.Has(FILM_EDIT)) {
		// Resize the Frame Buffer
		InitFrameBuffer();
		
		// Display thread initialization
		if (amiDisplayThread)
			InitDisplayThread();
	}

	if (updateActions.Has(CAMERA_EDIT)) {
		// Update Camera
		InitCamera();
	}

	if (updateActions.Has(GEOMETRY_EDIT)) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (updateActions.Has(MATERIALS_EDIT) || updateActions.Has(MATERIAL_TYPES_EDIT)) {
		// Update Scene Materials
		InitMaterials();
	}

	if  (updateActions.Has(AREALIGHTS_EDIT)) {
		// Update Scene Area Lights
		InitTriangleAreaLights();
	}

	if  (updateActions.Has(INFINITELIGHT_EDIT)) {
		// Update Scene Infinite Light
		InitInfiniteLight();
	}

	if  (updateActions.Has(SUNLIGHT_EDIT)) {
		// Update Scene Sun Light
		InitSunLight();
	}

	if  (updateActions.Has(SKYLIGHT_EDIT)) {
		// Update Scene Sun Light
		InitSkyLight();
	}

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	if (updateActions.Has(FILM_EDIT) || updateActions.Has(MATERIAL_TYPES_EDIT))
		InitKernels();

	updateActions.Reset();
	editMutex.unlock();

	SetKernelArgs();

	//--------------------------------------------------------------------------
	// Execute initialization kernels
	//--------------------------------------------------------------------------

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear the frame buffer
	oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
		cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, initFBWorkGroupSize)),
		cl::NDRange(initFBWorkGroupSize));

	// Initialize the tasks buffer
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
		cl::NDRange(renderEngine->taskCount), cl::NDRange(initWorkGroupSize));

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void RTPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;
	boost::barrier *frameBarrier = engine->frameBarrier;
	// To start all threads at the same time
	frameBarrier->wait();

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	try {
		const bool amiDisplayThread = (engine->displayDeviceIndex == threadIndex);
		if (amiDisplayThread) {
			clearSBKernel->setArg(0, *screenBufferBuff);
			oclQueue.enqueueNDRangeKernel(*clearSBKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(renderEngine->film->GetWidth() * renderEngine->film->GetHeight(), clearSBWorkGroupSize)),
					cl::NDRange(clearSBWorkGroupSize));
		}

		while (!boost::this_thread::interruption_requested()) {
			if (updateActions.HasAnyAction())
				UpdateOCLBuffers();

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
				oclQueue.enqueueNDRangeKernel(*advancePathsKernel, cl::NullRange,
					cl::NDRange(engine->taskCount), cl::NDRange(advancePathsWorkGroupSize));
			}

			// No need to transfer the frame buffer if I'm the display thread
			if (engine->displayDeviceIndex != threadIndex) {
				// Async. transfer of the frame buffer
				oclQueue.enqueueReadBuffer(*frameBufferBuff, CL_FALSE, 0,
					frameBufferBuff->getInfo<CL_MEM_SIZE>(), frameBuffer);
			}

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(*(taskStatsBuff), CL_FALSE, 0,
				sizeof(slg::ocl::GPUTaskStats) * renderEngine->taskCount, gpuTaskStats);

			oclQueue.finish();
			frameTime = WallClockTime() - startTime;

			//------------------------------------------------------------------

			frameBarrier->wait();

			// If I'm the display thread, my OpenCL device must merge all frame buffers
			// and do all frame post-processing steps
			if (amiDisplayThread) {
				// Last step include a film update
				boost::unique_lock<boost::mutex> lock(*(engine->filmMutex));

				//--------------------------------------------------------------
				// Clear the merged frame buffer
				//--------------------------------------------------------------

				oclQueue.enqueueNDRangeKernel(*clearFBKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, clearFBWorkGroupSize)),
						cl::NDRange(clearFBWorkGroupSize));

				//--------------------------------------------------------------
				// Merge all frame buffers
				//--------------------------------------------------------------

				for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
					// I don't need to lock renderEngine->setKernelArgsMutex
					// because I'm the only thread running

					if (i == threadIndex) {
						// Normalize and merge the my frame buffer
						u_int argIndex = 0;
						mergeFBKernel->setArg(argIndex++, *frameBufferBuff);
						mergeFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);
						oclQueue.enqueueNDRangeKernel(*mergeFBKernel, cl::NullRange,
								cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, mergeFBWorkGroupSize)),
								cl::NDRange(mergeFBWorkGroupSize));
					} else {
						// Transfer the frame buffer to the device
						oclQueue.enqueueWriteBuffer(*tmpFrameBufferBuff, CL_FALSE, 0,
								tmpFrameBufferBuff->getInfo<CL_MEM_SIZE>(),
								engine->renderThreads[i]->frameBuffer);

						// Normalize and merge the frame buffers
						u_int argIndex = 0;
						mergeFBKernel->setArg(argIndex++, *tmpFrameBufferBuff);
						mergeFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);
						oclQueue.enqueueNDRangeKernel(*mergeFBKernel, cl::NullRange,
								cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, mergeFBWorkGroupSize)),
								cl::NDRange(mergeFBWorkGroupSize));
					}
				}

				//--------------------------------------------------------------
				// Normalize the merged buffer
				//--------------------------------------------------------------

				oclQueue.enqueueNDRangeKernel(*normalizeFBKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, normalizeFBWorkGroupSize)),
						cl::NDRange(normalizeFBWorkGroupSize));

				//--------------------------------------------------------------
				// Apply Gaussian filter to the merged buffer
				//--------------------------------------------------------------

				for (u_int i = 0; i < 3; ++i) {
					oclQueue.enqueueNDRangeKernel(*applyBlurLightFilterXR1Kernel, cl::NullRange,
							cl::NDRange(RoundUp<unsigned int>(frameBufferPixelCount, applyBlurLightFilterXR1WorkGroupSize)),
							cl::NDRange(applyBlurLightFilterXR1WorkGroupSize));

					oclQueue.enqueueNDRangeKernel(*applyBlurLightFilterYR1Kernel, cl::NullRange,
							cl::NDRange(RoundUp<unsigned int>(frameBufferPixelCount, applyBlurLightFilterYR1WorkGroupSize)),
							cl::NDRange(applyBlurLightFilterYR1WorkGroupSize));
				}

				//--------------------------------------------------------------
				// Apply tone mapping to merged buffer
				//--------------------------------------------------------------

				oclQueue.enqueueNDRangeKernel(*toneMapLinearKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, toneMapLinearWorkGroupSize)),
						cl::NDRange(toneMapLinearWorkGroupSize));

				//--------------------------------------------------------------
				// Update the screen buffer
				//--------------------------------------------------------------

				oclQueue.enqueueNDRangeKernel(*updateScreenBufferKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, updateScreenBufferWorkGroupSize)),
						cl::NDRange(updateScreenBufferWorkGroupSize));
				
				//--------------------------------------------------------------
				// Transfer the screen frame buffer
				//--------------------------------------------------------------

				// The film has been locked before
				oclQueue.enqueueReadBuffer(*screenBufferBuff, CL_FALSE, 0,
						screenBufferBuff->getInfo<CL_MEM_SIZE>(), engine->film->GetScreenBuffer());
				oclQueue.finish();
			}

			//------------------------------------------------------------------

			frameBarrier->wait();

			// Main thread re-balance assigned iterations to each task and
			// update the film

			//------------------------------------------------------------------

			frameBarrier->wait();

			// Time to render a new frame
		}
		//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}
	oclQueue.finish();
}

}

#endif
