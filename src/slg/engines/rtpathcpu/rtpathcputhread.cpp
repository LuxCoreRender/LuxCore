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

#include "luxrays/utils/thread.h"

#include "slg/slg.h"
#include "slg/samplers/rtpathcpusampler.h"
#include "slg/engines/rtpathcpu/rtpathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RTPathCPURenderThread
//------------------------------------------------------------------------------

RTPathCPURenderThread::RTPathCPURenderThread(RTPathCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device) : 
	PathCPURenderThread(engine, index, device) {
}

RTPathCPURenderThread::~RTPathCPURenderThread() {
}

void RTPathCPURenderThread::StartRenderThread() {
	// Avoid to allocate the film thread because I'm going to use the global one

	CPURenderThread::StartRenderThread();
}

void RTPathCPURenderThread::RTRenderFunc() {
	//SLG_LOG("[RTPathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	RTPathCPURenderEngine *engine = (RTPathCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);
	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, engine->film, NULL,
			engine->samplerSharedData, Properties());
	((RTPathCPUSampler *)sampler)->SetRenderEngine(engine);
	sampler->RequestSamples(PIXEL_NORMALIZED_ONLY, pathTracer.eyeSampleSize);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	PathTracer::InitEyeSampleResults(engine->film, sampleResults);

	VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

	for (u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		// Check if we are in pause or edit mode
		if (engine->threadsPauseMode) {
			// Synchronize all threads
			engine->threadsSyncBarrier->wait();

			// Wait for the main thread
			engine->threadsSyncBarrier->wait();

			if (boost::this_thread::interruption_requested())
				break;

			((RTPathCPUSampler *)sampler)->Reset(engine->film);
		}

		pathTracer.RenderEyeSample(device, engine->renderConfig->scene,
				engine->film, sampler, sampleResults);

		// Variance clamping
		if (varianceClamping.hasClamping())
			varianceClamping.Clamp(*(engine->film), sampleResult);

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif
	}

	delete sampler;
	delete rndGen;

	threadDone = true;

	//SLG_LOG("[RTPathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
