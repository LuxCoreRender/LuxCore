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

void BiDirCPURenderEngine::ConnectVertices(
		const PathVertex &eyeVertex, const PathVertex &lightVertex,
		SampleResult *eyeSampleResult, const float u0) const {
	Scene *scene = renderConfig->scene;

	Vector eyeDir(lightVertex.bsdf.hitPoint - eyeVertex.bsdf.hitPoint);
	const float eyeDistance2 = eyeDir.LengthSquared();
	const float eyeDistance = sqrtf(eyeDistance2);
	eyeDir /= eyeDistance;

	// Check eye vertex BSDF
	float eyeBsdfPdfW, eyeBsdfRevPdfW;
	BSDFEvent eyeEvent;
	Spectrum eyeBsdfEval = eyeVertex.bsdf.Evaluate(eyeDir, &eyeEvent, &eyeBsdfPdfW, &eyeBsdfRevPdfW);

	if (!eyeBsdfEval.Black()) {
		// Check light vertex BSDF
		float lightBsdfPdfW, lightBsdfRevPdfW;
		BSDFEvent lightEvent;
		Spectrum lightBsdfEval = eyeVertex.bsdf.Evaluate(eyeDir, &lightEvent, &lightBsdfPdfW, &lightBsdfRevPdfW);

		if (!lightBsdfEval.Black()) {
			// Check the 2 surfaces can see each other
			const float cosThetaAtCamera = Dot(eyeVertex.bsdf.shadeN, eyeDir);
			const float cosThetaAtLight = Dot(lightVertex.bsdf.shadeN, -eyeDir);
			const float geometryTerm = cosThetaAtLight * cosThetaAtLight / eyeDistance2;
			if (geometryTerm <= 0.f)
				return;

			// Trace  ray between the two vertices
			Ray eyeRay(eyeVertex.bsdf.hitPoint, eyeDir);
			eyeRay.maxt = eyeDistance - MachineEpsilon::E(eyeDistance);
			RayHit eyeRayHit;
			BSDF bsdfConn;
			Spectrum connectionThroughput;
			if (!scene->Intersect(true, true, u0, &eyeRay, &eyeRayHit, &bsdfConn, &connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				if (eyeVertex.depth >= rrDepth) {
					// Russian Roulette
					const float prob = Max(eyeBsdfEval.Filter(), rrImportanceCap);
					eyeBsdfPdfW *= prob;
					eyeBsdfRevPdfW *= prob;
				}

				if (lightVertex.depth >= rrDepth) {
					// Russian Roulette
					const float prob = Max(lightBsdfEval.Filter(), rrImportanceCap);
					lightBsdfPdfW *= prob;
					lightBsdfRevPdfW *= prob;
				}

				// Convert pdfs to area pdf
				const float eyeBsdfPdfA = PdfWtoA(eyeBsdfPdfW, eyeDistance, cosThetaAtLight);
				const float lightBsdfPdfA  = PdfWtoA(lightBsdfPdfW,  eyeDistance, cosThetaAtCamera);

				// MIS weights
				const float lightWeight = eyeBsdfPdfA * (lightVertex.d0 + lightVertex.d1vc * lightBsdfRevPdfW);
				const float eyeWeight = lightBsdfPdfA * (eyeVertex.d0 + eyeVertex.d1vc * eyeBsdfRevPdfW);

				const float misWeight = 1.f / (lightWeight + 1.f + eyeWeight);

				eyeSampleResult->radiance += (misWeight * geometryTerm) * eyeBsdfEval * lightBsdfEval;
			}
		}
	}
}

void BiDirCPURenderEngine::ConnectToEye(const unsigned int pixelCount, 
		const PathVertex &lightVertex, const float u0,
		const Point &lensPoint, vector<SampleResult> &sampleResults) const {
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

				if (lightVertex.depth >= rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfEval.Filter(), rrImportanceCap);
					bsdfPdfW *= prob;
					bsdfRevPdfW *= prob;
				}

				const float cosToCamera = Dot(lightVertex.bsdf.shadeN, -eyeDir);
				const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

				const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
					scene->camera->GetPixelArea());
				const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
				const float fluxToRadianceFactor = cameraPdfA;

				// MIS weight (cameraPdfA must be expressed normalized device coordinate)
				const float weight = 1.f / ((cameraPdfA / pixelCount) *
					(lightVertex.d0 + lightVertex.d1vc * bsdfRevPdfW) + 1.f); // Balance heuristic (MIS)
				const Spectrum radiance = weight * connectionThroughput * lightVertex.throughput * fluxToRadianceFactor * bsdfEval;

				AddSampleResult(sampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
						radiance, 1.f);
			}
		}
	}
}

void BiDirCPURenderEngine::DirectLightSampling(
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertex &eyeVertex,
		Spectrum *radiance) const {
	Scene *scene = renderConfig->scene;
	
	if (!eyeVertex.bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(u0, &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW, emissionPdfW, cosThetaAtLight;
		Spectrum lightRadiance = light->Illuminate(scene, eyeVertex.bsdf.hitPoint,
				u1, u2, u3, &lightRayDir, &distance, &directPdfW, &emissionPdfW,
				&cosThetaAtLight);

		if (!lightRadiance.Black()) {
			BSDFEvent event;
			float bsdfPdfW, bsdfRevPdfW;
			Spectrum bsdfEval = eyeVertex.bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW, &bsdfRevPdfW);

			if (!bsdfEval.Black()) {
				Ray shadowRay(eyeVertex.bsdf.hitPoint, lightRayDir,
						MachineEpsilon::E(eyeVertex.bsdf.hitPoint),
						distance - MachineEpsilon::E(distance));
				RayHit shadowRayHit;
				BSDF shadowBsdf;
				Spectrum connectionThroughput;
				// Check if the light source is visible
				if (!scene->Intersect(false, false, u4, &shadowRay, &shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					if (eyeVertex.depth >= rrDepth) {
						// Russian Roulette
						const float prob = Max(bsdfEval.Filter(), rrImportanceCap);
						bsdfPdfW *= prob;
						bsdfRevPdfW *= prob;
					}

					const float cosThetaToLight = AbsDot(lightRayDir, eyeVertex.bsdf.shadeN);
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;

					// emissionPdfA / directPdfA = emissionPdfW / directPdfW
					const float weightLight  = bsdfPdfW / directLightSamplingPdfW;
					const float weightCamera = (emissionPdfW * cosThetaToLight / (directPdfW * cosThetaAtLight)) *
						(eyeVertex.d0 + eyeVertex.d1vc * bsdfRevPdfW);
					const float misWeight = 1.f / (weightLight + 1.f + weightCamera);

					const float factor = cosThetaToLight / directLightSamplingPdfW;

					*radiance += (misWeight * factor) * eyeVertex.throughput * connectionThroughput * lightRadiance * bsdfEval;
				}
			}
		}
	}
}

void BiDirCPURenderEngine::DirectHitFiniteLight(const PathVertex &eyeVertex, Spectrum *radiance) const {
	Scene *scene = renderConfig->scene;

	float directPdfA, emissionPdfW;
	const Spectrum lightRadiance = eyeVertex.bsdf.GetEmittedRadiance(scene, &directPdfA, &emissionPdfW);
	if (lightRadiance.Black())
		return;
	
	if (eyeVertex.depth == 1) {
		*radiance += eyeVertex.throughput * lightRadiance;
		return;
	}

	const float lightPickPdf = scene->PickLightPdf();
	directPdfA *= lightPickPdf;
	emissionPdfW *= lightPickPdf;

	// MIS weight
	const float misWeight = 1.f / (directPdfA * eyeVertex.d0 + emissionPdfW * eyeVertex.d1vc + 1.f); // Balance heuristic (MIS)

	*radiance += eyeVertex.throughput * misWeight * lightRadiance;
}

void BiDirCPURenderEngine::DirectHitInfiniteLight(const PathVertex &eyeVertex,
		Spectrum *radiance) const {
	Scene *scene = renderConfig->scene;

	float directPdfA, emissionPdfW;
	Spectrum lightRadiance = scene->GetEnvLightsRadiance(eyeVertex.bsdf.fixedDir,
			Point(), &directPdfA, &emissionPdfW);
	if (lightRadiance.Black())
		return;

	if (eyeVertex.depth == 1) {
		*radiance += eyeVertex.throughput * lightRadiance;
		return;
	}

	const float lightPickPdf = scene->PickLightPdf();
	directPdfA *= lightPickPdf;
	emissionPdfW *= lightPickPdf;

	// MIS weight
	const float misWeight = 1.f / (directPdfA * eyeVertex.d0 + emissionPdfW * eyeVertex.d1vc + 1.f); // Balance heuristic (MIS)

	*radiance += eyeVertex.throughput * misWeight * lightRadiance;
}

void BiDirCPURenderEngine::RenderThreadFuncImpl(CPURenderThread *renderThread) {
	//SLG_LOG("[BiDirCPURenderThread::" << renderThread->threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirCPURenderEngine *renderEngine = (BiDirCPURenderEngine *)renderThread->renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(renderThread->threadIndex + renderThread->seed);
	Scene *scene = renderEngine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = renderThread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();
	const unsigned int pixelCount = filmWidth * filmHeight;

	// Setup the sampler
	Sampler *sampler = renderEngine->renderConfig->AllocSampler(rndGen, film);
	const unsigned int sampleBootSize = 9;
	const unsigned int sampleLightStepSize = 6;
	const unsigned int sampleEyeStepSize = 11;
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
		renderEngine->threadSamplesCount[renderThread->threadIndex] += 1.0;
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
		float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
		Ray nextEventRay;
		lightVertex.throughput = light->Emit(scene,
			sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5), sampler->GetSample(6),
			&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW, &lightDirectPdfW, &cosThetaAtLight);
		if (!lightVertex.throughput.Black()) {
			lightEmitPdfW *= lightPickPdf;
			lightDirectPdfW *= lightPickPdf;

			lightVertex.throughput /= lightEmitPdfW;
			assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

			// I don't store the light vertex 0 because direct lighting will take
			// care of this kind of paths
			lightVertex.d0 = lightDirectPdfW / lightEmitPdfW; // Balance heuristic (MIS)
			const float usedCosLight = light->IsEnvironmental() ? 1.f : cosThetaAtLight;
            lightVertex.d1vc = usedCosLight / lightEmitPdfW;

			lightVertex.depth = 1;
			while (lightVertex.depth <= renderEngine->maxLightPathDepth) {
				const unsigned int sampleOffset = sampleBootSize + (lightVertex.depth - 1) * sampleLightStepSize;

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
					// Infinite lights use MIS based on solid angle instead of area
					if((lightVertex.depth > 1) || !light->IsEnvironmental())
                        lightVertex.d0 *= nextEventRayHit.t * nextEventRayHit.t;
					const float invCosTheta = 1.f / AbsDot(lightVertex.bsdf.shadeN, nextEventRay.d);
                    lightVertex.d0 *= invCosTheta;
					lightVertex.d1vc *= invCosTheta;

					// Store the vertex only if it isn't specular
					if (!lightVertex.bsdf.IsDelta()) {
						lightPathVertices.push_back(lightVertex);

						//------------------------------------------------------
						// Try to connect the light path vertex with the eye
						//------------------------------------------------------
						renderEngine->ConnectToEye(pixelCount, lightVertex, sampler->GetSample(sampleOffset + 1),
								lensPoint, sampleResults);
					}

					if (lightVertex.depth >= renderEngine->maxLightPathDepth)
						break;

					//----------------------------------------------------------
					// Build the next vertex path ray
					//----------------------------------------------------------

					Vector sampledDir;
					BSDFEvent event;
					float bsdfPdfW, cosSampledDir;
					const Spectrum bsdfSample = lightVertex.bsdf.Sample(&sampledDir,
							sampler->GetSample(sampleOffset + 2),
							sampler->GetSample(sampleOffset + 3),
							sampler->GetSample(sampleOffset + 4),
							&bsdfPdfW, &cosSampledDir, &event);
					if (bsdfSample.Black())
						break;

					float bsdfRevPdfW;
					if (event & SPECULAR)
						bsdfRevPdfW = bsdfPdfW;
					else
						lightVertex.bsdf.Pdf(sampledDir, NULL, &bsdfRevPdfW);

					if (lightVertex.depth >= renderEngine->rrDepth) {
						// Russian Roulette
						const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
						if (sampler->GetSample(sampleOffset + 5) < prob) {
							bsdfPdfW *= prob;
							bsdfRevPdfW *= prob;
						} else
							break;
					}

					lightVertex.throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
					assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

					// New MIS weights
					if (event & SPECULAR) {
						lightVertex.d0 = 0.f;
						lightVertex.d1vc *= (cosSampledDir / bsdfPdfW) * bsdfRevPdfW;
					} else {
						lightVertex.d1vc = (cosSampledDir / bsdfPdfW) * (lightVertex.d1vc *
								bsdfRevPdfW + lightVertex.d0);
						lightVertex.d0 = 1.f / bsdfPdfW;
					}

					nextEventRay = Ray(lightVertex.bsdf.hitPoint, sampledDir);
					++(lightVertex.depth);
				} else {
					// Ray lost in space...
					break;
				}
			}
		}

		//----------------------------------------------------------------------
		// Trace eye path
		//----------------------------------------------------------------------

		PathVertex eyeVertex;
		SampleResult eyeSampleResult;
		eyeSampleResult.type = PER_PIXEL_NORMALIZED;
		eyeSampleResult.alpha = 1.f;

		Ray eyeRay;
		eyeSampleResult.screenX = min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
		eyeSampleResult.screenY = min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
		camera->GenerateRay(eyeSampleResult.screenX, eyeSampleResult.screenY, &eyeRay,
			sampler->GetSample(7), sampler->GetSample(8));

		eyeVertex.bsdf.fixedDir = -eyeRay.d;
		eyeVertex.throughput = Spectrum(1.f, 1.f, 1.f);
		const float cosAtCamera = Dot(scene->camera->GetDir(), eyeRay.d);
		const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
			scene->camera->GetPixelArea() * pixelCount);
		eyeVertex.d0 = 1.f / cameraPdfW; // Balance heuristic (MIS)
		eyeVertex.d1vc = 0.f;

		eyeVertex.depth = 1;
		while (eyeVertex.depth <= renderEngine->maxEyePathDepth) {
			const unsigned int sampleOffset = sampleBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize +
				(eyeVertex.depth - 1) * sampleEyeStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			if (!scene->Intersect(false, true, sampler->GetSample(sampleOffset), &eyeRay,
					&eyeRayHit, &eyeVertex.bsdf, &connectionThroughput)) {
				// Nothing was hit, look for infinitelight

				// This is a trick, you can not have a BSDF of something that has
				// not been hit. DirectHitInfiniteLight must be aware of this.
				eyeVertex.bsdf.fixedDir = -eyeRay.d;
				eyeVertex.throughput *= connectionThroughput;

				renderEngine->DirectHitInfiniteLight(eyeVertex, &eyeSampleResult.radiance);

				if (eyeVertex.depth == 1)
					eyeSampleResult.alpha = 0.f;
				break;
			}
			eyeVertex.throughput *= connectionThroughput;

			// Something was hit

			// Update MIS constants
			eyeVertex.d0 *= eyeRayHit.t * eyeRayHit.t; // Balance heuristic (MIS)
			const float invCosTheta = 1.f / AbsDot(eyeVertex.bsdf.shadeN, eyeVertex.bsdf.fixedDir);
			eyeVertex.d0 *= invCosTheta;
			eyeVertex.d1vc *= invCosTheta;

			// Check if it is a light source
			if (eyeVertex.bsdf.IsLightSource()) {
				renderEngine->DirectHitFiniteLight(eyeVertex, &eyeSampleResult.radiance);

				// SLG light sources are like black bodies
				break;
			}

			// Note: pass-through check is done inside SceneIntersect()

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			renderEngine->DirectLightSampling(sampler->GetSample(sampleOffset + 1),
					sampler->GetSample(sampleOffset + 2),
					sampler->GetSample(sampleOffset + 3),
					sampler->GetSample(sampleOffset + 4),
					sampler->GetSample(sampleOffset + 5),
					eyeVertex, &eyeSampleResult.radiance);

			//------------------------------------------------------------------
			// Connect vertex path ray with all light path vertices
			//------------------------------------------------------------------

			if (!eyeVertex.bsdf.IsDelta()) {
				for (vector<PathVertex>::const_iterator lightPathVertex = lightPathVertices.begin();
						lightPathVertex < lightPathVertices.end(); ++lightPathVertex)
					renderEngine->ConnectVertices(eyeVertex, *lightPathVertex, &eyeSampleResult,
							sampler->GetSample(sampleOffset + 6));
			}

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			Vector sampledDir;
			BSDFEvent event;
			float cosSampledDir, bsdfPdfW;
			const Spectrum bsdfSample = eyeVertex.bsdf.Sample(&sampledDir,
					sampler->GetSample(sampleOffset + 7),
					sampler->GetSample(sampleOffset + 8),
					sampler->GetSample(sampleOffset + 9),
					&bsdfPdfW, &cosSampledDir, &event);
			if (bsdfSample.Black())
				break;

			float bsdfRevPdfW;
			if (event & SPECULAR)
				bsdfRevPdfW = bsdfPdfW;
			else
				eyeVertex.bsdf.Pdf(sampledDir, NULL, &bsdfRevPdfW);

			if (eyeVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
				if (prob > sampler->GetSample(sampleOffset + 10)) {
					bsdfPdfW *= prob;
					bsdfRevPdfW *= prob;
				} else
					break;
			}

			eyeVertex.throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
			assert (!eyeVertex.throughput.IsNaN() && !eyeVertex.throughput.IsInf());

			// New MIS weights
			if (event & SPECULAR) {
				eyeVertex.d0 = 0.f;
				eyeVertex.d1vc *= (cosSampledDir / bsdfPdfW) * bsdfRevPdfW;
			} else {
				eyeVertex.d1vc = (cosSampledDir / bsdfPdfW) * (eyeVertex.d1vc *
						bsdfRevPdfW + eyeVertex.d0);
				eyeVertex.d0 = 1.f / bsdfPdfW;
			}

			eyeRay = Ray(eyeVertex.bsdf.hitPoint, sampledDir);
			++(eyeVertex.depth);
		}

		sampleResults.push_back(eyeSampleResult);

		sampler->NextSample(sampleResults);
	}

	//SLG_LOG("[BiDirCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
