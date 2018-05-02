/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/randomgen.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathocl/pathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLNativeRenderThread
//------------------------------------------------------------------------------

PathOCLNativeRenderThread::PathOCLNativeRenderThread(const u_int index,
		NativeThreadIntersectionDevice *device, PathOCLRenderEngine *re) :
		PathOCLBaseNativeRenderThread(index, device, re) {
	threadFilm = NULL;
}

PathOCLNativeRenderThread::~PathOCLNativeRenderThread() {
	delete threadFilm; 
}

void PathOCLNativeRenderThread::Start() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;

	const u_int filmWidth = engine->film->GetWidth();
	const u_int filmHeight = engine->film->GetHeight();
	const u_int *filmSubRegion = engine->film->GetSubRegion();

	delete threadFilm;

	threadFilm = new Film(filmWidth, filmHeight, filmSubRegion);
	threadFilm->CopyDynamicSettings(*(engine->film));
	threadFilm->RemoveChannel(Film::IMAGEPIPELINE);
	threadFilm->SetImagePipelines(NULL);
	// I collect samples statistics only on the GPUs. This solution is a bit
	// tricky but is simpler and a faster at the same time.
	threadFilm->GetDenoiser().SetEnabled(false);
	threadFilm->Init();

	PathOCLBaseNativeRenderThread::Start();
}

void PathOCLNativeRenderThread::RenderThreadImpl() {
	//SLG_LOG("[PathOCLRenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);

	// Clear the film (i.e. the thread can be started/stopped multiple times)
	threadFilm->Clear();
	
	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, threadFilm, NULL,
			engine->samplerSharedData);
	sampler->RequestSamples(pathTracer.sampleSize);
	
	VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

	// Initialize SampleResult
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	pathTracer.InitSampleResults(engine->film, sampleResults);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	// I can not use engine->renderConfig->GetProperty() here because the
	// RenderConfig properties cache is not thread safe
	const u_int filmWidth = threadFilm->GetWidth();
	const u_int filmHeight = threadFilm->GetHeight();
	const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>() *
		filmWidth * filmHeight;

	for (u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!boost::this_thread::interruption_requested() && engine->pauseMode)
				boost::this_thread::sleep(boost::posix_time::millisec(100));

			if (boost::this_thread::interruption_requested())
				break;
		}

		pathTracer.RenderSample(intersectionDevice, engine->renderConfig->scene, threadFilm, sampler, sampleResults);

		// Variance clamping
		if (varianceClamping.hasClamping())
			varianceClamping.Clamp(*threadFilm, sampleResult);

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif

		// Check halt conditions
		if ((haltDebug > 0u) && (steps >= haltDebug))
			break;
		if (engine->film->GetConvergence() == 1.f)
			break;
	}

	delete sampler;
	delete rndGen;

	threadDone = true;

	//SLG_LOG("[PathOCLRenderEngine::" << threadIndex << "] Rendering thread halted");
}

#endif
