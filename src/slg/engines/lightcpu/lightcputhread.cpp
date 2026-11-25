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

#include <cassert>
#include "luxrays/utils/thread.h"

#include "slg/engines/lightcpu/lightcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace std::literals::chrono_literals;

//------------------------------------------------------------------------------
// LightCPU RenderThread
//------------------------------------------------------------------------------

LightCPURenderThread::LightCPURenderThread(
	LightCPURenderEngine *engine,
	const u_int index,
	IntersectionDeviceRef device) :
		CPUNoTileRenderThread(engine, index, device) {
}

void LightCPURenderThread::RenderFunc(std::stop_token stop_token) {
#ifndef NDEBUG
	SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread started");
#endif

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	LightCPURenderEngine *engine = (LightCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	auto rndGen = std::make_unique<RandomGenerator>(engine->seedBase + 1 + threadIndex);

	// Setup the sampler
	auto sampler = engine->renderConfig.AllocSampler(rndGen, engine->GetFilm(),
			engine->GetSampleSplatter(), engine->samplerSharedData,
			// Disable image plane meaning for samples 0 and 1
			Properties() << Property("sampler.imagesamples.enable")(false));
	sampler->SetThreadIndex(threadIndex);
	sampler->RequestSamples(SCREEN_NORMALIZED_ONLY, pathTracer.lightSampleSize);

	VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults;
	for(u_int steps = 0; !stop_token.stop_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!stop_token.stop_requested() && engine->pauseMode)
				std::this_thread::sleep_for(100ms);

			if (stop_token.stop_requested())
				break;
		}

		pathTracer.RenderLightSample(device, engine->renderConfig.GetScene(),
				engine->GetFilm(), *sampler, sampleResults);

		// Variance clamping
		if (varianceClamping.hasClamping()) {
			for(u_int i = 0; i < sampleResults.size(); ++i)
				varianceClamping.Clamp(engine->GetFilm(), sampleResults[i]);
		}

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
        std::this_thread::yield();
#endif

		// Check halt conditions
		if (engine->GetFilm().GetConvergence() == 1.f)
			break;
	}

	threadDone = true;

#ifndef NDEBUG
	SLG_LOG("[LightCPURenderThread::" << threadIndex << "] Rendering thread halted");
#endif
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
