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

// NOTE: this is code is heavily based on Tomas Davidovic's SmallVCM
// (http://www.davidovic.cz and http://www.smallvcm.com)

#include "slg/engines/bidircpu/bidircpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirCPU RenderThread
//------------------------------------------------------------------------------

BiDirCPURenderThread::BiDirCPURenderThread(BiDirCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device) {
}

SampleResult &BiDirCPURenderThread::AddResult(vector<SampleResult> &sampleResults, const bool fromLight) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	SampleResult &sampleResult = sampleResults[size];

	sampleResult.Init(
			fromLight ?
				Film::RADIANCE_PER_SCREEN_NORMALIZED :
				(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH),
			engine->film->GetRadianceGroupCount());

	return sampleResult;
}

void BiDirCPURenderThread::ConnectVertices(const float time,
		const PathVertexVM &eyeVertex, const PathVertexVM &lightVertex,
		SampleResult &eyeSampleResult, const float u0) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector p2pDir(lightVertex.bsdf.hitPoint.p - eyeVertex.bsdf.hitPoint.p);
	const float p2pDistance2 = p2pDir.LengthSquared();
	const float p2pDistance = sqrtf(p2pDistance2);
	p2pDir /= p2pDistance;

	// Check eye vertex BSDF
	float eyeBsdfPdfW, eyeBsdfRevPdfW;
	BSDFEvent eyeEvent;
	const Spectrum eyeBsdfEval = eyeVertex.bsdf.Evaluate(p2pDir, &eyeEvent, &eyeBsdfPdfW, &eyeBsdfRevPdfW);

	if (!eyeBsdfEval.Black()) {
		// Check light vertex BSDF
		float lightBsdfPdfW, lightBsdfRevPdfW;
		BSDFEvent lightEvent;
		const Spectrum lightBsdfEval = lightVertex.bsdf.Evaluate(-p2pDir, &lightEvent, &lightBsdfPdfW, &lightBsdfRevPdfW);

		if (!lightBsdfEval.Black()) {
			// Check if the 2 surfaces can see each other
			const float cosThetaAtCamera = Dot(eyeVertex.bsdf.hitPoint.shadeN, p2pDir);
			const float cosThetaAtLight = Dot(lightVertex.bsdf.hitPoint.shadeN, -p2pDir);
			// Was:
			//  const float geometryTerm = cosThetaAtCamera * cosThetaAtLight / eyeDistance2;
			//
			// but now BSDF::Evaluate() follows LuxRender habit to return the
			// result multiplied by cosThetaAtLight
			const float geometryTerm = 1.f / p2pDistance2;

			// Trace ray between the two vertices
			Ray p2pRay(eyeVertex.bsdf.hitPoint.p, p2pDir,
					0.f,
					p2pDistance,
					time);
			p2pRay.UpdateMinMaxWithEpsilon();
			RayHit p2pRayHit;
			BSDF bsdfConn;
			Spectrum connectionThroughput;
			PathVolumeInfo volInfo = eyeVertex.volInfo; // I need to use a copy here
			if (!scene->Intersect(device, true, &volInfo, u0, &p2pRay, &p2pRayHit, &bsdfConn,
					&connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				if (eyeVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = RenderEngine::RussianRouletteProb(eyeBsdfEval, engine->rrImportanceCap);
					eyeBsdfPdfW *= prob;
					eyeBsdfRevPdfW *= prob;
				}

				if (lightVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = RenderEngine::RussianRouletteProb(lightBsdfEval, engine->rrImportanceCap);
					lightBsdfPdfW *= prob;
					lightBsdfRevPdfW *= prob;
				}

				// Convert pdfs to area pdfs
				const float eyeBsdfPdfA = PdfWtoA(eyeBsdfPdfW, p2pDistance, cosThetaAtLight);
				const float lightBsdfPdfA  = PdfWtoA(lightBsdfPdfW,  p2pDistance, cosThetaAtCamera);

				// MIS weights
				const float lightWeight = MIS(eyeBsdfPdfA) *
					(misVmWeightFactor + lightVertex.dVCM + lightVertex.dVC * MIS(lightBsdfRevPdfW));
				const float eyeWeight = MIS(lightBsdfPdfA) *
					(misVmWeightFactor + eyeVertex.dVCM + eyeVertex.dVC * MIS(eyeBsdfRevPdfW));

				const float misWeight = 1.f / (lightWeight + 1.f + eyeWeight);

				eyeSampleResult.radiance[lightVertex.lightID] += (misWeight * geometryTerm) * eyeVertex.throughput * eyeBsdfEval *
						connectionThroughput * lightBsdfEval * lightVertex.throughput;
			}
		}
	}
}

void BiDirCPURenderThread::ConnectToEye(const float time,
		const PathVertexVM &lightVertex, const float u0,
		const Point &lensPoint, vector<SampleResult> &sampleResults) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector eyeDir(lightVertex.bsdf.hitPoint.p - lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	float bsdfPdfW, bsdfRevPdfW;
	BSDFEvent event;
	const Spectrum bsdfEval = lightVertex.bsdf.Evaluate(-eyeDir, &event, &bsdfPdfW, &bsdfRevPdfW);

	if (!bsdfEval.Black()) {
		Ray eyeRay(lensPoint, eyeDir,
				0.f,
				eyeDistance,
				time);
		eyeRay.UpdateMinMaxWithEpsilon();

		float scrX, scrY;
		if (scene->camera->GetSamplePosition(&eyeRay, &scrX, &scrY)) {
			// I have to flip the direction of the traced ray because
			// the information inside PathVolumeInfo are about the path from
			// the light toward the camera (i.e. ray.o would be in the wrong
			// place).
			Ray traceRay(lightVertex.bsdf.hitPoint.p, -eyeDir,
					0.f, eyeDistance, time);
			traceRay.UpdateMinMaxWithEpsilon();
			RayHit traceRayHit;

			BSDF bsdfConn;
			Spectrum connectionThroughput;
			PathVolumeInfo volInfo = lightVertex.volInfo; // I need to use a copy here
			if (!scene->Intersect(device, true, &volInfo, u0, &traceRay, &traceRayHit, &bsdfConn,
					&connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				if (lightVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = RenderEngine::RussianRouletteProb(bsdfEval, engine->rrImportanceCap);
					bsdfRevPdfW *= prob;
				}

				const float cosToCamera = Dot(lightVertex.bsdf.hitPoint.shadeN, -eyeDir);
				const float cosAtCamera = Dot(scene->camera->GetDir(), eyeDir);

				const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
					scene->camera->GetPixelArea());
				const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
				// Was:
				//  const float fluxToRadianceFactor = cameraPdfA;
				//
				// but now BSDF::Evaluate() follows LuxRender habit to return the
				// result multiplied by cosThetaToLight
				const float fluxToRadianceFactor = cameraPdfW / (eyeDistance * eyeDistance);

				const float weightLight = MIS(cameraPdfA) *
					(misVmWeightFactor + lightVertex.dVCM + lightVertex.dVC * MIS(bsdfRevPdfW));
				const float misWeight = 1.f / (weightLight + 1.f);

				const Spectrum radiance = (misWeight * fluxToRadianceFactor) *
					connectionThroughput * lightVertex.throughput * bsdfEval;

				SampleResult &sampleResult = AddResult(sampleResults, true);
				sampleResult.filmX = scrX;
				sampleResult.filmY = scrY;
				
				// Add radiance from the light source
				sampleResult.radiance[lightVertex.lightID] = radiance;
			}
		}
	}
}

void BiDirCPURenderThread::DirectLightSampling(const float time,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertexVM &eyeVertex,
		SampleResult &eyeSampleResult) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	
	if (!eyeVertex.bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(u0, &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW, emissionPdfW, cosThetaAtLight;
		const Spectrum lightRadiance = light->Illuminate(*scene, eyeVertex.bsdf.hitPoint.p,
				u1, u2, u3, &lightRayDir, &distance, &directPdfW, &emissionPdfW,
				&cosThetaAtLight);

		if (!lightRadiance.Black()) {
			BSDFEvent event;
			float bsdfPdfW, bsdfRevPdfW;
			const Spectrum bsdfEval = eyeVertex.bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW, &bsdfRevPdfW);

			if (!bsdfEval.Black()) {
				Ray shadowRay(eyeVertex.bsdf.hitPoint.p, lightRayDir,
						0.f,
						distance,
						time);
				shadowRay.UpdateMinMaxWithEpsilon();
				RayHit shadowRayHit;
				BSDF shadowBsdf;
				Spectrum connectionThroughput;
				PathVolumeInfo volInfo = eyeVertex.volInfo; // I need to use a copy here
				// Check if the light source is visible
				if (!scene->Intersect(device, false, &volInfo, u4, &shadowRay, &shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					// I'm ignoring volume emission because it is not sampled in
					// direct light step.

					// If the light source is not intersectable, it can not be
					// sampled with BSDF
					bsdfPdfW *= (light->IsEnvironmental() || light->IsIntersectable()) ? 1.f : 0.f;
					
					// The +1 is there to account the current path vertex used for DL
					if (eyeVertex.depth + 1 >= engine->rrDepth) {
						// Russian Roulette
						const float prob = RenderEngine::RussianRouletteProb(bsdfEval, engine->rrImportanceCap);
						bsdfPdfW *= prob;
						bsdfRevPdfW *= prob;
					}

					const float cosThetaToLight = AbsDot(lightRayDir, eyeVertex.bsdf.hitPoint.shadeN);
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;

					// emissionPdfA / directPdfA = emissionPdfW / directPdfW
					const float weightLight = MIS(bsdfPdfW / directLightSamplingPdfW);
					const float weightCamera = MIS(emissionPdfW * cosThetaToLight / (directPdfW * cosThetaAtLight)) *
						(misVmWeightFactor + eyeVertex.dVCM + eyeVertex.dVC * MIS(bsdfRevPdfW));
					const float misWeight = 1.f / (weightLight + 1.f + weightCamera);

					// Was:
					//  const float factor = cosThetaToLight / directLightSamplingPdfW;
					//
					// but now BSDF::Evaluate() follows LuxRender habit to return the
					// result multiplied by cosThetaToLight
					const float factor = 1.f / directLightSamplingPdfW;

					eyeSampleResult.radiance[light->GetID()] += (misWeight * factor) *
							eyeVertex.throughput * connectionThroughput * lightRadiance * bsdfEval;
				}
			}
		}
	}
}

void BiDirCPURenderThread::DirectHitLight(
		const LightSource *light, const Spectrum &lightRadiance,
		const float directPdfA, const float emissionPdfW,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const {
	if (lightRadiance.Black())
		return;

	if (eyeVertex.depth == 1) {
		*radiance += eyeVertex.throughput * lightRadiance;
		return;
	}

	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	const float lightPickPdf = scene->lightDefs.GetLightStrategy()->SampleLightPdf(light);

	// MIS weight
	const float weightCamera = MIS(directPdfA * lightPickPdf) * eyeVertex.dVCM +
		MIS(emissionPdfW * lightPickPdf) * eyeVertex.dVC;
	const float misWeight = 1.f / (weightCamera + 1.f);

	*radiance += misWeight * eyeVertex.throughput * lightRadiance;
}

void BiDirCPURenderThread::DirectHitLight(const bool finiteLightSource,
		const PathVertexVM &eyeVertex, SampleResult &eyeSampleResult) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	float directPdfA, emissionPdfW;
	if (finiteLightSource) {
		const Spectrum lightRadiance = eyeVertex.bsdf.GetEmittedRadiance(&directPdfA, &emissionPdfW);
		DirectHitLight(eyeVertex.bsdf.GetLightSource(), lightRadiance, directPdfA, emissionPdfW,
				eyeVertex, &eyeSampleResult.radiance[eyeVertex.bsdf.GetLightID()]);
	} else {
		BOOST_FOREACH(EnvLightSource *el, scene->lightDefs.GetEnvLightSources()) {
			const Spectrum lightRadiance = el->GetRadiance(*scene, eyeVertex.bsdf.hitPoint.fixedDir, &directPdfA, &emissionPdfW);
			DirectHitLight(el, lightRadiance, directPdfA, emissionPdfW, eyeVertex, &eyeSampleResult.radiance[el->GetID()]);
		}
	}
}

void BiDirCPURenderThread::TraceLightPath(const float time,
		Sampler *sampler, const Point &lensPoint,
		vector<PathVertexVM> &lightPathVertices,
		vector<SampleResult> &sampleResults) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(sampler->GetSample(2), &lightPickPdf);

	// Initialize the light path
	PathVertexVM lightVertex;
	lightVertex.lightID = light->GetID();
	
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray lightRay;
	lightVertex.throughput = light->Emit(*scene,
		sampler->GetSample(5), sampler->GetSample(6), sampler->GetSample(7), sampler->GetSample(8), sampler->GetSample(9),
		&lightRay.o, &lightRay.d, &lightEmitPdfW, &lightDirectPdfW, &cosThetaAtLight);
	lightRay.UpdateMinMaxWithEpsilon();
	lightRay.time = time;
	if (!lightVertex.throughput.Black()) {
		lightEmitPdfW *= lightPickPdf;
		lightDirectPdfW *= lightPickPdf;

		lightVertex.throughput /= lightEmitPdfW;
		assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

		// I don't store the light vertex 0 because direct lighting will take
		// care of these kind of paths
		lightVertex.dVCM = MIS(lightDirectPdfW / lightEmitPdfW);
		// If the light source is not intersectable, it can not be
		// sampled with BSDF
		if (light->IsEnvironmental() || light->IsIntersectable()) {
			const float usedCosLight = light->IsEnvironmental() ? 1.f : cosThetaAtLight;
			lightVertex.dVC = MIS(usedCosLight / lightEmitPdfW);
		} else
			lightVertex.dVC = 0.f;
		lightVertex.dVM = lightVertex.dVC * misVcWeightFactor;

		lightVertex.depth = 1;
		while (lightVertex.depth <= engine->maxLightPathDepth) {
			const u_int sampleOffset = sampleBootSize + (lightVertex.depth - 1) * sampleLightStepSize;

			RayHit nextEventRayHit;
			Spectrum connectionThroughput;
			const bool hit = scene->Intersect(device, true, &lightVertex.volInfo, sampler->GetSample(sampleOffset),
					&lightRay, &nextEventRayHit, &lightVertex.bsdf,
					&connectionThroughput);

			if (hit) {
				// Something was hit

				// Update the new light vertex
				lightVertex.throughput *= connectionThroughput;
		
				// Infinite lights use MIS based on solid angle instead of area
				if((lightVertex.depth > 1) || !light->IsEnvironmental())
					lightVertex.dVCM *= MIS(nextEventRayHit.t * nextEventRayHit.t);
				const float factor = 1.f / MIS(AbsDot(lightVertex.bsdf.hitPoint.shadeN, lightRay.d));
				lightVertex.dVCM *= factor;
				lightVertex.dVC *= factor;
				lightVertex.dVM *= factor;

				// Store the vertex only if it isn't specular
				if (!lightVertex.bsdf.IsDelta()) {
					lightPathVertices.push_back(lightVertex);

					//----------------------------------------------------------
					// Try to connect the light path vertex with the eye
					//----------------------------------------------------------

					ConnectToEye(time, lightVertex, sampler->GetSample(sampleOffset + 1),
							lensPoint, sampleResults);
				}

				if (lightVertex.depth >= engine->maxLightPathDepth)
					break;

				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				if (!Bounce(time, sampler, sampleOffset + 2, &lightVertex, &lightRay))
					break;
			} else {
				// Ray lost in space...
				break;
			}
		}
	}
}

bool BiDirCPURenderThread::Bounce(const float time, Sampler *sampler,
		const u_int sampleOffset, PathVertexVM *pathVertex, Ray *nextEventRay) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;

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

	if (pathVertex->depth >= engine->rrDepth) {
		// Russian Roulette
		const float prob = RenderEngine::RussianRouletteProb(bsdfSample, engine->rrImportanceCap);
		if (prob < sampler->GetSample(sampleOffset + 2))
			return false;

		pathVertex->throughput /= prob;
	}

	// Was:
	//  pathVertex->throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
	//
	// but now BSDF::Sample() follows LuxRender habit to return the
	// result multiplied by cosSampledDir / bsdfPdfW
	pathVertex->throughput *= bsdfSample;
	assert (!pathVertex->throughput.IsNaN() && !pathVertex->throughput.IsInf());

	// New MIS weights
	if (event & SPECULAR) {
		pathVertex->dVCM = 0.f;
		// Was:
		//  const float factor = MIS(cosSampledDir / bsdfPdfW) * MIS(bsdfRevPdfW);
		//
		// but bsdfPdfW = bsdfRevPdfW for specular material.
		assert (bsdfPdfW == bsdfRevPdfW);
		const float factor = MIS(cosSampledDir);
		pathVertex->dVC *= factor;
		pathVertex->dVM *= factor;
	} else {
		pathVertex->dVC = MIS(cosSampledDir / bsdfPdfW) * (pathVertex->dVC *
				MIS(bsdfRevPdfW) + pathVertex->dVCM + misVmWeightFactor);
		pathVertex->dVM = MIS(cosSampledDir / bsdfPdfW) * (pathVertex->dVM *
				MIS(bsdfRevPdfW) + pathVertex->dVCM * misVcWeightFactor + 1.f);
		pathVertex->dVCM = MIS(1.f / bsdfPdfW);
	}

	// Update volume information
	pathVertex->volInfo.Update(event, pathVertex->bsdf);

	*nextEventRay = Ray(pathVertex->bsdf.hitPoint.p, sampledDir);
	nextEventRay->UpdateMinMaxWithEpsilon();
	nextEventRay->time = time;

	++pathVertex->depth;

	return true;
}

void BiDirCPURenderThread::RenderFunc() {
	//SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;
	Film *film = threadFilm;

	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film, engine->sampleSplatter,
			engine->samplerSharedData);
	const u_int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		engine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		engine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex
	sampler->RequestSamples(sampleSize);

	// Disable vertex merging
	misVmWeightFactor = 0.f;
	misVcWeightFactor = 0.f;

	vector<SampleResult> sampleResults;
	vector<PathVertexVM> lightPathVertices;
	// I can not use engine->renderConfig->GetProperty() here because the
	// RenderConfig properties cache is not thread safe
	const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>() *
		film->GetWidth() * film->GetHeight();

	for(u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		sampleResults.clear();
		lightPathVertices.clear();

		const float time = sampler->GetSample(12);

		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera->SampleLens(time, sampler->GetSample(3), sampler->GetSample(4),
				&lensPoint)) {
			sampler->NextSample(sampleResults);
			continue;
		}

		//----------------------------------------------------------------------
		// Trace light path
		//----------------------------------------------------------------------

		TraceLightPath(time, sampler, lensPoint, lightPathVertices, sampleResults);

		//----------------------------------------------------------------------
		// Trace eye path
		//----------------------------------------------------------------------

		PathVertexVM eyeVertex;
		SampleResult &eyeSampleResult = AddResult(sampleResults, false);

		film->GetSampleXY(sampler->GetSample(0), sampler->GetSample(1),
				&eyeSampleResult.filmX, &eyeSampleResult.filmY);
		Ray eyeRay;
		camera->GenerateRay(eyeSampleResult.filmX, eyeSampleResult.filmY, &eyeRay,
			sampler->GetSample(10), sampler->GetSample(11), time);

		eyeVertex.bsdf.hitPoint.fixedDir = -eyeRay.d;
		eyeVertex.throughput = Spectrum(1.f);
		const float cosAtCamera = Dot(scene->camera->GetDir(), eyeRay.d);
		const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera *
			scene->camera->GetPixelArea());
		eyeVertex.dVCM = MIS(1.f / cameraPdfW);
		eyeVertex.dVC = 0.f;
		eyeVertex.dVM = 0.f;

		eyeVertex.depth = 1;
		while (eyeVertex.depth <= engine->maxEyePathDepth) {
			eyeSampleResult.firstPathVertex = (eyeVertex.depth == 1);
			eyeSampleResult.lastPathVertex = (eyeVertex.depth == engine->maxEyePathDepth);

			const u_int sampleOffset = sampleBootSize + engine->maxLightPathDepth * sampleLightStepSize +
				(eyeVertex.depth - 1) * sampleEyeStepSize;

			// NOTE: I account for volume emission only with path tracing (i.e. here and
			// not in any other place)
			RayHit eyeRayHit;
			Spectrum connectionThroughput, connectEmission;
			const bool hit = scene->Intersect(device, false,
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
			if (eyeVertex.bsdf.IsLightSource()) {
				DirectHitLight(true, eyeVertex, eyeSampleResult);

				// SLG light sources are like black bodies
				break;
			}

			// Note: pass-through check is done inside Scene::Intersect()

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			DirectLightSampling(time,
					sampler->GetSample(sampleOffset + 1),
					sampler->GetSample(sampleOffset + 2),
					sampler->GetSample(sampleOffset + 3),
					sampler->GetSample(sampleOffset + 4),
					sampler->GetSample(sampleOffset + 5),
					eyeVertex, eyeSampleResult);

			//------------------------------------------------------------------
			// Connect vertex path ray with all light path vertices
			//------------------------------------------------------------------

			if (!eyeVertex.bsdf.IsDelta()) {
				for (vector<PathVertexVM>::const_iterator lightPathVertex = lightPathVertices.begin();
						lightPathVertex < lightPathVertices.end(); ++lightPathVertex)
					ConnectVertices(time, eyeVertex, *lightPathVertex, eyeSampleResult,
							sampler->GetSample(sampleOffset + 6));
			}

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			if (!Bounce(time, sampler, sampleOffset + 7, &eyeVertex, &eyeRay))
				break;

#ifdef WIN32
			// Work around Windows bad scheduling
			renderThread->yield();
#endif
		}

		sampler->NextSample(sampleResults);

		if ((haltDebug > 0u) && (steps >= haltDebug))
			break;
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] Rendering thread halted");
}
