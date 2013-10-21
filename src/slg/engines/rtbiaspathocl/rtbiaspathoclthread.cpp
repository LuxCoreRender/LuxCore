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

	if (engine->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
		const LinearToneMapParams *ltm = (const LinearToneMapParams *)engine->film->GetToneMapParams();
		ss << " -D PARAM_TONEMAP_LINEAR_SCALE=" << ltm->scale << "f";
	} else
		ss << " -D PARAM_TONEMAP_LINEAR_SCALE=1.f";

	ss << " -D PARAM_GAMMA=" << engine->film->GetGamma() << "f" <<
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

void RTBiasPathOCLRenderThread::BeginEdit() {
}

void RTBiasPathOCLRenderThread::EndEdit(const EditActionList &editActions) {
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
	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;

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

	if (updateActions.Has(AREALIGHTS_EDIT)) {
		// Update Scene Area Lights
		InitTriangleAreaLights();
	}

	if (updateActions.Has(INFINITELIGHT_EDIT)) {
		// Update Scene Infinite Light
		InitInfiniteLight();
	}

	if (updateActions.Has(SUNLIGHT_EDIT)) {
		// Update Scene Sun Light
		InitSunLight();
	}

	if (updateActions.Has(SKYLIGHT_EDIT)) {
		// Update Scene Sun Light
		InitSkyLight();
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

	// Boost barriers are supposed to be not interruptable but they are
	// and seems to missing a way to reset them. So better to disable
	// interruptions.
	boost::this_thread::disable_interruption di;

	RTBiasPathOCLRenderEngine *engine = (RTBiasPathOCLRenderEngine *)renderEngine;
	boost::barrier *frameBarrier = engine->frameBarrier;
	const u_int tileSize = engine->tileRepository->tileSize;
	const u_int threadFilmPixelCount = tileSize * tileSize;
	const u_int taskCount = engine->taskCount;
	const u_int engineFilmPixelCount = engine->film->GetWidth() * engine->film->GetHeight();

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

		// Initialize OpenCL structures
		oclQueue.enqueueNDRangeKernel(*initSeedKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(taskCount, initSeedWorkGroupSize)),
				cl::NDRange(initSeedWorkGroupSize));

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		const bool amiDisplayThread = (engine->displayDeviceIndex == threadIndex);
		if (amiDisplayThread) {
			oclQueue.enqueueNDRangeKernel(*clearSBKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(engineFilmPixelCount, clearSBWorkGroupSize)),
					cl::NDRange(clearSBWorkGroupSize));
		}

		while (!boost::this_thread::interruption_requested()) {
			//------------------------------------------------------------------
			// Render the tiles
			//------------------------------------------------------------------

			TileRepository::Tile *tile = NULL;
			while (engine->NextTile(&tile, threadFilm)) {
				threadFilm->Reset();
				//const u_int tileWidth = Min(engine->tileRepository->tileSize, engine->film->GetWidth() - tile->xStart);
				//const u_int tileHeight = Min(engine->tileRepository->tileSize, engine->film->GetHeight() - tile->yStart);
				//SLG_LOG("[BiasPathOCLRenderThread::" << threadIndex << "] Tile: "
				//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
				//		"(" << tileWidth << ", " << tileHeight << ") [" << tile->sampleIndex << "]");

				// Clear the frame buffer
				oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
					cl::NDRange(RoundUp<u_int>(threadFilmPixelCount, filmClearWorkGroupSize)),
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

					mergePixelSamplesKernel->setArg(0, tile->xStart);
					mergePixelSamplesKernel->setArg(1, tile->yStart);
				}

				// Render all pixel samples
				oclQueue.enqueueNDRangeKernel(*renderSampleKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(taskCount, renderSampleWorkGroupSize)),
						cl::NDRange(renderSampleWorkGroupSize));
				// Merge all pixel samples and accumulate statistics
				oclQueue.enqueueNDRangeKernel(*mergePixelSamplesKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(threadFilmPixelCount, mergePixelSamplesWorkGroupSize)),
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
				// Statistics are accumulated by MergePixelSample kernel if not enableProgressiveRefinement
				const u_int step = engine->tileRepository->totalSamplesPerPixel;
				for (u_int i = 0; i < taskCount; i += step)
					tracedRaysCount += gpuTaskStats[i].raysCount;

				intersectionDevice->IntersectionKernelExecuted(tracedRaysCount);
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

				oclQueue.enqueueWriteBuffer(*mergedFrameBufferBuff, CL_FALSE, 0,
						mergedFrameBufferBuff->getInfo<CL_MEM_SIZE>(),
						engine->film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->GetPixels());

				//--------------------------------------------------------------
				// Normalize the merged buffer
				//--------------------------------------------------------------

				oclQueue.enqueueNDRangeKernel(*normalizeFBKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(engineFilmPixelCount, normalizeFBWorkGroupSize)),
						cl::NDRange(normalizeFBWorkGroupSize));

				//--------------------------------------------------------------
				// Apply tone mapping to merged buffer
				//--------------------------------------------------------------

				oclQueue.enqueueNDRangeKernel(*toneMapLinearKernel, cl::NullRange,
						cl::NDRange(RoundUp<u_int>(engineFilmPixelCount, toneMapLinearWorkGroupSize)),
						cl::NDRange(toneMapLinearWorkGroupSize));

				//--------------------------------------------------------------
				// Update the screen buffer
				//--------------------------------------------------------------

				oclQueue.enqueueNDRangeKernel(*updateScreenBufferKernel, cl::NullRange,
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
						oclQueue.enqueueNDRangeKernel(*applyBlurFilterXR1Kernel, cl::NullRange,
								cl::NDRange(RoundUp<unsigned int>(engineFilmPixelCount, applyBlurFilterXR1WorkGroupSize)),
								cl::NDRange(applyBlurFilterXR1WorkGroupSize));

						oclQueue.enqueueNDRangeKernel(*applyBlurFilterYR1Kernel, cl::NullRange,
								cl::NDRange(RoundUp<unsigned int>(engineFilmPixelCount, applyBlurFilterYR1WorkGroupSize)),
								cl::NDRange(applyBlurFilterYR1WorkGroupSize));
					}
				}

				//--------------------------------------------------------------
				// Transfer the screen frame buffer
				//--------------------------------------------------------------

				// The film has been locked before
				oclQueue.enqueueReadBuffer(*screenBufferBuff, CL_FALSE, 0,
						screenBufferBuff->getInfo<CL_MEM_SIZE>(), engine->film->GetScreenBuffer());
				oclQueue.finish();
			}

			//--------------------------------------------------------------
			// Update OpenCL buffers if there is any edit action
			//--------------------------------------------------------------

			if ((amiDisplayThread) && (engine->updateActions.HasAnyAction())) {
				engine->editMutex.lock();

				// Update all threads
				for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
					RTBiasPathOCLRenderThread *thread = (RTBiasPathOCLRenderThread *)(engine->renderThreads[i]);
					thread->UpdateOCLBuffers(engine->updateActions);
				}

				// Reset updateActions
				engine->updateActions.Reset();

				// Clear the film
				engine->film->Reset();

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
	oclQueue.finish();
}

#endif
