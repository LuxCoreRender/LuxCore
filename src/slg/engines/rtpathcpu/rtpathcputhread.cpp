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

	RTPathCPURenderEngine *engine = (RTPathCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);
	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, engine->film, engine->sampleSplatter,
			engine->samplerSharedData);
	sampler->RequestSamples(engine->sampleSize);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults(1);
//	SampleResult &sampleResult = sampleResults[0];
	InitSampleResults(sampleResults);

	for (;;) {
//		VarianceClamping varianceClamping(engine->sqrtVarianceClampMaxValue);

		for (u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
			// Check if we are in pause mode
			if (engine->pauseMode) {
				// Check every 100ms if I have to continue the rendering
				while (!boost::this_thread::interruption_requested() && engine->pauseMode)
					boost::this_thread::sleep(boost::posix_time::millisec(100));

				if (boost::this_thread::interruption_requested())
					break;
			}

			if (engine->beginEditMode)
				break;

			RenderSample(engine->film, sampler, sampleResults);

			// Variance clamping
//			if (varianceClamping.hasClamping())
//				varianceClamping.Clamp(*threadFilm, sampleResult);

			sampler->NextSample(sampleResults);

#ifdef WIN32
			// Work around Windows bad scheduling
			renderThread->yield();
#endif
		}

		if (boost::this_thread::interruption_requested())
			break;
		if (engine->beginEditMode) {
			// Synchronize all threads
			engine->syncBarrier->wait();

			// Wait for the main thread to finish the scene editing
			engine->syncBarrier->wait();

			((RTPathCPUSampler *)sampler)->Reset(engine->film);
		}
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[RTPathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
