/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include <memory>
#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/randomgen.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/utils/thread.h"
#include "luxrays/devices/ocldevice.h"
#include "luxrays/kernels/kernels.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathocl/pathocl.h"

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#include <Windows.h>
#endif

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace std::literals::chrono_literals;

//------------------------------------------------------------------------------
// PathOCLNativeRenderThread
//------------------------------------------------------------------------------

PathOCLNativeRenderThread::PathOCLNativeRenderThread(const u_int index,
	NativeIntersectionDeviceRef device, PathOCLRenderEngine *re) :
	PathOCLBaseNativeRenderThread(index, device, re)
{}

PathOCLNativeRenderThread::~PathOCLNativeRenderThread() {
}

void PathOCLNativeRenderThread::Start() {
	if (threadIndex == 0) {
		// Only the first thread allocate a film. It is then used by all
		// other threads too.
		PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;

		const u_int filmWidth = engine->GetFilm().GetWidth();
		const u_int filmHeight = engine->GetFilm().GetHeight();
		const u_int *filmSubRegion = engine->GetFilm().GetSubRegion();

		threadFilm = Film::Create(filmWidth, filmHeight, filmSubRegion);
		threadFilm->CopyDynamicSettings(engine->GetFilm());
		// I'm not removing the pipeline and disabling the film denoiser
		// in order to support BCD denoiser.
		threadFilm->SetThreadCount(engine->renderNativeThreads.size());
		threadFilm->Init();
	}

	PathOCLBaseNativeRenderThread::Start();
}

void PathOCLNativeRenderThread::StartRenderThread() {
	if (threadIndex == 0) {
		// Clear the film (i.e. the thread can be started/stopped multiple times)
		threadFilm->Clear();
	}

	PathOCLBaseNativeRenderThread::StartRenderThread();
}

FilmPtr PathOCLNativeRenderThread::GetThreadFilmPtr() {
	PathOCLRenderEngine * engine = static_cast<PathOCLRenderEngine *>(renderEngine);
	auto * thread0 = static_cast<PathOCLNativeRenderThread *>(
		engine->renderNativeThreads[0]
	);
	return FilmPtr(thread0->threadFilm.get());
}

FilmRef PathOCLNativeRenderThread::GetThreadFilm() {
	return *GetThreadFilmPtr();
}

void PathOCLNativeRenderThread::RenderThreadImpl(std::stop_token stop_token) {
	//SLG_LOG("[PathOCLRenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	// (engine->seedBase + 1) seed is used for sharedRndGen
	auto rndGen = std::make_unique<RandomGenerator>(engine->seedBase + 1 + threadIndex);

	// All threads use the film allocated by the first thread
	FilmPtr film = GetThreadFilmPtr();
	//FilmRef film = ((PathOCLNativeRenderThread *)(engine->renderNativeThreads[0]))->threadFilm;
	
	// Setup the sampler(s)

	SamplerUPtr lightSampler;

	auto eyeSampler = engine->renderConfig.AllocSampler(
		rndGen, film,
		engine->GetSampleSplatter(),
		engine->eyeSamplerSharedData, Properties()
	);
	eyeSampler->SetThreadIndex(threadIndex);
	eyeSampler->RequestSamples(PIXEL_NORMALIZED_ONLY, pathTracer.eyeSampleSize);

	if (pathTracer.hybridBackForwardEnable) {
		// Light path sampler is always Metropolis
		Properties props;
		props <<
			Property("sampler.type")("METROPOLIS") <<
			// Disable image plane meaning for samples 0 and 1
			Property("sampler.imagesamples.enable")(false) <<
			Property("sampler.metropolis.addonlycaustics")(true);

		lightSampler = Sampler::FromProperties(props, rndGen, film, engine->lightSampleSplatter,
				engine->lightSamplerSharedData);
		lightSampler->SetThreadIndex(threadIndex);
		lightSampler->RequestSamples(SCREEN_NORMALIZED_ONLY, pathTracer.lightSampleSize);
	}
	
	VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

	// Setup PathTracer thread state
	PathTracerThreadState pathTracerThreadState(
		intersectionDevice,
		eyeSampler,
		lightSampler,
		engine->renderConfig.GetScene(),
		*film,
		&varianceClamping
	);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	for (u_int steps = 0; !stop_token.stop_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!stop_token.stop_requested() && engine->pauseMode)
				std::this_thread::sleep_for(100ms);

			if (stop_token.stop_requested())
				break;
		}

		pathTracer.RenderSample(pathTracerThreadState);

#ifdef WIN32
		// Work around Windows bad scheduling
        std::this_thread::yield();
#endif

		// Check halt conditions
		if (engine->GetFilm().GetConvergence() == 1.f)
			break;
                if (stop_token.stop_requested())
                        break;

		if (engine->photonGICache) {
			engine->photonGICache->Update(engine->renderOCLThreads.size() + threadIndex, engine->GetTotalEyeSPP());
		}
	}


	threadDone = true;

	// This is done to stop threads pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	if (engine->photonGICache)
		engine->photonGICache->FinishUpdate(threadIndex);

	//SLG_LOG("[PathOCLRenderEngine::" << threadIndex << "] Rendering thread halted");
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
