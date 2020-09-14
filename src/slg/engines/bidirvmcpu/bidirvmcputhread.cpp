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

// NOTE: this is code is heavily based on Tomas Davidovic's SmallVCM
// (http://www.davidovic.cz) and http://www.smallvcm.com)

#include "luxrays/utils/thread.h"

#include "slg/engines/bidirvmcpu/bidirvmcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirVMCPU RenderThread
//------------------------------------------------------------------------------

BiDirVMCPURenderThread::BiDirVMCPURenderThread(BiDirVMCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		BiDirCPURenderThread(engine, index, device) {
}

void BiDirVMCPURenderThread::RenderFuncVM() {
	//SLG_LOG("[BiDirVMCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	BiDirVMCPURenderEngine *engine = (BiDirVMCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;

	// Setup the samplers
	vector<Sampler *> samplers(engine->lightPathsCount, NULL);
	const u_int sampleSize = 
		sampleBootSizeVM + // To generate the initial light vertex and trace eye ray
		engine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		engine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex

	for (u_int i = 0; i < samplers.size(); ++i) {
		Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, engine->film,
				engine->sampleSplatter,	engine->samplerSharedData, Properties());
		sampler->SetThreadIndex(threadIndex);
		sampler->RequestSamples(PIXEL_NORMALIZED_AND_SCREEN_NORMALIZED, sampleSize);

		samplers[i] = sampler;
	}

	u_int iteration = 0;
	vector<vector<SampleResult> > samplesResults(samplers.size());
	vector<vector<PathVertexVM> > lightPathsVertices(samplers.size());
	vector<Point> lensPoints(samplers.size());
	HashGrid hashGrid;

	for(u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!boost::this_thread::interruption_requested() && engine->pauseMode)
				boost::this_thread::sleep(boost::posix_time::millisec(100));

			if (boost::this_thread::interruption_requested())
				break;
		}

		// Clear the arrays
		for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex) {
			samplesResults[samplerIndex].clear();
			lightPathsVertices[samplerIndex].clear();
		}

		// Setup vertex merging
		float radius = engine->baseRadius;
        radius /= powf(float(iteration + 1), .5f * (1.f - engine->radiusAlpha));
		radius = Max(radius, DEFAULT_EPSILON_STATIC);
		const float radius2 = radius * radius;

		const float vmFactor = M_PI * radius2 * engine->lightPathsCount;
		vmNormalization = 1.f / vmFactor;

		const float etaVCM = vmFactor;
		misVmWeightFactor = MIS(etaVCM);
		misVcWeightFactor = MIS(1.f / etaVCM);

		// Using the same time for all rays in the same pass is required by the
		// current implementation (i.e. I can not mix paths with different
		// times). However this is detrimental for the Metropolis sampler.
		const float timeSample = rndGen->floatValue();
		const float time = scene->camera->GenerateRayTime(timeSample);

		//----------------------------------------------------------------------
		// Trace all light paths
		//----------------------------------------------------------------------

		for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex) {
			Sampler *sampler = samplers[samplerIndex];

			// Sample a point on the camera lens
			if (!camera->SampleLens(time, sampler->GetSample(3), sampler->GetSample(4),
					&lensPoints[samplerIndex]))
				continue;

			if (!TraceLightPath(time, sampler, lensPoints[samplerIndex],
					lightPathsVertices[samplerIndex], samplesResults[samplerIndex]))
				continue;
		}

		//----------------------------------------------------------------------
		// Store all light path vertices in the k-NN accelerator
		//----------------------------------------------------------------------

		hashGrid.Build(lightPathsVertices, radius);

		//cout << "==========================================\n";
		//cout << "Iteration: " << iteration << "  Paths: " << engine->lightPathsCount << "  Light path vertices: "<< hashGrid.GetVertexCount() <<"\n";

		//----------------------------------------------------------------------
		// Trace all eye paths
		//----------------------------------------------------------------------

		for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex) {
			Sampler *sampler = samplers[samplerIndex];

			PathVertexVM eyeVertex;
			SampleResult &eyeSampleResult = AddResult(samplesResults[samplerIndex], false);

			eyeSampleResult.filmX = sampler->GetSample(0);
			eyeSampleResult.filmY = sampler->GetSample(1);
			Ray eyeRay;
			camera->GenerateRay(time,
					eyeSampleResult.filmX, eyeSampleResult.filmY, &eyeRay,
					&eyeVertex.volInfo, sampler->GetSample(9), sampler->GetSample(10));

			eyeVertex.bsdf.hitPoint.fixedDir = -eyeRay.d;
			eyeVertex.throughput = Spectrum(1.f);
			float cameraPdfW;
			scene->camera->GetPDF(eyeRay, 0.f, eyeSampleResult.filmX, eyeSampleResult.filmY, &cameraPdfW, nullptr);
			eyeVertex.dVCM = MIS(1.f / cameraPdfW);
			eyeVertex.dVC = 1.f;
			eyeVertex.dVM = 1.f;

			eyeVertex.depth = 1;
			while (eyeVertex.depth <= engine->maxEyePathDepth) {
				eyeSampleResult.firstPathVertex = (eyeVertex.depth == 1);
				eyeSampleResult.lastPathVertex = (eyeVertex.depth == engine->maxEyePathDepth);

				const u_int sampleOffset = sampleBootSizeVM + engine->maxLightPathDepth * sampleLightStepSize +
					(eyeVertex.depth - 1) * sampleEyeStepSize;

				// NOTE: I account for volume emission only with path tracing (i.e. here and
				// not in any other place)
				RayHit eyeRayHit;
				Spectrum connectionThroughput, connectEmission;
				const bool hit = scene->Intersect(device,
						EYE_RAY | (eyeSampleResult.firstPathVertex ? CAMERA_RAY : GENERIC_RAY),
						&eyeVertex.volInfo, sampler->GetSample(sampleOffset),
						&eyeRay, &eyeRayHit, &eyeVertex.bsdf,
						&connectionThroughput, &eyeVertex.throughput, &eyeSampleResult);

				if (!hit) {
					// Nothing was hit, look for infinitelight

					// This is a trick, you can not have a BSDF of something that has
					// not been hit. DirectHitInfiniteLight must be aware of this.
					eyeVertex.bsdf.hitPoint.fixedDir = -eyeRay.d;
					eyeVertex.throughput *= connectionThroughput;

					DirectHitLight(false, eyeVertex, eyeSampleResult);

					if (eyeSampleResult.firstPathVertex) {
						eyeSampleResult.alpha = 0.f;
						eyeSampleResult.depth = std::numeric_limits<float>::infinity();
					}
					break;
				}
				eyeVertex.throughput *= connectionThroughput;

				// Something was hit
				if (eyeSampleResult.firstPathVertex) {
					eyeSampleResult.alpha = 1.f;
					eyeSampleResult.depth = eyeRayHit.t;
				}

				// Update MIS constants
				const float factor = 1.f / MIS(AbsDot(eyeVertex.bsdf.hitPoint.shadeN, eyeVertex.bsdf.hitPoint.fixedDir));
				eyeVertex.dVCM *= MIS(eyeRayHit.t * eyeRayHit.t) * factor;
				eyeVertex.dVC *= factor;
				eyeVertex.dVM *= factor;

				// Check if it is a light source
				if (eyeVertex.bsdf.IsLightSource())
					DirectHitLight(true, eyeVertex, eyeSampleResult);

				// Note: pass-through check is done inside Scene::Intersect()

				//--------------------------------------------------------------
				// Direct light sampling
				//--------------------------------------------------------------

				DirectLightSampling(time,
						sampler->GetSample(sampleOffset + 1),
						sampler->GetSample(sampleOffset + 2),
						sampler->GetSample(sampleOffset + 3),
						sampler->GetSample(sampleOffset + 4),
						sampler->GetSample(sampleOffset + 5),
						eyeVertex, eyeSampleResult);

				if (!eyeVertex.bsdf.IsDelta()) {
					//----------------------------------------------------------
					// Connect vertex path ray with all light path vertices
					//----------------------------------------------------------
			
					const vector<PathVertexVM> &lightPathVertices = lightPathsVertices[samplerIndex];
					for (vector<PathVertexVM>::const_iterator lightPathVertex = lightPathVertices.begin();
							lightPathVertex < lightPathVertices.end(); ++lightPathVertex)
						ConnectVertices(time,
								eyeVertex, *lightPathVertex, eyeSampleResult,
								sampler->GetSample(sampleOffset + 6));

					//----------------------------------------------------------
					// Vertex Merging step
					//----------------------------------------------------------

					hashGrid.Process(this, eyeVertex, &eyeSampleResult.radiance[0]);
				}

				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				if (!Bounce(time, sampler, sampleOffset + 7, &eyeVertex, &eyeRay))
					break;
			}
		}

		//----------------------------------------------------------------------
		// Splat all samples
		//----------------------------------------------------------------------

		for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex)
			samplers[samplerIndex]->NextSample(samplesResults[samplerIndex]);

		++iteration;

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif

		//hashGrid.PrintStatistics();

		// Check halt conditions
		if (engine->film->GetConvergence() == 1.f)
			break;
	}

	for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex)
		delete samplers[samplerIndex];
	delete rndGen;

	threadDone = true;

	//SLG_LOG("[BiDirVMCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
