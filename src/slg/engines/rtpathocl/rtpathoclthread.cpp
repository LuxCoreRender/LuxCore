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
	clearFBKernel = NULL;
	clearSBKernel = NULL;
	mergeFBKernel = NULL;
	normalizeFBKernel = NULL;
	applyBlurFilterXR1Kernel = NULL;
	applyBlurFilterYR1Kernel = NULL;
	toneMapLinearKernel = NULL;
	sumRGBValuesReduceKernel = NULL;
	sumRGBValueAccumulateKernel = NULL;
	toneMapAutoLinearKernel = NULL;
	updateScreenBufferKernel = NULL;

	tmpFrameBufferBuff = NULL;
	mergedFrameBufferBuff = NULL;
	screenBufferBuff = NULL;
	assignedIters = ((RTPathOCLRenderEngine*)renderEngine)->minIterations;
	frameTime = 0.0;
}

RTPathOCLRenderThread::~RTPathOCLRenderThread() {
	delete clearFBKernel;
	delete clearSBKernel;
	delete mergeFBKernel;
	delete normalizeFBKernel;
	delete applyBlurFilterXR1Kernel;
	delete applyBlurFilterYR1Kernel;
	delete toneMapLinearKernel;
	delete sumRGBValuesReduceKernel;
	delete sumRGBValueAccumulateKernel;
	delete toneMapAutoLinearKernel;
	delete updateScreenBufferKernel;
}

void RTPathOCLRenderThread::AdditionalInit() {
	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;
	if (engine->displayDeviceIndex == threadIndex)
		InitDisplayThread();

	PathOCLRenderThread::AdditionalInit();
}

string RTPathOCLRenderThread::AdditionalKernelOptions() {
	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			PathOCLRenderThread::AdditionalKernelOptions();

	float toneMapScale = 1.f;
	float gamma = 2.2f;

	const ImagePipeline *ip = engine->film->GetImagePipeline();
	if (ip) {
		const ToneMap *tm = (const ToneMap *)ip->GetPlugin(typeid(LinearToneMap));
		if (tm) {
			const LinearToneMap *ltm = (const LinearToneMap *)tm;
			toneMapScale = ltm->scale;
		}

		const GammaCorrectionPlugin *gc = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
		if (gc)
			gamma = gc->gamma;
	}

	ss <<
			" -D PARAM_TONEMAP_LINEAR_SCALE=" << toneMapScale <<
			" -D PARAM_GAMMA=" << gamma << "f" <<
			" -D PARAM_GHOSTEFFECT_INTENSITY=" << engine->ghostEffect << "f";

	return ss.str();
}

string RTPathOCLRenderThread::AdditionalKernelSources() {
	stringstream ss;
	ss << PathOCLRenderThread::AdditionalKernelSources() <<
			slg::ocl::KernelSource_rtpathoclbase_funcs;

	return ss.str();
}

void RTPathOCLRenderThread::CompileAdditionalKernels(cl::Program *program) {
	PathOCLRenderThread::CompileAdditionalKernels(program);

	//--------------------------------------------------------------------------
	// ClearFrameBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &clearFBKernel, &clearFBWorkGroupSize, "ClearFrameBuffer");

	//--------------------------------------------------------------------------
	// ClearScreenBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &clearSBKernel, &clearSBWorkGroupSize, "ClearScreenBuffer");

	//--------------------------------------------------------------------------
	// MergeFrameBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &mergeFBKernel, &mergeFBWorkGroupSize, "MergeFrameBuffer");

	//--------------------------------------------------------------------------
	// NormalizeFrameBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &normalizeFBKernel, &normalizeFBWorkGroupSize, "NormalizeFrameBuffer");

	//--------------------------------------------------------------------------
	// Gaussian blur frame buffer filter kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &applyBlurFilterXR1Kernel, &applyBlurFilterXR1WorkGroupSize, "ApplyGaussianBlurFilterXR1");
	CompileKernel(program, &applyBlurFilterYR1Kernel, &applyBlurFilterYR1WorkGroupSize, "ApplyGaussianBlurFilterYR1");

	//--------------------------------------------------------------------------
	// ToneMapLinear kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &toneMapLinearKernel, &toneMapLinearWorkGroupSize, "ToneMapLinear");

	//--------------------------------------------------------------------------
	// ToneMapAutoLinear kernel
	//--------------------------------------------------------------------------

	size_t workgroupSize;
	CompileKernel(program, &sumRGBValuesReduceKernel, &workgroupSize, "SumRGBValuesReduce");
	if (workgroupSize != 64)
		throw runtime_error("RTPathOCL requires a workgroup size of 64");
	CompileKernel(program, &sumRGBValueAccumulateKernel, &workgroupSize, "SumRGBValueAccumulate");
	CompileKernel(program, &toneMapAutoLinearKernel, &workgroupSize, "ToneMapAutoLinear");

	//--------------------------------------------------------------------------
	// UpdateScreenBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &updateScreenBufferKernel, &updateScreenBufferWorkGroupSize, "UpdateScreenBuffer");
}

void RTPathOCLRenderThread::Interrupt() {
}

void RTPathOCLRenderThread::Stop() {
	PathOCLRenderThread::Stop();

	FreeOCLBuffer(&tmpFrameBufferBuff);
	FreeOCLBuffer(&mergedFrameBufferBuff);
	FreeOCLBuffer(&screenBufferBuff);
}

void RTPathOCLRenderThread::BeginSceneEdit() {
}

void RTPathOCLRenderThread::EndSceneEdit(const EditActionList &editActions) {
}

void RTPathOCLRenderThread::InitDisplayThread() {
	const u_int filmBufferPixelCount = threadFilms[0]->film->GetWidth() * threadFilms[0]->film->GetHeight();
	AllocOCLBufferRW(&tmpFrameBufferBuff, sizeof(slg::ocl::Pixel) * filmBufferPixelCount, "Tmp FrameBuffer");
	AllocOCLBufferRW(&mergedFrameBufferBuff, sizeof(slg::ocl::Pixel) * filmBufferPixelCount, "Merged FrameBuffer");

	AllocOCLBufferRW(&screenBufferBuff, sizeof(Spectrum) * filmBufferPixelCount, "Screen FrameBuffer");
}

void RTPathOCLRenderThread::SetAdditionalKernelArgs() {
	PathOCLRenderThread::SetAdditionalKernelArgs();

	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;
	if (engine->displayDeviceIndex == threadIndex) {
		boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);

		u_int argIndex = 0;
		clearFBKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		clearFBKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		clearFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		clearSBKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		clearSBKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		clearSBKernel->setArg(argIndex, *screenBufferBuff);

		argIndex = 0;
		normalizeFBKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		normalizeFBKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		normalizeFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		applyBlurFilterXR1Kernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		applyBlurFilterXR1Kernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		applyBlurFilterXR1Kernel->setArg(argIndex++, *screenBufferBuff);
		applyBlurFilterXR1Kernel->setArg(argIndex++, *tmpFrameBufferBuff);
		argIndex = 0;
		applyBlurFilterYR1Kernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		applyBlurFilterYR1Kernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		applyBlurFilterYR1Kernel->setArg(argIndex++, *tmpFrameBufferBuff);
		applyBlurFilterYR1Kernel->setArg(argIndex++, *screenBufferBuff);

		argIndex = 0;
		toneMapLinearKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		toneMapLinearKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		toneMapLinearKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		sumRGBValuesReduceKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		sumRGBValuesReduceKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		sumRGBValuesReduceKernel->setArg(argIndex++, *mergedFrameBufferBuff);
		sumRGBValuesReduceKernel->setArg(argIndex++, *tmpFrameBufferBuff);

		argIndex = 0;
		sumRGBValueAccumulateKernel->setArg(argIndex++, 0);
		sumRGBValueAccumulateKernel->setArg(argIndex++, *tmpFrameBufferBuff);

		argIndex = 0;
		toneMapAutoLinearKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		toneMapAutoLinearKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		toneMapAutoLinearKernel->setArg(argIndex++, *mergedFrameBufferBuff);
		float gamma = 2.2f;
		const ImagePipeline *ip = engine->film->GetImagePipeline();
		if (ip) {
			const GammaCorrectionPlugin *gc = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
			if (gc)
				gamma = gc->gamma;
		}
		toneMapAutoLinearKernel->setArg(argIndex++, gamma);
		toneMapAutoLinearKernel->setArg(argIndex++, *tmpFrameBufferBuff);

		argIndex = 0;
		updateScreenBufferKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
		updateScreenBufferKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
		updateScreenBufferKernel->setArg(argIndex++, *mergedFrameBufferBuff);
		updateScreenBufferKernel->setArg(argIndex++, *screenBufferBuff);
	}
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

	if (updateActions.Has(MATERIALS_EDIT) || updateActions.Has(MATERIAL_TYPES_EDIT)) {
		// Update Scene Materials
		InitMaterials();
	}

	if (updateActions.Has(LIGHTS_EDIT) || updateActions.Has(LIGHT_TYPES_EDIT)) {
		// Update Scene Lights
		InitLights();
	}

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	if (updateActions.Has(MATERIAL_TYPES_EDIT) || updateActions.Has(LIGHT_TYPES_EDIT))
		InitKernels();

	SetKernelArgs();

	//--------------------------------------------------------------------------
	// Execute initialization kernels
	//--------------------------------------------------------------------------

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear the frame buffer
	const u_int filmPixelCount = threadFilms[0]->film->GetWidth() * threadFilms[0]->film->GetHeight();
	oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));

	// Initialize the tasks buffer
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(engine->taskCount, initWorkGroupSize)),
			cl::NDRange(initWorkGroupSize));

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
	lastEditTime = WallClockTime();
}

void RTPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	// Boost barriers are supposed to be non interruptible but they are
	// and seem to be missing a way to reset them. So better to disable
	// interruptions.
	boost::this_thread::disable_interruption di;

	RTPathOCLRenderEngine *engine = (RTPathOCLRenderEngine *)renderEngine;
	boost::barrier *frameBarrier = engine->frameBarrier;

	const u_int filmBufferPixelCount = engine->film->GetWidth() * engine->film->GetHeight();

	// Check if to use autolinear or linear tonemapping
	bool useAutoLinearToneMap = true;
	const ImagePipeline *ip = engine->film->GetImagePipeline();
	if (ip) {
		const ToneMap *tm = (const ToneMap *)ip->GetPlugin(typeid(LinearToneMap));
		if (tm)
			useAutoLinearToneMap = false;
	}

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

		const bool amiDisplayThread = (engine->displayDeviceIndex == threadIndex);
		if (amiDisplayThread) {
			initQueue.enqueueNDRangeKernel(*clearSBKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, clearSBWorkGroupSize)),
					cl::NDRange(clearSBWorkGroupSize));
		}

		double lastEditTime = WallClockTime();
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

			// No need to transfer the frame buffer if I'm not the display thread
			if (engine->displayDeviceIndex != threadIndex)
				threadFilms[0]->TransferFilm(currentQueue);

			// Async. transfer of GPU task statistics
			currentQueue.enqueueReadBuffer(*(taskStatsBuff), CL_FALSE, 0,
				sizeof(slg::ocl::pathocl::GPUTaskStats) * engine->taskCount, gpuTaskStats);

			currentQueue.finish();
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

				currentQueue.enqueueNDRangeKernel(*clearFBKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, clearFBWorkGroupSize)),
						cl::NDRange(clearFBWorkGroupSize));

				//--------------------------------------------------------------
				// Merge all frame buffers
				//--------------------------------------------------------------

				for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
					// I don't need to lock renderEngine->setKernelArgsMutex
					// because I'm the only thread running

					if (i == threadIndex) {
						// Normalize and merge the frame buffers
						u_int argIndex = 0;
						mergeFBKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
						mergeFBKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
						mergeFBKernel->setArg(argIndex++, *(threadFilms[0]->channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[0]));
						mergeFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);
						currentQueue.enqueueNDRangeKernel(*mergeFBKernel, cl::NullRange,
								cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, mergeFBWorkGroupSize)),
								cl::NDRange(mergeFBWorkGroupSize));
					} else {
						// Transfer the frame buffer to the device
						RTPathOCLRenderThread *thread = (RTPathOCLRenderThread *)(engine->renderThreads[i]);
						currentQueue.enqueueWriteBuffer(*tmpFrameBufferBuff, CL_FALSE, 0,
								tmpFrameBufferBuff->getInfo<CL_MEM_SIZE>(),
								thread->threadFilms[0]->film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->GetPixels());

						// Normalize and merge the frame buffers
						u_int argIndex = 0;
						mergeFBKernel->setArg(argIndex++, thread->threadFilms[0]->film->GetWidth());
						mergeFBKernel->setArg(argIndex++, thread->threadFilms[0]->film->GetHeight());
						mergeFBKernel->setArg(argIndex++, *tmpFrameBufferBuff);
						mergeFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);
						currentQueue.enqueueNDRangeKernel(*mergeFBKernel, cl::NullRange,
								cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, mergeFBWorkGroupSize)),
								cl::NDRange(mergeFBWorkGroupSize));
					}
				}

				//--------------------------------------------------------------
				// Normalize the merged buffer
				//--------------------------------------------------------------

				currentQueue.enqueueNDRangeKernel(*normalizeFBKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, normalizeFBWorkGroupSize)),
						cl::NDRange(normalizeFBWorkGroupSize));

				//--------------------------------------------------------------
				// Apply tone mapping to merged buffer
				//--------------------------------------------------------------

				if (useAutoLinearToneMap) {
					// Reduce the pixel sum
					u_int workSize = RoundUpPow2<u_int>(filmBufferPixelCount) / 2;
					currentQueue.enqueueNDRangeKernel(*sumRGBValuesReduceKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(workSize, 64)),
						cl::NDRange(64));
					workSize /= 64;
					
					// Accumulate the pixel sum in a single value
					{
						boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);
						sumRGBValueAccumulateKernel->setArg(0, workSize);	
					}
					
					currentQueue.enqueueNDRangeKernel(*sumRGBValueAccumulateKernel, cl::NullRange,
						cl::NDRange(64),
						cl::NDRange(64));

					// Apply the scale
					currentQueue.enqueueNDRangeKernel(*toneMapAutoLinearKernel, cl::NullRange,
							cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, 64)),
							cl::NDRange(64));
				} else {
					currentQueue.enqueueNDRangeKernel(*toneMapLinearKernel, cl::NullRange,
							cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, toneMapLinearWorkGroupSize)),
							cl::NDRange(toneMapLinearWorkGroupSize));
				}

				//--------------------------------------------------------------
				// Update the screen buffer
				//--------------------------------------------------------------

				currentQueue.enqueueNDRangeKernel(*updateScreenBufferKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(filmBufferPixelCount, updateScreenBufferWorkGroupSize)),
						cl::NDRange(updateScreenBufferWorkGroupSize));

				//--------------------------------------------------------------
				// Apply Gaussian filter to the screen buffer
				//--------------------------------------------------------------

				// Base the amount of blur on the time since the last update (using a 3secs window)
				const double timeSinceLastUpdate = WallClockTime() - lastEditTime;
				const float weight = Lerp(Clamp<float>(timeSinceLastUpdate, 0.f, engine->blurTimeWindow) / 5.f,
						engine->blurMaxCap, engine->blurMinCap);

				if (weight > 0.f) {
					applyBlurFilterXR1Kernel->setArg(4, weight);
					applyBlurFilterYR1Kernel->setArg(4, weight);
					for (u_int i = 0; i < 3; ++i) {
						currentQueue.enqueueNDRangeKernel(*applyBlurFilterXR1Kernel, cl::NullRange,
								cl::NDRange(RoundUp<unsigned int>(filmBufferPixelCount, applyBlurFilterXR1WorkGroupSize)),
								cl::NDRange(applyBlurFilterXR1WorkGroupSize));

						currentQueue.enqueueNDRangeKernel(*applyBlurFilterYR1Kernel, cl::NullRange,
								cl::NDRange(RoundUp<unsigned int>(filmBufferPixelCount, applyBlurFilterYR1WorkGroupSize)),
								cl::NDRange(applyBlurFilterYR1WorkGroupSize));
					}
				}

				//--------------------------------------------------------------
				// Transfer the screen frame buffer
				//--------------------------------------------------------------

				// The film has been locked before
				currentQueue.enqueueReadBuffer(*screenBufferBuff, CL_FALSE, 0,
						screenBufferBuff->getInfo<CL_MEM_SIZE>(), engine->film->channel_RGB_TONEMAPPED->GetPixels());
				currentQueue.finish();
			}

			//------------------------------------------------------------------
			// Update OpenCL buffers if there is any edit action
			//------------------------------------------------------------------

			if (amiDisplayThread) {
				engine->editCanStart.notify_one();
				engine->editMutex.lock();

				if (engine->updateActions.HasAnyAction()) {
					// Update all threads
					for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
						RTPathOCLRenderThread *thread = (RTPathOCLRenderThread *)(engine->renderThreads[i]);
						thread->UpdateOCLBuffers(engine->updateActions);
					}

					// Reset updateActions
					engine->updateActions.Reset();

					// Clear the film
					engine->film->Reset();
				}

				engine->editMutex.unlock();
			}

			//------------------------------------------------------------------

			frameBarrier->wait();

			// Time to render a new frame
		}
		//SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		SLG_LOG("[RTPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}

	intersectionDevice->GetOpenCLQueue().finish();
}

#endif
