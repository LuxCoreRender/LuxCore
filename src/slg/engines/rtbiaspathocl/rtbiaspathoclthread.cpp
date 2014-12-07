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
#include "slg/film/tonemap.h"
#include "slg/film/imagepipelineplugins.h"
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
	clearFBKernel = NULL;
	clearSBKernel = NULL;
	normalizeFBKernel = NULL;
	applyBlurFilterXR1Kernel = NULL;
	applyBlurFilterYR1Kernel = NULL;
	toneMapLinearKernel = NULL;
	updateScreenBufferKernel = NULL;

	tmpFrameBufferBuff = NULL;
	mergedFrameBufferBuff = NULL;
	screenBufferBuff = NULL;
}

RTBiasPathOCLRenderThread::~RTBiasPathOCLRenderThread() {
	delete clearFBKernel;
	delete clearSBKernel;
	delete normalizeFBKernel;
	delete applyBlurFilterXR1Kernel;
	delete applyBlurFilterYR1Kernel;
	delete toneMapLinearKernel;
	delete updateScreenBufferKernel;
}

void RTBiasPathOCLRenderThread::AdditionalInit() {
	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;
	if (engine->displayDeviceIndex == threadIndex)
		InitDisplayThread();

	BiasPathOCLRenderThread::AdditionalInit();
}

string RTBiasPathOCLRenderThread::AdditionalKernelOptions() {
	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			BiasPathOCLRenderThread::AdditionalKernelOptions();

	float toneMapScale = 1.f;
	float gamma = 2.2f;

	const ImagePipeline *ip = engine->film->GetImagePipeline();
	if (ip) {
		const ToneMap *tm = (const ToneMap *)ip->GetPlugin(typeid(LinearToneMap));
		if (tm && (tm->GetType() == TONEMAP_LINEAR)) {
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

string RTBiasPathOCLRenderThread::AdditionalKernelSources() {
	stringstream ss;
	ss << BiasPathOCLRenderThread::AdditionalKernelSources() <<
			slg::ocl::KernelSource_rtpathoclbase_funcs;

	return ss.str();
}

void RTBiasPathOCLRenderThread::CompileAdditionalKernels(cl::Program *program) {
	BiasPathOCLRenderThread::CompileAdditionalKernels(program);

	//--------------------------------------------------------------------------
	// ClearFrameBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &clearFBKernel, &clearFBWorkGroupSize, "ClearFrameBuffer");

	//--------------------------------------------------------------------------
	// ClearScreenBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &clearSBKernel, &clearSBWorkGroupSize, "ClearScreenBuffer");

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
	// UpdateScreenBuffer kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &updateScreenBufferKernel, &updateScreenBufferWorkGroupSize, "UpdateScreenBuffer");
}

void RTBiasPathOCLRenderThread::Interrupt() {
}

void RTBiasPathOCLRenderThread::Stop() {
	BiasPathOCLRenderThread::Stop();

	FreeOCLBuffer(&tmpFrameBufferBuff);
	FreeOCLBuffer(&mergedFrameBufferBuff);
	FreeOCLBuffer(&screenBufferBuff);
}

void RTBiasPathOCLRenderThread::BeginSceneEdit() {
}

void RTBiasPathOCLRenderThread::EndSceneEdit(const EditActionList &editActions) {
}

void RTBiasPathOCLRenderThread::InitDisplayThread() {
	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;
	const u_int engineFilmPixelCount = engine->film->GetWidth() * engine->film->GetHeight();
	AllocOCLBufferRW(&tmpFrameBufferBuff, sizeof(slg::ocl::Pixel) * engineFilmPixelCount, "Tmp FrameBuffer");
	AllocOCLBufferRW(&mergedFrameBufferBuff, sizeof(slg::ocl::Pixel) * engineFilmPixelCount, "Merged FrameBuffer");

	AllocOCLBufferRW(&screenBufferBuff, sizeof(Spectrum) * engineFilmPixelCount, "Screen FrameBuffer");
}

void RTBiasPathOCLRenderThread::SetAdditionalKernelArgs() {
	BiasPathOCLRenderThread::SetAdditionalKernelArgs();

	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;
	if (engine->displayDeviceIndex == threadIndex) {
		boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);

		u_int argIndex = 0;
		clearFBKernel->setArg(argIndex++, engine->film->GetWidth());
		clearFBKernel->setArg(argIndex++, engine->film->GetHeight());
		clearFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		clearSBKernel->setArg(argIndex++, engine->film->GetWidth());
		clearSBKernel->setArg(argIndex++, engine->film->GetHeight());
		clearSBKernel->setArg(argIndex, *screenBufferBuff);

		argIndex = 0;
		normalizeFBKernel->setArg(argIndex++, engine->film->GetWidth());
		normalizeFBKernel->setArg(argIndex++, engine->film->GetHeight());
		normalizeFBKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		applyBlurFilterXR1Kernel->setArg(argIndex++, engine->film->GetWidth());
		applyBlurFilterXR1Kernel->setArg(argIndex++, engine->film->GetHeight());
		applyBlurFilterXR1Kernel->setArg(argIndex++, *screenBufferBuff);
		applyBlurFilterXR1Kernel->setArg(argIndex++, *tmpFrameBufferBuff);
		argIndex = 0;
		applyBlurFilterYR1Kernel->setArg(argIndex++, engine->film->GetWidth());
		applyBlurFilterYR1Kernel->setArg(argIndex++, engine->film->GetHeight());
		applyBlurFilterYR1Kernel->setArg(argIndex++, *tmpFrameBufferBuff);
		applyBlurFilterYR1Kernel->setArg(argIndex++, *screenBufferBuff);

		argIndex = 0;
		toneMapLinearKernel->setArg(argIndex++, engine->film->GetWidth());
		toneMapLinearKernel->setArg(argIndex++, engine->film->GetHeight());
		toneMapLinearKernel->setArg(argIndex++, *mergedFrameBufferBuff);

		argIndex = 0;
		updateScreenBufferKernel->setArg(argIndex++, engine->film->GetWidth());
		updateScreenBufferKernel->setArg(argIndex++, engine->film->GetHeight());
		updateScreenBufferKernel->setArg(argIndex++, *mergedFrameBufferBuff);
		updateScreenBufferKernel->setArg(argIndex++, *screenBufferBuff);
	}
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
		// Update Scene Materials
		InitMaterials();
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
	lastEditTime = WallClockTime();
}

void RTBiasPathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	// Boost barriers are supposed to be not interruptible but they are
	// and seem to be missing a way to reset them. So better to disable
	// interruptions.
	boost::this_thread::disable_interruption di;

	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;
	boost::barrier *frameBarrier = engine->frameBarrier;
	const u_int tileWidth = engine->tileRepository->tileWidth;
	const u_int tileHeight = engine->tileRepository->tileHeight;
	const u_int threadFilmPixelCount = tileWidth * tileHeight;
	const u_int taskCount = engine->taskCount;
	const u_int engineFilmPixelCount = engine->film->GetWidth() * engine->film->GetHeight();

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

		const bool amiDisplayThread = (engine->displayDeviceIndex == threadIndex);
		if (amiDisplayThread) {
			initQueue.enqueueNDRangeKernel(*clearSBKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(engineFilmPixelCount, clearSBWorkGroupSize)),
					cl::NDRange(clearSBWorkGroupSize));
		}

		while (!boost::this_thread::interruption_requested()) {
			cl::CommandQueue &currentQueue = intersectionDevice->GetOpenCLQueue();

			//------------------------------------------------------------------
			// Render the tiles
			//------------------------------------------------------------------

			TileRepository::Tile *tile = NULL;
			while (engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tile, threadFilms[0]->film)) {
				//const double t0 = WallClockTime();
				threadFilms[0]->film->Reset();
				//const u_int tileW = Min(engine->tileRepository->tileWidth, engine->film->GetWidth() - tile->xStart);
				//const u_int tileH = Min(engine->tileRepository->tileHeight, engine->film->GetHeight() - tile->yStart);
				//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Tile: "
				//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
				//		"(" << tileW << ", " << tileH << ")");

				// Clear the frame buffer
				currentQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(threadFilmPixelCount, filmClearWorkGroupSize)),
					cl::NDRange(filmClearWorkGroupSize));

				// Initialize the statistics
				currentQueue.enqueueNDRangeKernel(*initStatKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(taskCount, initStatWorkGroupSize)),
					cl::NDRange(initStatWorkGroupSize));

				// Render the tile
				UpdateKernelArgsForTile(tile->xStart, tile->yStart, 0);

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
				const u_int step = engine->tileRepository->totalSamplesPerPixel;
				for (u_int i = 0; i < taskCount; i += step)
					tracedRaysCount += gpuTaskStats[i].raysCount;

				intersectionDevice->IntersectionKernelExecuted(tracedRaysCount);
				
				//const double t1 = WallClockTime();
				//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Tile rendering time: " + ToString((u_int)((t1 - t0) * 1000.0)) + "ms");
			}

			//------------------------------------------------------------------

			frameBarrier->wait();

			// If I'm the display thread, my OpenCL device must merge all frame buffers
			// and do all frame post-processing steps
			if (amiDisplayThread) {
				boost::unique_lock<boost::mutex> lock(*(engine->filmMutex));

				//--------------------------------------------------------------
				// Transfer the frame buffer to the display device
				//--------------------------------------------------------------

				currentQueue.enqueueWriteBuffer(*mergedFrameBufferBuff, CL_FALSE, 0,
						mergedFrameBufferBuff->getInfo<CL_MEM_SIZE>(),
						engine->film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->GetPixels());

				//--------------------------------------------------------------
				// Normalize the merged buffer
				//--------------------------------------------------------------

				currentQueue.enqueueNDRangeKernel(*normalizeFBKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(engineFilmPixelCount, normalizeFBWorkGroupSize)),
						cl::NDRange(normalizeFBWorkGroupSize));

				//--------------------------------------------------------------
				// Apply tone mapping to merged buffer
				//--------------------------------------------------------------

				currentQueue.enqueueNDRangeKernel(*toneMapLinearKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(engineFilmPixelCount, toneMapLinearWorkGroupSize)),
						cl::NDRange(toneMapLinearWorkGroupSize));

				//--------------------------------------------------------------
				// Update the screen buffer
				//--------------------------------------------------------------

				currentQueue.enqueueNDRangeKernel(*updateScreenBufferKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(engineFilmPixelCount, updateScreenBufferWorkGroupSize)),
						cl::NDRange(updateScreenBufferWorkGroupSize));

				//--------------------------------------------------------------
				// Apply Gaussian filter to the screen buffer
				//--------------------------------------------------------------

				// Base the amount of blur on the time since the last update
				const double timeSinceLastUpdate = WallClockTime() - lastEditTime;
				const float weight = Lerp(Clamp<float>(timeSinceLastUpdate, 0.f, engine->blurTimeWindow) / 5.f,
						engine->blurMaxCap, engine->blurMinCap);

				if (weight > 0.f) {
					applyBlurFilterXR1Kernel->setArg(4, weight);
					applyBlurFilterYR1Kernel->setArg(4, weight);
					for (u_int i = 0; i < 3; ++i) {
						currentQueue.enqueueNDRangeKernel(*applyBlurFilterXR1Kernel, cl::NullRange,
								cl::NDRange(RoundUp<unsigned int>(engineFilmPixelCount, applyBlurFilterXR1WorkGroupSize)),
								cl::NDRange(applyBlurFilterXR1WorkGroupSize));

						currentQueue.enqueueNDRangeKernel(*applyBlurFilterYR1Kernel, cl::NullRange,
								cl::NDRange(RoundUp<unsigned int>(engineFilmPixelCount, applyBlurFilterYR1WorkGroupSize)),
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
						RTBiasPathOCLRenderThread *thread = (RTBiasPathOCLRenderThread *)(engine->renderThreads[i]);
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
		//SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		SLG_LOG("[RTBiasPathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}

	intersectionDevice->GetOpenCLQueue().finish();
}

#endif
