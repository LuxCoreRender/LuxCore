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

#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <numeric>
#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/randomgen.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/devices/ocldevice.h"
#include "luxrays/kernels/kernels.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathocl/pathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace std::literals::chrono_literals;

//------------------------------------------------------------------------------
// PathOCLRenderThread
//------------------------------------------------------------------------------

PathOCLOpenCLRenderThread::PathOCLOpenCLRenderThread(const u_int index,
		HardwareIntersectionDeviceRef device, PathOCLRenderEngine *re) :
		PathOCLBaseOCLRenderThread(index, device, re) {
}

PathOCLOpenCLRenderThread::~PathOCLOpenCLRenderThread() {
}

void PathOCLOpenCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight,
		u_int *filmSubRegion) {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	FilmConstRef engineFilm = engine->GetFilm();

	*filmWidth = engineFilm.GetWidth();
	*filmHeight = engineFilm.GetHeight();

	const u_int *subRegion = engineFilm.GetSubRegion();
	filmSubRegion[0] = subRegion[0];
	filmSubRegion[1] = subRegion[1];
	filmSubRegion[2] = subRegion[2];
	filmSubRegion[3] = subRegion[3];
}

void PathOCLOpenCLRenderThread::StartRenderThread() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;

	// I have to load the start film otherwise it is overwritten at the first
	// merge of all thread films
	if (engine->hasStartFilm && (threadIndex == 0))
		threadFilms[0]->GetFilm().AddFilm(engine->GetFilm());

	PathOCLBaseOCLRenderThread::StartRenderThread();
}

static void PGICUpdateCallBack(CompiledScene *compiledScene) {
	compiledScene->RecompilePhotonGI();
}

// The sum of all the light tracing (RADIANCE_PER_SCREEN_NORMALIZED) channels
static double LightChannelsEnergy(const Film &film) {
	double energy = 0.0;
	if (film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (auto const &channel : film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs) {
			const float *pixels = channel->GetPixels();
			energy = accumulate(pixels, pixels + channel->GetWidth() * channel->GetHeight() * 3, energy);
		}
	}

	return energy;
}

void PathOCLOpenCLRenderThread::RenderThreadImpl(std::stop_token stop_token) {
	//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;
	const u_int lightTaskCount = engine->lightTaskCount;

	intersectionDevice.PushThreadCurrentDevice();

	//----------------------------------------------------------------------
	// Execute initialization kernels
	//----------------------------------------------------------------------

	// Clear the frame buffer
	threadFilms[0]->ClearFilm(intersectionDevice, filmClearKernel, filmClearWorkGroupSize);

	// Initialize random number generator seeds
	intersectionDevice.EnqueueKernel(initSeedKernel,
			HardwareDeviceRange(engine->taskCount), HardwareDeviceRange(initWorkGroupSize));

	// Initialize the tasks buffer
	EnqueueInitKernel();
	if (lightTaskCount > 0)
		EnqueueInitLightPathsKernel();

	// Check if I have to load the start film
	if (engine->hasStartFilm && (threadIndex == 0))
		threadFilms[0]->SendFilm(intersectionDevice);

	//----------------------------------------------------------------------
	// Rendering loop
	//----------------------------------------------------------------------

	// The film refresh time target
	const double targetTime = 0.2; // 200ms

	u_int iterations = 4;
	u_int totalIterations = 0;

	double totalTransferTime = 0.0;
	double totalKernelTime = 0.0;

	const std::function<void()> pgicUpdateCallBack = std::bind(PGICUpdateCallBack, engine->compiledScene);

	// Light tracing duty cycle: the light kernels run every lightPeriod
	// iterations. When a window of light paths adds no energy to the light
	// tracing channels (no caustic in view), the period goes to
	// lightIdlePeriod; the first window that adds energy sets it back to 1.
	// The period changes only how many light paths are traced, and the
	// light channels are normalized by that count.
	const u_int lightIdlePeriod = 16;
	const double lightWindowPaths = 8.0 * lightTaskCount;
	u_int lightPeriod = 1;
	u_int lightIteration = 0;
	double lightPathTotal = 0.0;
	double lightWindowStart = 0.0;
	double lightWindowEnergy = 0.0;

	// Film reads: EnqueueFilmRead() queues non-blocking reads of the film and
	// of the sample counts, CompleteFilmRead() runs once they are on the host
	// and updates the film sample counts (the GPU light paths are the
	// RADIANCE_PER_SCREEN_NORMALIZED samples)
	auto EnqueueFilmRead = [&]() {
		// Async. transfer of the Film buffers
		threadFilms[0]->RecvFilm(intersectionDevice);

		// Async. transfer of GPU task statistics
		intersectionDevice.EnqueueReadBuffer(
			taskStatsBuff,
			CL_FALSE,
			sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount,
			gpuTaskStats.get());
		if (lightTaskCount > 0) {
			intersectionDevice.EnqueueReadBuffer(
				lightPathCountsBuff,
				CL_FALSE,
				sizeof(u_int) * lightTaskCount,
				lightPathCounts.get());
		}
	};

	// Path guiding: the sampling CDFs are rebuilt when this device has
	// rendered 2, 4, 8, ... samples per pixel, the last time at
	// path.guiding.trainingspp
	const bool pathGuiding = UsePathGuiding();
	const double guidingPixelCount = double(threadFilms[0]->GetFilm().GetWidth()) *
			threadFilms[0]->GetFilm().GetHeight();
	u_int guidingNextSpp = 2;
	double guidingSampleCount = 0.0;

	auto CompleteFilmRead = [&]() {
		// I need to update the film samples count

		double totalCount = 0.0;
		for (size_t i = 0; i < taskCount; ++i)
			totalCount += gpuTaskStats[i].sampleCount;
		guidingSampleCount = totalCount;
		lightPathTotal = 0.0;
		for (size_t i = 0; i < lightTaskCount; ++i)
			lightPathTotal += lightPathCounts[i];
		threadFilms[0]->GetFilm().SetSampleCount(totalCount, totalCount, lightPathTotal);

		if ((lightTaskCount > 0) && (lightPathTotal - lightWindowStart >= lightWindowPaths)) {
			const double energy = LightChannelsEnergy(threadFilms[0]->GetFilm());
			const u_int period = (energy > lightWindowEnergy) ? 1 : lightIdlePeriod;
			if (period != lightPeriod) {
				SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Light tracing kernels every " <<
						period << " iteration(s): the last " << (lightPathTotal - lightWindowStart) <<
						" light paths " << ((period == 1) ? "added" : "did not add") << " energy to the film");
				lightPeriod = period;
			}
			lightWindowStart = lightPathTotal;
			lightWindowEnergy = energy;
		}
	};

	// WallClockTime() has the 10-16ms resolution of GetTickCount() on Windows
	auto Now = []() {
		return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	};

	// The work is queued one batch of iterations ahead: the thread waits,
	// without spinning, for the work before the last batch while the device
	// runs that batch, so the device does not wait for the host between
	// batches. A queue marker follows each batch and each film read; markers
	// lists them, oldest first (true for a film read).
	std::deque<bool> markers;
	double lastBatchTime = Now();

	// The time of a film read, measured once with nothing else queued: the
	// host can not see when the device runs the copies of a queued read (on
	// Windows a marker can complete for the host only with the next batch).
	// Each film read adds it to totalTransferTime.
	u_int filmReadCount = 0;
	double filmReadTime = 0.0;

	// Waits for the oldest marker. A batch counts as the time since the
	// previous batch.
	auto WaitMarker = [&]() {
		intersectionDevice.WaitMarker();

		if (markers.front())
			CompleteFilmRead();
		else {
			const double batchTime = Now();
			const double time = batchTime - lastBatchTime;
			lastBatchTime = batchTime;
			totalKernelTime += time;

			// Check if I have to adjust the number of kernel enqueued
			if (time > targetTime)
				iterations = Max<u_int>(iterations - 1, 1);
			else
				iterations = Min<u_int>(iterations + 1, 128);
		}

		markers.pop_front();
	};

	auto DrainQueue = [&]() {
		while (!markers.empty())
			WaitMarker();
	};

	while (!stop_token.stop_requested()) {
		//if (threadIndex == 0)
		//	SLG_LOG("[DEBUG] =================================");

		// Check if we are in pause mode
		if (engine->pauseMode) {
			DrainQueue();

			// Check every 100ms if I have to continue the rendering
			while (!stop_token.stop_requested() && engine->pauseMode)
				std::this_thread::sleep_for(100ms);

			if (stop_token.stop_requested())
				break;
		}

		// The device is idle: the time to the next batch starts now
		if (markers.empty())
			lastBatchTime = Now();

		//------------------------------------------------------------------

		// Transfer the film only if I have already spent enough time running
		// rendering kernels. This is very important when rendering very high
		// resolution images (for instance at 4961x3508)

		if ((find(markers.begin(), markers.end(), true) == markers.end()) &&
				(totalTransferTime < totalKernelTime * (1.0 / 100.0))) {
			// The second read is the measured one: the first allocates the
			// pinned memory of the copies
			if (filmReadCount == 1) {
				DrainQueue();

				const double transferStart = Now();
				EnqueueFilmRead();
				intersectionDevice.FinishQueue();
				CompleteFilmRead();
				filmReadTime = Now() - transferStart;

				lastBatchTime = Now();
			} else {
				EnqueueFilmRead();
				intersectionDevice.EnqueueMarker();
				markers.push_back(true);
			}

			++filmReadCount;
			totalTransferTime += filmReadTime;
		}

		//------------------------------------------------------------------

		// This is required for updating film denoiser parameter
		if (threadFilms[0]->GetFilm().GetDenoiser().IsEnabled()) {
			std::unique_lock<std::mutex> lock(engine->setKernelArgsMutex);
			SetAllAdvancePathsKernelArgs(0);
		}

		for (u_int i = 0; i < iterations; ++i) {
			// The light rays follow the eye rays: when the light kernel does not
			// run, they are not traced and wait for the next light iteration
			const bool lightStep = (lightTaskCount > 0) && ((lightIteration++ % lightPeriod) == 0);

			// Trace rays
			const double profileStart = BeginKernelProfile();
			intersectionDevice.EnqueueTraceRayBuffer(raysBuff, hitsBuff,
					taskCount + (lightStep ? lightTaskCount : 0));
			EndKernelProfile(KERNEL_PROFILE_TRACE, profileStart);

			// Advance to next path state
			EnqueueAdvancePathsKernel();
			if (lightStep)
				EnqueueAdvanceLightPathsKernel();
		}
		totalIterations += iterations;

		intersectionDevice.EnqueueMarker();
		markers.push_back(false);

		// Wait for the work queued before this batch, except a film read just
		// before it: that one is waited for after the next batch is queued
		while ((markers.size() > 2) || ((markers.size() == 2) && !markers.front()))
			WaitMarker();

		// Check halt conditions
		if (engine->GetFilm().GetConvergence() == 1.f)
			break;

		if (pathGuiding && (guidingNextSpp > 0) &&
				(guidingSampleCount >= guidingNextSpp * guidingPixelCount)) {
			// Queued after the last batch: it runs between two iterations
			const bool stopTraining = (guidingNextSpp >= engine->pathTracer.guidingTrainingSpp);
			EnqueuePathGuidingRebuild(stopTraining);
			SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Path guiding CDFs rebuilt at " <<
					guidingNextSpp << " samples/pixel" << (stopTraining ? ", training done" : ""));
			guidingNextSpp = stopTraining ? 0 : Min(2 * guidingNextSpp, engine->pathTracer.guidingTrainingSpp);
		}

		if (engine->photonGICache) {
                    if (engine->photonGICache->Update(threadIndex, engine->GetTotalEyeSPP(), pgicUpdateCallBack)) {
                            // The queued batch uses the buffers InitPhotonGI() replaces
                            DrainQueue();
                            InitPhotonGI();
                            SetKernelArgs();
                    }
		}
	} // ~while

		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
        if (stop_token.stop_requested())
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");

	// The sample counts must match the final film
	DrainQueue();
	EnqueueFilmRead();
	intersectionDevice.FinishQueue();
	CompleteFilmRead();
	LogKernelProfile();
	
	threadDone = true;

	// This is done to stop threads pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	if (engine->photonGICache)
		engine->photonGICache->FinishUpdate(threadIndex);
	
	intersectionDevice.PopThreadCurrentDevice();
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
