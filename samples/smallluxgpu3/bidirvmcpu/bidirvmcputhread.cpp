/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

// NOTE: this is code is heavily based on Tomas Davidovic's SmallVCM
// (http://www.davidovic.cz) and http://www.smallvcm.com)

#include "renderconfig.h"
#include "bidirvmcpu/bidirvmcpu.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sampler/sampler.h"

//------------------------------------------------------------------------------
// BiDirVMCPU RenderThread
//------------------------------------------------------------------------------

BiDirVMCPURenderThread::BiDirVMCPURenderThread(BiDirVMCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device, const u_int seedVal) :
		BiDirCPURenderThread(engine, index, device, seedVal) {
}

void BiDirVMCPURenderThread::RenderFuncVM() {
	//SLG_LOG("[BiDirVMCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirVMCPURenderEngine *engine = (BiDirVMCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(seed);
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();
	pixelCount = filmWidth * filmHeight;

	// Setup the samplers
	vector<Sampler *> samplers(engine->lightPathCount, NULL);
	const unsigned int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		engine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		engine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex
	for (u_int i = 0; i < samplers.size(); ++i) {
		Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film);
		sampler->RequestSamples(sampleSize);

		samplers[i] = sampler;
	}

	u_int iteration = 0;
	vector<vector<SampleResult> > samplesResults(samplers.size());
	vector<vector<PathVertexVM> > lightPathsVertices(samplers.size());
	vector<Point> lensPoints(samplers.size());
	while (!boost::this_thread::interruption_requested()) {
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

		const float vmFactor = M_PI * radius2 * engine->lightPathCount;
		vmNormalization = 1.f / vmFactor;

		const float etaVCM = vmFactor;
		misVmWeightFactor = MIS(etaVCM);
		misVcWeightFactor = MIS(1.f / etaVCM);

		//----------------------------------------------------------------------
		// Trace all light paths
		//----------------------------------------------------------------------

		for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex) {
			Sampler *sampler = samplers[samplerIndex];

			// Sample a point on the camera lens
			if (!camera->SampleLens(sampler->GetSample(3), sampler->GetSample(4),
					&lensPoints[samplerIndex]))
				continue;

			TraceLightPath(sampler, lensPoints[samplerIndex],
					lightPathsVertices[samplerIndex], samplesResults[samplerIndex]);
		}

		//----------------------------------------------------------------------
		// Store all light path vertices in the k-NN accelerator
		//----------------------------------------------------------------------

		HashGrid hashGrid(lightPathsVertices, radius);

		//----------------------------------------------------------------------
		// Trace all eye paths
		//----------------------------------------------------------------------

		for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex) {
			Sampler *sampler = samplers[samplerIndex];
			const vector<PathVertexVM> &lightPathVertices = lightPathsVertices[samplerIndex];

			PathVertexVM eyeVertex;
			SampleResult eyeSampleResult;
			eyeSampleResult.type = PER_PIXEL_NORMALIZED;
			eyeSampleResult.alpha = 1.f;

			Ray eyeRay;
			eyeSampleResult.screenX = min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
			eyeSampleResult.screenY = min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
			camera->GenerateRay(eyeSampleResult.screenX, eyeSampleResult.screenY, &eyeRay,
				sampler->GetSample(9), sampler->GetSample(10));

			eyeVertex.bsdf.fixedDir = -eyeRay.d;
			eyeVertex.throughput = Spectrum(1.f, 1.f, 1.f);
			const float cosAtCamera = Dot(scene->camera->GetDir(), eyeRay.d);
			const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
				scene->camera->GetPixelArea() * pixelCount);
			eyeVertex.dVCM = MIS(1.f / cameraPdfW);
			eyeVertex.dVC = 0.f;
			eyeVertex.dVM = 0.f;

			eyeVertex.depth = 1;
			while (eyeVertex.depth <= engine->maxEyePathDepth) {
				const unsigned int sampleOffset = sampleBootSize + engine->maxLightPathDepth * sampleLightStepSize +
					(eyeVertex.depth - 1) * sampleEyeStepSize;

				RayHit eyeRayHit;
				Spectrum connectionThroughput;
				if (!scene->Intersect(device, false, true, sampler->GetSample(sampleOffset), &eyeRay,
						&eyeRayHit, &eyeVertex.bsdf, &connectionThroughput)) {
					// Nothing was hit, look for infinitelight

					// This is a trick, you can not have a BSDF of something that has
					// not been hit. DirectHitInfiniteLight must be aware of this.
					eyeVertex.bsdf.fixedDir = -eyeRay.d;
					eyeVertex.throughput *= connectionThroughput;

					DirectHitLight(false, eyeVertex, &eyeSampleResult.radiance);

					if (eyeVertex.depth == 1)
						eyeSampleResult.alpha = 0.f;
					break;
				}
				eyeVertex.throughput *= connectionThroughput;

				// Something was hit

				// Update MIS constants
				eyeVertex.dVCM *= MIS(eyeRayHit.t * eyeRayHit.t);
				const float factor = 1.f / MIS(AbsDot(eyeVertex.bsdf.shadeN, eyeVertex.bsdf.fixedDir));
				eyeVertex.dVCM *= factor;
				eyeVertex.dVC *= factor;
				eyeVertex.dVM *= factor;

				// Check if it is a light source
				if (eyeVertex.bsdf.IsLightSource()) {
					DirectHitLight(true, eyeVertex, &eyeSampleResult.radiance);

					// SLG light sources are like black bodies
					break;
				}

				// Note: pass-through check is done inside SceneIntersect()

				//--------------------------------------------------------------
				// Direct light sampling
				//--------------------------------------------------------------

				DirectLightSampling(sampler->GetSample(sampleOffset + 1),
						sampler->GetSample(sampleOffset + 2),
						sampler->GetSample(sampleOffset + 3),
						sampler->GetSample(sampleOffset + 4),
						sampler->GetSample(sampleOffset + 5),
						eyeVertex, &eyeSampleResult.radiance);

				if (!eyeVertex.bsdf.IsDelta()) {
					//----------------------------------------------------------
					// Connect vertex path ray with all light path vertices
					//----------------------------------------------------------

					for (vector<PathVertexVM>::const_iterator lightPathVertex = lightPathVertices.begin();
							lightPathVertex < lightPathVertices.end(); ++lightPathVertex)
						ConnectVertices(eyeVertex, *lightPathVertex, &eyeSampleResult,
								sampler->GetSample(sampleOffset + 6));

					//----------------------------------------------------------
					// Vertex Merging step
					//----------------------------------------------------------

					hashGrid.Process(this, eyeVertex, &eyeSampleResult.radiance);
				}

				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				if (!Bounce(sampler, sampleOffset + 7, &eyeVertex, &eyeRay))
					break;

				++(eyeVertex.depth);
			}

			samplesResults[samplerIndex].push_back(eyeSampleResult);
		}

		//----------------------------------------------------------------------
		// Splat all samples
		//----------------------------------------------------------------------

		for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex)
			samplers[samplerIndex]->NextSample(samplesResults[samplerIndex]);

		++iteration;
	}

	for (u_int samplerIndex = 0; samplerIndex < samplers.size(); ++samplerIndex)
		delete samplers[samplerIndex];
	delete rndGen;

	//SLG_LOG("[BiDirVMCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
