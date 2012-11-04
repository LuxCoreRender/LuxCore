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

#include "renderconfig.h"
#include "bidircpu/bidircpu.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sampler/sampler.h"
#include "lightcpu/lightcpu.h"

//------------------------------------------------------------------------------
// BiDirCPU RenderThread
//------------------------------------------------------------------------------

void BiDirCPURenderEngine::ConnectToEye(const unsigned int pixelCount, 
		const PathVertex &lightVertex, const float u0,
		const Point &lensPoint, vector<SampleResult> &sampleResults) {
	Scene *scene = renderConfig->scene;

	Vector eyeDir(lightVertex.bsdf.hitPoint - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	float bsdfPdfW, bsdfRevPdfW;
	BSDFEvent event;
	Spectrum bsdfEval = lightVertex.bsdf.Evaluate(-eyeDir, &event, &bsdfPdfW, &bsdfRevPdfW);

	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir);
		eyeRay.maxt = eyeDistance - MachineEpsilon::E(eyeDistance);

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(lensPoint, eyeDir, eyeDistance, &scrX, &scrY)) {
			RayHit eyeRayHit;
			BSDF bsdfConn;
			Spectrum connectionThroughput;
			if (!scene->Intersect(true, true, u0, &eyeRay, &eyeRayHit, &bsdfConn, &connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				const float cosToCamera = Dot(lightVertex.bsdf.shadeN, -eyeDir);
				const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

				const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
					scene->camera->GetPixelArea());
				const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
				const float fluxToRadianceFactor = cameraPdfA;

				// MIS weights (cameraPdfA must be expressed normalized device coordinate)
				const float weight = 1.f / ((cameraPdfA / pixelCount) * lightVertex.d0); // Balance heuristic (MIS)
				const Spectrum radiance = weight * connectionThroughput * lightVertex.throughput * fluxToRadianceFactor * bsdfEval;

				AddSampleResult(sampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
						radiance, 1.f);
			}
		}
	}
}

void BiDirCPURenderEngine::RenderThreadFuncImpl(CPURenderThread *renderThread) {
	//SLG_LOG("[BiDirCPURenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirCPURenderEngine *renderEngine = (BiDirCPURenderEngine *)renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;
	//PerspectiveCamera *camera = scene->camera;
	Film *film = renderThread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();
	const unsigned int pixelCount = filmWidth * filmHeight;

	// Setup the sampler
	Sampler *sampler = renderEngine->renderConfig->AllocSampler(rndGen, film);
	const unsigned int sampleBootSize = 10;
	const unsigned int sampleEyeStepSize = 6;
	const unsigned int sampleLightStepSize = 6;
	const unsigned int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		renderEngine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		renderEngine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex
	sampler->RequestSamples(sampleSize);

	vector<SampleResult> sampleResults;
	vector<PathVertex> lightPathVertices;
	vector<PathVertex> eyePathVertices;
	renderEngine->threadSamplesCount[renderThread->threadIndex] = 0.0;
	while (!boost::this_thread::interruption_requested()) {
		renderEngine->threadSamplesCount[renderThread->threadIndex] += 1;
		sampleResults.clear();
		lightPathVertices.clear();
		eyePathVertices.clear();

		// Sample a point on the camera lens
		Point lensPoint;
		if (!scene->camera->SampleLens(rndGen->floatValue(), rndGen->floatValue(),
				&lensPoint)) {
			sampler->NextSample(sampleResults);
			continue;
		}

		//----------------------------------------------------------------------
		// Trace light path
		//----------------------------------------------------------------------

		// Select one light source
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(sampler->GetSample(2), &lightPickPdf);

		// Initialize the light path
		PathVertex lightVertex;
		float lightEmitPdfW, lightDirectPdfW;
		Ray nextEventRay;
		lightVertex.throughput = light->Emit(scene,
			sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5), sampler->GetSample(6),
			&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW, &lightDirectPdfW);
		if (!lightVertex.throughput.Black()) {
			lightVertex.throughput /= lightEmitPdfW * lightPickPdf;
			assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

			// I don't store the light vertex 0 because direct lighting will take
			// care of this kind of paths
			lightVertex.d0 = lightDirectPdfW / lightEmitPdfW; // Balance heuristic (MIS)

			int depth = 1;
			while (depth <= renderEngine->maxLightPathDepth) {
				const unsigned int sampleOffset = sampleBootSize + (depth - 1) * sampleLightStepSize;

				RayHit nextEventRayHit;
				Spectrum connectionThroughput;
				if (scene->Intersect(true, true, sampler->GetSample(sampleOffset),
						&nextEventRay, &nextEventRayHit, &lightVertex.bsdf, &connectionThroughput)) {
					// Something was hit

					// Check if it is a light source
					if (lightVertex.bsdf.IsLightSource()) {
						// SLG light sources are like black bodies
						break;
					}

					// Update the new light vertex
					lightVertex.throughput *= connectionThroughput;
					lightVertex.depth = depth;
					// Infinite lights use MIS based on solid angle instead of area
					if((depth > 1) && light->IsEnvironmental())
                        lightVertex.d0 *= nextEventRayHit.t * nextEventRayHit.t;
                    lightVertex.d0 /= AbsDot(lightVertex.bsdf.shadeN, nextEventRay.d);

					// Store the vertex only if it isn't specular
					if (!lightVertex.bsdf.IsDelta()) {
						lightPathVertices.push_back(lightVertex);

						// Try to connect the light path vertex with the eye
						renderEngine->ConnectToEye(pixelCount, lightVertex, sampler->GetSample(sampleOffset + 1),
								lensPoint, sampleResults);
					}

					if (depth >= renderEngine->maxLightPathDepth)
						break;

					//----------------------------------------------------------
					// Build the next vertex path ray
					//----------------------------------------------------------

					float bsdfPdf;
					Vector sampledDir;
					BSDFEvent event;
					float cosSampleDir;
					const Spectrum bsdfSample = lightVertex.bsdf.Sample(&sampledDir,
							sampler->GetSample(sampleOffset + 2),
							sampler->GetSample(sampleOffset + 3),
							sampler->GetSample(sampleOffset + 4),
							&bsdfPdf, &cosSampleDir, &event);
					if ((bsdfPdf <= 0.f) || bsdfSample.Black())
						break;

					if (depth >= renderEngine->rrDepth) {
						// Russian Roulette
						const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
						if (prob > sampler->GetSample(sampleOffset + 5))
							bsdfPdf *= prob;
						else
							break;
					}

					lightVertex.throughput *= bsdfSample * (cosSampleDir / bsdfPdf);
					assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());
					if (event & SPECULAR)
						lightVertex.d0 = 0.f;
					else
						lightVertex.d0 = 1.f / bsdfPdf;

					nextEventRay = Ray(lightVertex.bsdf.hitPoint, sampledDir);
					++depth;
				} else {
					// Ray lost in space...
					break;
				}
			}
		}

		sampler->NextSample(sampleResults);
	}

	//SLG_LOG("[BiDirCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
