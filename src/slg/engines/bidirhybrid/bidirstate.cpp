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
#include "slg/renderconfig.h"
#include "slg/engines/bidirhybrid/bidirhybrid.h"

#include "luxrays/core/intersectiondevice.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirState
//------------------------------------------------------------------------------

static inline float MIS(const float a) {
	//return a; // Balance heuristic
	return a * a; // Power heuristic
}

const unsigned int sampleEyeBootSize = 6;
const unsigned int sampleEyeStepSize = 11;
const unsigned int sampleLightBootSize = 6;
const unsigned int sampleLightStepSize = 6;

BiDirState::BiDirState(BiDirHybridRenderThread *renderThread,
		Film *film, luxrays::RandomGenerator *rndGen) : HybridRenderState(renderThread, film, rndGen),
		eyeSampleResults(((BiDirHybridRenderEngine *)renderThread->renderEngine)->eyePathCount) {
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)renderThread->renderEngine;

	// Setup the sampler
	const unsigned int sampleSize = 
		renderEngine->eyePathCount * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize) + // For each eye path and eye vertex
		renderEngine->lightPathCount * (sampleLightBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize); // For each light path and light vertex
	sampler->RequestSamples(sampleSize);
}

void BiDirState::ConnectVertices(HybridRenderThread *renderThread,
		const u_int eyePathIndex,
		const PathVertex &eyeVertex, const PathVertex &lightVertex,
		const float u0) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;

	Vector eyeDir(lightVertex.bsdf.hitPoint.p - eyeVertex.bsdf.hitPoint.p);
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
		Spectrum lightBsdfEval = lightVertex.bsdf.Evaluate(-eyeDir, &lightEvent, &lightBsdfPdfW, &lightBsdfRevPdfW);

		if (!lightBsdfEval.Black()) {
			// Check the 2 surfaces can see each other
			const float cosThetaAtCamera = Dot(eyeVertex.bsdf.hitPoint.shadeN, eyeDir);
			const float cosThetaAtLight = Dot(lightVertex.bsdf.hitPoint.shadeN, -eyeDir);
			const float geometryTerm = 1.f / eyeDistance2;
			if (geometryTerm <= 0.f)
				return;

			// Trace ray between the two vertices
			const float epsilon = Max(MachineEpsilon::E(eyeVertex.bsdf.hitPoint.p), MachineEpsilon::E(eyeDistance));
			Ray eyeRay(eyeVertex.bsdf.hitPoint.p, eyeDir,
					epsilon,
					eyeDistance - epsilon);

			if (eyeVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = RenderEngine::RussianRouletteProb(eyeBsdfEval, renderEngine->rrImportanceCap);
				eyeBsdfPdfW *= prob;
				eyeBsdfRevPdfW *= prob;
			}

			if (lightVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = RenderEngine::RussianRouletteProb(lightBsdfEval, renderEngine->rrImportanceCap);
				lightBsdfPdfW *= prob;
				lightBsdfRevPdfW *= prob;
			}

			// Convert pdfs to area pdfs
			const float eyeBsdfPdfA = PdfWtoA(eyeBsdfPdfW, eyeDistance, cosThetaAtLight);
			const float lightBsdfPdfA  = PdfWtoA(lightBsdfPdfW,  eyeDistance, cosThetaAtCamera);

			// MIS weights
			const float lightWeight = eyeBsdfPdfA * (lightVertex.dVCM + lightVertex.dVC * MIS(lightBsdfRevPdfW));
			const float eyeWeight = lightBsdfPdfA * (eyeVertex.dVCM + eyeVertex.dVC * MIS(eyeBsdfRevPdfW));

			const float misWeight = 1.f / (renderEngine->lightPathCount * (lightWeight + 1.f + eyeWeight));

			const Spectrum radiance = (misWeight * geometryTerm) * eyeVertex.throughput * eyeBsdfEval *
						lightBsdfEval * lightVertex.throughput;

			// Add the ray to trace and the result
			eyeSampleResults[eyePathIndex].sampleValue.push_back(u0);
			thread->PushRay(eyeRay);
			eyeSampleResults[eyePathIndex].sampleRadiance.push_back(radiance);
		}
	}
}

bool BiDirState::ConnectToEye(HybridRenderThread *renderThread,
		const PathVertex &lightVertex, const float u0,
		const Point &lensPoint) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	Vector eyeDir(lightVertex.bsdf.hitPoint.p - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	float bsdfPdfW, bsdfRevPdfW;
	BSDFEvent event;
	Spectrum bsdfEval = lightVertex.bsdf.Evaluate(-eyeDir, &event, &bsdfPdfW, &bsdfRevPdfW);

	if (!bsdfEval.Black()) {
		const float epsilon = Max(MachineEpsilon::E(lensPoint), MachineEpsilon::E(eyeDistance));
		Ray eyeRay(lensPoint, eyeDir,
				epsilon,
				eyeDistance - epsilon);

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(&eyeRay, &scrX, &scrY)) {
			if (lightVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = RenderEngine::RussianRouletteProb(bsdfEval, renderEngine->rrImportanceCap);
				bsdfPdfW *= prob;
				bsdfRevPdfW *= prob;
			}

			const float cosToCamera = Dot(lightVertex.bsdf.hitPoint.shadeN, -eyeDir);
			const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

			const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
				scene->camera->GetPixelArea());
			const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
			const float fluxToRadianceFactor = cameraPdfW / (eyeDistance * eyeDistance);

			const float weightLight = MIS(cameraPdfA) *
					(lightVertex.dVCM + lightVertex.dVC * MIS(bsdfRevPdfW));
			const float misWeight = 1.f / (renderEngine->lightPathCount * (weightLight + 1.f));

			const Spectrum radiance = misWeight * lightVertex.throughput * fluxToRadianceFactor * bsdfEval;

			// Add the ray to trace and the result
			lightSampleValue.push_back(u0);
			thread->PushRay(eyeRay);
			SampleResult::AddSampleResult(lightSampleResults, scrX, scrY, radiance);
			return true;
		}
	}

	return false;
}

void BiDirState::DirectLightSampling(HybridRenderThread *renderThread,
		const u_int eyePathIndex,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertex &eyeVertex) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	
	if (!eyeVertex.bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(u0, &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW, emissionPdfW, cosThetaAtLight;
		Spectrum lightRadiance = light->Illuminate(*scene, eyeVertex.bsdf.hitPoint.p,
				u1, u2, u3, &lightRayDir, &distance, &directPdfW, &emissionPdfW,
				&cosThetaAtLight);

		if (!lightRadiance.Black()) {
			BSDFEvent event;
			float bsdfPdfW, bsdfRevPdfW;
			Spectrum bsdfEval = eyeVertex.bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW, &bsdfRevPdfW);

			if (!bsdfEval.Black()) {
				const float epsilon = Max(MachineEpsilon::E(eyeVertex.bsdf.hitPoint.p), MachineEpsilon::E(distance));
				Ray shadowRay(eyeVertex.bsdf.hitPoint.p, lightRayDir,
						epsilon,
						distance - epsilon);

				if (eyeVertex.depth >= renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = RenderEngine::RussianRouletteProb(bsdfEval, renderEngine->rrImportanceCap);
					bsdfPdfW *= prob;
					bsdfRevPdfW *= prob;
				}

				const float cosThetaToLight = AbsDot(lightRayDir, eyeVertex.bsdf.hitPoint.shadeN);
				const float directLightSamplingPdfW = directPdfW * lightPickPdf;

				// emissionPdfA / directPdfA = emissionPdfW / directPdfW
				const float weightLight = MIS(bsdfPdfW / directLightSamplingPdfW);
				const float weightCamera = MIS(emissionPdfW * cosThetaToLight / (directPdfW * cosThetaAtLight)) *
					(eyeVertex.dVCM + eyeVertex.dVC * MIS(bsdfRevPdfW));
				const float misWeight = 1.f / (weightLight + 1.f + weightCamera);

				const float factor = 1.f / directLightSamplingPdfW;
		
				const Spectrum radiance = (misWeight * factor) * eyeVertex.throughput * lightRadiance * bsdfEval;

				// Add the ray to trace and the result
				eyeSampleResults[eyePathIndex].sampleValue.push_back(u4);
				thread->PushRay(shadowRay);
				eyeSampleResults[eyePathIndex].sampleRadiance.push_back(radiance);
			}
		}
	}
}

void BiDirState::DirectHitLight(HybridRenderThread *renderThread,
		const LightSource *light, const Spectrum &lightRadiance,
		const float directPdfA, const float emissionPdfW,
		const PathVertex &eyeVertex, Spectrum *radiance) const {
	if (lightRadiance.Black())
		return;

	if (eyeVertex.depth == 1) {
		*radiance += eyeVertex.throughput * lightRadiance;
		return;
	}

	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	const float lightPickPdf = scene->lightDefs.GetLightStrategy()->SampleLightPdf(light);

	// MIS weight
	const float weightCamera = MIS(directPdfA * lightPickPdf) * eyeVertex.dVCM +
		MIS(emissionPdfW * lightPickPdf) * eyeVertex.dVC;
	const float misWeight = 1.f / (weightCamera + 1.f);

	*radiance += misWeight * eyeVertex.throughput * lightRadiance;
}

void BiDirState::DirectHitLight(HybridRenderThread *renderThread,
		const bool finiteLightSource, const PathVertex &eyeVertex, Spectrum *radiance) const {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	float directPdfA, emissionPdfW;
	if (finiteLightSource) {
		const Spectrum lightRadiance = eyeVertex.bsdf.GetEmittedRadiance(&directPdfA, &emissionPdfW);
		DirectHitLight(renderThread, eyeVertex.bsdf.GetLightSource(), lightRadiance, directPdfA, emissionPdfW, eyeVertex, radiance);
	} else {
		BOOST_FOREACH(EnvLightSource *el, scene->lightDefs.GetEnvLightSources()) {
			const Spectrum lightRadiance = el->GetRadiance(*scene, eyeVertex.bsdf.hitPoint.fixedDir, &directPdfA, &emissionPdfW);
			DirectHitLight(renderThread, el, lightRadiance, directPdfA, emissionPdfW, eyeVertex, radiance);
		}
	}
}

void BiDirState::TraceLightPath(HybridRenderThread *renderThread,
		Sampler *sampler, const u_int lightPathIndex,
		vector<vector<PathVertex> > &lightPaths) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	const u_int lightPathSampleOffset = renderEngine->eyePathCount * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize) +
		lightPathIndex * (sampleLightBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize);

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(sampler->GetSample(lightPathSampleOffset), &lightPickPdf);

	// Initialize the light path
	PathVertex lightVertex;
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray lightRay;
	lightVertex.throughput = light->Emit(*scene,
			sampler->GetSample(lightPathSampleOffset + 1), sampler->GetSample(lightPathSampleOffset + 2),
			sampler->GetSample(lightPathSampleOffset + 3), sampler->GetSample(lightPathSampleOffset + 4),
			sampler->GetSample(lightPathSampleOffset + 5),
			&lightRay.o, &lightRay.d, &lightEmitPdfW, &lightDirectPdfW, &cosThetaAtLight);
	if (!lightVertex.throughput.Black()) {
		lightEmitPdfW *= lightPickPdf;
		lightDirectPdfW *= lightPickPdf;

		lightVertex.throughput /= lightEmitPdfW;
		assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

		// I don't store the light vertex 0 because direct lighting will take
		// care of these kind of paths
		lightVertex.dVCM = MIS(lightDirectPdfW / lightEmitPdfW);
		const float usedCosLight = light->IsEnvironmental() ? 1.f : cosThetaAtLight;
		lightVertex.dVC = MIS(usedCosLight / lightEmitPdfW);

		lightVertex.depth = 1;
		while (lightVertex.depth <= renderEngine->maxLightPathDepth) {
			const unsigned int lightVertexSampleOffset = lightPathSampleOffset + sampleLightBootSize + (lightVertex.depth - 1) * sampleLightStepSize;

			RayHit nextEventRayHit;
			Spectrum connectionThroughput;
			if (scene->Intersect(thread->device, true, sampler->GetSample(lightVertexSampleOffset),
					&lightRay, &nextEventRayHit, &lightVertex.bsdf, &connectionThroughput)) {
				// Something was hit

				// Update the new light vertex
				lightVertex.throughput *= connectionThroughput;
				// Infinite lights use MIS based on solid angle instead of area
				if((lightVertex.depth > 1) || !light->IsEnvironmental())
					lightVertex.dVCM *= MIS(nextEventRayHit.t * nextEventRayHit.t);
				const float factor = 1.f / MIS(AbsDot(lightVertex.bsdf.hitPoint.shadeN, lightRay.d));
				lightVertex.dVCM *= factor;
				lightVertex.dVC *= factor;

				// Store the vertex only if it isn't specular
				if (!lightVertex.bsdf.IsDelta())
					lightPaths[lightPathIndex].push_back(lightVertex);

				if (lightVertex.depth >= renderEngine->maxLightPathDepth)
					break;

				//----------------------------------------------------------
				// Build the next vertex path ray
				//----------------------------------------------------------

				if (!Bounce(renderThread, sampler, lightVertexSampleOffset + 2,
						&lightVertex, &lightRay))
					break;

				++(lightVertex.depth);
			} else {
				// Ray lost in space...
				break;
			}
		}
	}
}

bool BiDirState::Bounce(HybridRenderThread *renderThread,
		Sampler *sampler, const u_int sampleOffset,
		PathVertex *pathVertex, Ray *nextEventRay) const {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;

	Vector sampledDir;
	BSDFEvent event;
	float bsdfPdfW, cosSampledDir;
	const Spectrum bsdfSample = pathVertex->bsdf.Sample(&sampledDir,
			sampler->GetSample(sampleOffset),
			sampler->GetSample(sampleOffset + 1),
			&bsdfPdfW, &cosSampledDir, &event);
	if (bsdfSample.Black())
		return false;

	float bsdfRevPdfW;
	if (event & SPECULAR)
		bsdfRevPdfW = bsdfPdfW;
	else
		pathVertex->bsdf.Pdf(sampledDir, NULL, &bsdfRevPdfW);

	if (pathVertex->depth >= renderEngine->rrDepth) {
		// Russian Roulette
		const float prob = RenderEngine::RussianRouletteProb(bsdfSample, renderEngine->rrImportanceCap);
		if (sampler->GetSample(sampleOffset + 3) < prob) {
			bsdfPdfW *= prob;
			bsdfRevPdfW *= prob;
		} else
			return false;
	}

	pathVertex->throughput *= bsdfSample;
	assert (!pathVertex->throughput.IsNaN() && !pathVertex->throughput.IsInf());

	// New MIS weights
	if (event & SPECULAR) {
		pathVertex->dVCM = 0.f;
		pathVertex->dVC *= MIS(cosSampledDir / bsdfPdfW) * MIS(bsdfRevPdfW);
	} else {
		pathVertex->dVC = MIS(cosSampledDir / bsdfPdfW) * (pathVertex->dVC *
				MIS(bsdfRevPdfW) + pathVertex->dVCM);
		pathVertex->dVCM = MIS(1.f / bsdfPdfW);
	}

	*nextEventRay = Ray(pathVertex->bsdf.hitPoint.p, sampledDir);

	return true;
}

void BiDirState::GenerateRays(HybridRenderThread *renderThread) {
	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	Camera *camera = scene->camera;
	Film *film = thread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();

	lightSampleValue.clear();
	lightSampleResults.clear();
	for (u_int eyePathIndex = 0; eyePathIndex < renderEngine->eyePathCount; ++eyePathIndex) {
		eyeSampleResults[eyePathIndex].lightPathVertexConnections = 0;
		eyeSampleResults[eyePathIndex].radiance = Spectrum();
		eyeSampleResults[eyePathIndex].sampleValue.clear();
		eyeSampleResults[eyePathIndex].sampleRadiance.clear();
	}

	//--------------------------------------------------------------------------
	// Build the set of light paths
	//--------------------------------------------------------------------------

	vector<vector<PathVertex> > lightPaths(renderEngine->lightPathCount);
	for (u_int lightPathIndex = 0; lightPathIndex < renderEngine->lightPathCount; ++lightPathIndex) {
		//----------------------------------------------------------------------
		// Trace a new light path
		//----------------------------------------------------------------------

		TraceLightPath(renderThread, sampler, lightPathIndex, lightPaths);
	}

	//--------------------------------------------------------------------------
	// Build the set of eye paths and trace rays
	//--------------------------------------------------------------------------

	for (u_int eyePathIndex = 0; eyePathIndex < renderEngine->eyePathCount; ++eyePathIndex) {
		//----------------------------------------------------------------------
		// Trace eye path
		//----------------------------------------------------------------------

		const u_int eyePathSampleOffset = eyePathIndex * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize);

		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera->SampleLens(0.f, sampler->GetSample(eyePathSampleOffset + 2), sampler->GetSample(eyePathSampleOffset + 3),
				&lensPoint))
			return;

		//----------------------------------------------------------------------
		// Trace all connections between all light paths and the first eye
		// vertex
		//----------------------------------------------------------------------

		if (eyePathIndex == 0) {
			// As suggested in the CBDPT paper (paragraph 3.2). I connect the light vertices
			// only with the first eye vertex of the first path. This is because, in a pinhole camera,
			// all the first vertices are the same (they are slightly different only if the lens radius
			// is > 0.0).
			for (u_int lightPathIndex = 0; lightPathIndex < lightPaths.size(); ++lightPathIndex) {
				const u_int lightPathSampleOffset = renderEngine->eyePathCount * (sampleEyeBootSize + renderEngine->maxEyePathDepth * sampleEyeStepSize) +
					lightPathIndex * (sampleLightBootSize + renderEngine->maxLightPathDepth * sampleLightStepSize);

				for (u_int lightVertexIndex = 0; lightVertexIndex < lightPaths[lightPathIndex].size(); ++lightVertexIndex) {
					const unsigned int lightVertexSampleOffset = lightPathSampleOffset + sampleLightBootSize + lightVertexIndex * sampleLightStepSize;

					//--------------------------------------------------------------
					// Try to connect the light path vertex with the eye
					//--------------------------------------------------------------
					if (ConnectToEye(renderThread, lightPaths[lightPathIndex][lightVertexIndex],
							sampler->GetSample(lightVertexSampleOffset + 1), lensPoint))
						eyeSampleResults[eyePathIndex].lightPathVertexConnections += 1;
				}
			}
		}

		PathVertex eyeVertex;
		eyeSampleResults[eyePathIndex].alpha = 1.f;

		Ray eyeRay;
		eyeSampleResults[eyePathIndex].filmX = luxrays::Min(sampler->GetSample(eyePathSampleOffset) * filmWidth, (float)(filmWidth - 1));
		eyeSampleResults[eyePathIndex].filmY = luxrays::Min(sampler->GetSample(eyePathSampleOffset + 1) * filmHeight, (float)(filmHeight - 1));
		camera->GenerateRay(eyeSampleResults[eyePathIndex].filmX, eyeSampleResults[eyePathIndex].filmY, &eyeRay,
			sampler->GetSample(eyePathSampleOffset + 4), sampler->GetSample(eyePathSampleOffset + 5), 0.f);

		eyeVertex.bsdf.hitPoint.fixedDir = -eyeRay.d;
		eyeVertex.throughput = Spectrum(1.f, 1.f, 1.f);
		const float cosAtCamera = Dot(scene->camera->GetDir(), eyeRay.d);
		const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
			scene->camera->GetPixelArea());
		eyeVertex.dVCM = MIS(1.f / cameraPdfW);
		eyeVertex.dVC = 0.f;

		eyeVertex.depth = 1;
		while (eyeVertex.depth <= renderEngine->maxEyePathDepth) {
			const unsigned int eyeVertexSampleOffset = eyePathSampleOffset + sampleEyeBootSize + (eyeVertex.depth - 1) * sampleEyeStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			if (!scene->Intersect(thread->device, false, sampler->GetSample(eyeVertexSampleOffset),
					&eyeRay, &eyeRayHit, &eyeVertex.bsdf, &connectionThroughput)) {
				// Nothing was hit, look for infinitelight

				// This is a trick, you can not have a BSDF of something that has
				// not been hit. DirectHitInfiniteLight must be aware of this.
				eyeVertex.bsdf.hitPoint.fixedDir = -eyeRay.d;
				eyeVertex.throughput *= connectionThroughput;

				DirectHitLight(renderThread, false, eyeVertex, &eyeSampleResults[eyePathIndex].radiance);

				if (eyeVertex.depth == 1)
					eyeSampleResults[eyePathIndex].alpha = 0.f;
				break;
			}
			eyeVertex.throughput *= connectionThroughput;

			// Something was hit

			// Update MIS constants
			eyeVertex.dVCM *= MIS(eyeRayHit.t * eyeRayHit.t);
			const float factor = 1.f / MIS(AbsDot(eyeVertex.bsdf.hitPoint.shadeN, eyeVertex.bsdf.hitPoint.fixedDir));
			eyeVertex.dVCM *= factor;
			eyeVertex.dVC *= factor;

			// Check if it is a light source
			if (eyeVertex.bsdf.IsLightSource())
				DirectHitLight(renderThread, true, eyeVertex, &eyeSampleResults[eyePathIndex].radiance);

			// Note: pass-through check is done inside Scene::Intersect()

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			DirectLightSampling(renderThread, eyePathIndex,
					sampler->GetSample(eyeVertexSampleOffset + 1),
					sampler->GetSample(eyeVertexSampleOffset + 2),
					sampler->GetSample(eyeVertexSampleOffset + 3),
					sampler->GetSample(eyeVertexSampleOffset + 4),
					sampler->GetSample(eyeVertexSampleOffset + 5),
					eyeVertex);

			//------------------------------------------------------------------
			// Connect vertex path ray with all light path vertices
			//------------------------------------------------------------------

			if (!eyeVertex.bsdf.IsDelta()) {
				for (u_int lightPathIndex = 0; lightPathIndex < lightPaths.size(); ++lightPathIndex) {
					for (u_int lightVertexIndex = 0; lightVertexIndex < lightPaths[lightPathIndex].size(); ++lightVertexIndex) {
						ConnectVertices(renderThread, eyePathIndex, eyeVertex, lightPaths[lightPathIndex][lightVertexIndex],
								sampler->GetSample(eyeVertexSampleOffset + 6));
					}
				}
			}

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			if (!Bounce(renderThread, sampler, eyeVertexSampleOffset + 7,
					&eyeVertex, &eyeRay))
				break;

			++(eyeVertex.depth);
		}
	}
	
	//SLG_LOG("[BiDirState::" << renderThread->threadIndex << "] Generated rays: " << lightSampleResults.size() + eyeSampleRadiance.size());
}

bool BiDirState::ValidResult(BiDirHybridRenderThread *renderThread,
		const Ray *ray, const RayHit *rayHit,
		const float u0, Spectrum *radiance) {
	if (rayHit->Miss())
		return true;
	else {
		BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)renderThread->renderEngine;
		Scene *scene = renderEngine->renderConfig->scene;

		// I have to check if it is a hit over a pass-through point
		BSDF bsdf(false, // true or false, here, doesn't really matter
				*scene, *ray, *rayHit, u0, NULL);

		// Check if it is a pass-through point
		Spectrum t = bsdf.GetPassThroughTransparency();
		if (!t.Black()) {
			*radiance *= t;

			// It is a pass-through material, continue to trace the ray. I do
			// this on the CPU.

			Ray newRay(*ray);
			newRay.mint = rayHit->t + MachineEpsilon::E(rayHit->t);
			RayHit newRayHit;			
			Spectrum connectionThroughput;
			if (scene->Intersect(renderThread->device, false, // true or false, here, doesn't really matter
					u0, &newRay, &newRayHit, &bsdf, &connectionThroughput)) {
				// Something was hit
				return false;
			} else {
				*radiance *= connectionThroughput;
				return true;
			}
		} else
			return false;
	}
}

double BiDirState::CollectResults(HybridRenderThread *renderThread) {
	BiDirHybridRenderThread *thread = (BiDirHybridRenderThread *)renderThread;
	BiDirHybridRenderEngine *renderEngine = (BiDirHybridRenderEngine *)thread->renderEngine;
	
	vector<SampleResult> validSampleResults;

	// Evaluate the RayHit results for each eye path
	SampleResult eyeSampleResult(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA, 1);
	u_int currentLightSampleResultsIndex = 0;
	for (u_int eyePathIndex = 0; eyePathIndex < renderEngine->eyePathCount; ++eyePathIndex) {
		// For each eye path, evaluate the RayHit results for eye to light path vertex connections
		for (u_int i = 0; i < eyeSampleResults[eyePathIndex].lightPathVertexConnections; ++i) {
			const Ray *ray;
			const RayHit *rayHit;
			thread->PopRay(&ray, &rayHit);

			if (ValidResult(thread, ray, rayHit, lightSampleValue[currentLightSampleResultsIndex],
					&lightSampleResults[currentLightSampleResultsIndex].radiancePerScreenNormalized[0]))
				validSampleResults.push_back(lightSampleResults[currentLightSampleResultsIndex]);

			++currentLightSampleResultsIndex;
		}

		eyeSampleResult.filmX = eyeSampleResults[eyePathIndex].filmX;
		eyeSampleResult.filmY = eyeSampleResults[eyePathIndex].filmY;
		eyeSampleResult.radiancePerPixelNormalized[0] = eyeSampleResults[eyePathIndex].radiance;
		eyeSampleResult.alpha = eyeSampleResults[eyePathIndex].alpha;
		for (u_int i = 0; i < eyeSampleResults[eyePathIndex].sampleRadiance.size(); ++i) {
			const Ray *ray;
			const RayHit *rayHit;
			thread->PopRay(&ray, &rayHit);

			if (ValidResult(thread, ray, rayHit, eyeSampleResults[eyePathIndex].sampleValue[i],
					&eyeSampleResults[eyePathIndex].sampleRadiance[i]))
				eyeSampleResult.radiancePerPixelNormalized[0] += eyeSampleResults[eyePathIndex].sampleRadiance[i];
		}
		validSampleResults.push_back(eyeSampleResult);
	}

	sampler->NextSample(validSampleResults);

	// This is a very raw (and somewhat boosted) approximation
	return renderEngine->eyePathCount * renderEngine->lightPathCount;
}
