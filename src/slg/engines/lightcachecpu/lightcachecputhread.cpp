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

#include "slg/engines/lightcachecpu/lightcachecpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCacheCPU RenderThread
//------------------------------------------------------------------------------

LightCacheCPURenderThread::LightCacheCPURenderThread(LightCacheCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device) {
}

SampleResult &LightCacheCPURenderThread::AddResult(vector<SampleResult> &sampleResults) const {
	LightCacheCPURenderEngine *engine = (LightCacheCPURenderEngine *)renderEngine;

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	SampleResult &sampleResult = sampleResults[size];

	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH | Film::SAMPLECOUNT,
			engine->film->GetRadianceGroupCount());

	return sampleResult;
}

void LightCacheCPURenderThread::TraceEyePath(Sampler *sampler, vector<SampleResult> &sampleResults) {
	LightCacheCPURenderEngine *engine = (LightCacheCPURenderEngine *)renderEngine;
	const Scene *scene = engine->renderConfig->scene;
	const Camera *camera = scene->camera;

	SampleResult &sampleResult = AddResult(sampleResults);

	sampleResult.filmX = sampler->GetSample(0);
	sampleResult.filmY = sampler->GetSample(1);

	Ray eyeRay;
	PathVolumeInfo volInfo;
	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));

	Spectrum eyePathThroughput(1.f);
	int depth = 1;
	while (depth <= engine->maxPathDepth) {
		sampleResult.firstPathVertex = (depth == 1);
		sampleResult.lastPathVertex = (depth == engine->maxPathDepth);

		const u_int sampleOffset = sampleBootSize + (depth - 1) * sampleStepSize;

		BSDF bsdf;	
		RayHit eyeRayHit;
		Spectrum connectionThroughput;
		const bool hit = scene->Intersect(device, false, sampleResult.firstPathVertex,
				&volInfo, sampler->GetSample(sampleOffset),
				&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput,
				&eyePathThroughput, &sampleResult);

		if (!hit) {
			// Nothing was hit, check infinite lights (including sun)
			BOOST_FOREACH(EnvLightSource *envLight, scene->lightDefs.GetEnvLightSources()) {
				const Spectrum envRadiance = envLight->GetRadiance(*scene, -eyeRay.d);
				sampleResult.AddEmission(envLight->GetID(), eyePathThroughput * connectionThroughput,
						envRadiance);
			}

			if (sampleResult.firstPathVertex) {
				sampleResult.alpha = 0.f;
				sampleResult.depth = std::numeric_limits<float>::infinity();
			}
			break;
		} else {
			// Something was hit, check if it is a light source
			
			if (sampleResult.firstPathVertex) {
				sampleResult.alpha = 1.f;
				sampleResult.depth = eyeRayHit.t;
			}

			if (bsdf.IsLightSource()) {
				sampleResult.AddEmission(bsdf.GetLightID(), eyePathThroughput * connectionThroughput,
						bsdf.GetEmittedRadiance());
				break;
			} else {
				// Check if it is a specular bounce

				float bsdfPdf;
				Vector sampledDir;
				BSDFEvent event;
				float cosSampleDir;
				const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 1),
						sampler->GetSample(sampleOffset + 2),
						&bsdfPdf, &cosSampleDir, &event);
				if (bsdfSample.Black() || ((depth == 1) && !(event & SPECULAR)))
					break;

				// If depth = 1 and it is a specular bounce, I continue to trace the
				// eye path looking for a light source

				eyePathThroughput *= connectionThroughput * bsdfSample;
				assert (!eyePathThroughput.IsNaN() && !eyePathThroughput.IsInf());

				eyeRay.Update(bsdf.hitPoint.p, sampledDir);
			}

			++depth;
		}
	}
}

void LightCacheCPURenderThread::RenderFunc() {
	//SLG_LOG("[LightCacheCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	LightCacheCPURenderEngine *engine = (LightCacheCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);

	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, engine->film,
			engine->sampleSplatter, engine->samplerSharedData);
	const u_int sampleSize = 
		sampleBootSize + // To generate the initial setup
		engine->maxPathDepth * sampleStepSize; // For each eye vertex
	sampler->RequestSamples(sampleSize);

	VarianceClamping varianceClamping(engine->sqrtVarianceClampMaxValue);

	//--------------------------------------------------------------------------
	// Trace eye paths
	//--------------------------------------------------------------------------

	// I can not use engine->renderConfig->GetProperty() here because the
	// RenderConfig properties cache is not thread safe
	const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>() *
		engine->film->GetWidth() * engine->film->GetHeight();
	
	vector<SampleResult> sampleResults;
	for(u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!boost::this_thread::interruption_requested() && engine->pauseMode)
				boost::this_thread::sleep(boost::posix_time::millisec(100));

			if (boost::this_thread::interruption_requested())
				break;
		}

		sampleResults.clear();
		TraceEyePath(sampler, sampleResults);

		// Variance clamping
		if (varianceClamping.hasClamping())
			for(u_int i = 0; i < sampleResults.size(); ++i)
				varianceClamping.Clamp(*(engine->film), sampleResults[i]);

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

	//SLG_LOG("[LightCacheCPURenderThread::" << threadIndex << "] Rendering thread halted");
}
