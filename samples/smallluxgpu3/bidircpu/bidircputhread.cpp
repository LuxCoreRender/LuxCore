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
#include "bidircpu/bidircpu.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sampler/sampler.h"

//------------------------------------------------------------------------------
// BiDirCPU RenderThread
//------------------------------------------------------------------------------

//static const u_int BiDirCPURenderThread::sampleBootSize = 11;
//static const u_int BiDirCPURenderThread::sampleLightStepSize = 6;
//static const u_int BiDirCPURenderThread::sampleEyeStepSize = 11;

BiDirCPURenderThread::BiDirCPURenderThread(BiDirCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device, const u_int seedVal) :
		CPURenderThread(engine, index, device, seedVal, true, true), samplesCount(0.0) {
}

void BiDirCPURenderThread::ConnectVertices(
		const PathVertexVM &eyeVertex, const PathVertexVM &lightVertex,
		SampleResult *eyeSampleResult, const float u0) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

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
			if (!scene->Intersect(device, true, true, u0, &eyeRay, &eyeRayHit, &bsdfConn, &connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				if (eyeVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = Max(eyeBsdfEval.Filter(), engine->rrImportanceCap);
					eyeBsdfPdfW *= prob;
					eyeBsdfRevPdfW *= prob;
				}

				if (lightVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = Max(lightBsdfEval.Filter(), engine->rrImportanceCap);
					lightBsdfPdfW *= prob;
					lightBsdfRevPdfW *= prob;
				}

				// Convert pdfs to area pdf
				const float eyeBsdfPdfA = PdfWtoA(eyeBsdfPdfW, eyeDistance, cosThetaAtLight);
				const float lightBsdfPdfA  = PdfWtoA(lightBsdfPdfW,  eyeDistance, cosThetaAtCamera);

				// MIS weights
				const float lightWeight = eyeBsdfPdfA * (lightVertex.dVCM + lightVertex.dVC * MIS(lightBsdfRevPdfW));
				const float eyeWeight = lightBsdfPdfA * (eyeVertex.dVCM + eyeVertex.dVC * MIS(eyeBsdfRevPdfW));

				const float misWeight = 1.f / (lightWeight + 1.f + eyeWeight);

				eyeSampleResult->radiance += (misWeight * geometryTerm) * eyeVertex.throughput * eyeBsdfEval *
						connectionThroughput * lightBsdfEval * lightVertex.throughput;
			}
		}
	}
}

void BiDirCPURenderThread::ConnectToEye(const PathVertexVM &lightVertex, const float u0,
		const Point &lensPoint, vector<SampleResult> &sampleResults) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

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
			if (!scene->Intersect(device, true, true, u0, &eyeRay, &eyeRayHit, &bsdfConn, &connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				if (lightVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfEval.Filter(), engine->rrImportanceCap);
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
				const float weightLight = MIS(cameraPdfA / pixelCount) *
					(lightVertex.dVCM + lightVertex.dVC * MIS(bsdfRevPdfW));
				const float misWeight = 1.f / (weightLight + 1.f);

				const Spectrum radiance = misWeight * connectionThroughput * lightVertex.throughput * fluxToRadianceFactor * bsdfEval;

				AddSampleResult(sampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
						radiance, 1.f);
			}
		}
	}
}

void BiDirCPURenderThread::DirectLightSampling(
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertexVM &eyeVertex,
		Spectrum *radiance) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	
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
				if (!scene->Intersect(device, false, false, u4, &shadowRay, &shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					if (eyeVertex.depth >= engine->rrDepth) {
						// Russian Roulette
						const float prob = Max(bsdfEval.Filter(), engine->rrImportanceCap);
						bsdfPdfW *= prob;
						bsdfRevPdfW *= prob;
					}

					const float cosThetaToLight = AbsDot(lightRayDir, eyeVertex.bsdf.shadeN);
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;

					// emissionPdfA / directPdfA = emissionPdfW / directPdfW
					const float weightLight = MIS(bsdfPdfW / directLightSamplingPdfW);
					const float weightCamera = MIS(emissionPdfW * cosThetaToLight / (directPdfW * cosThetaAtLight)) *
						(eyeVertex.dVCM + eyeVertex.dVC * MIS(bsdfRevPdfW));
					const float misWeight = 1.f / (weightLight + 1.f + weightCamera);

					const float factor = cosThetaToLight / directLightSamplingPdfW;

					*radiance += (misWeight * factor) * eyeVertex.throughput * connectionThroughput * lightRadiance * bsdfEval;
				}
			}
		}
	}
}

void BiDirCPURenderThread::DirectHitLight(const bool finiteLightSource,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	float directPdfA, emissionPdfW;
	const Spectrum lightRadiance = (finiteLightSource) ?
		eyeVertex.bsdf.GetEmittedRadiance(scene, &directPdfA, &emissionPdfW) :
		scene->GetEnvLightsRadiance(eyeVertex.bsdf.fixedDir, Point(), &directPdfA, &emissionPdfW);
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
	const float weightCamera = MIS(directPdfA) * eyeVertex.dVCM +
		MIS(emissionPdfW) * eyeVertex.dVC;
	const float misWeight = 1.f / (weightCamera + 1.f);

	*radiance += misWeight * eyeVertex.throughput * lightRadiance;
}

void BiDirCPURenderThread::TraceLightPath(Sampler *sampler,
		const Point &lensPoint,
		vector<PathVertexVM> &lightPathVertices,
		vector<SampleResult> &sampleResults) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->SampleAllLights(sampler->GetSample(2), &lightPickPdf);

	// Initialize the light path
	PathVertexVM lightVertex;
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray lightRay;
	lightVertex.throughput = light->Emit(scene,
		sampler->GetSample(5), sampler->GetSample(6), sampler->GetSample(7), sampler->GetSample(8),
		&lightRay.o, &lightRay.d, &lightEmitPdfW, &lightDirectPdfW, &cosThetaAtLight);
	if (!lightVertex.throughput.Black()) {
		lightEmitPdfW *= lightPickPdf;
		lightDirectPdfW *= lightPickPdf;

		lightVertex.throughput /= lightEmitPdfW;
		assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

		// I don't store the light vertex 0 because direct lighting will take
		// care of this kind of paths
		lightVertex.dVCM = MIS(lightDirectPdfW / lightEmitPdfW);
		const float usedCosLight = light->IsEnvironmental() ? 1.f : cosThetaAtLight;
		lightVertex.dVC = MIS(usedCosLight / lightEmitPdfW);
		lightVertex.dVM = lightVertex.dVC * misVcWeightFactor;

		lightVertex.depth = 1;
		while (lightVertex.depth <= engine->maxLightPathDepth) {
			const unsigned int sampleOffset = sampleBootSize + (lightVertex.depth - 1) * sampleLightStepSize;

			RayHit nextEventRayHit;
			Spectrum connectionThroughput;
			if (scene->Intersect(device, true, true, sampler->GetSample(sampleOffset),
					&lightRay, &nextEventRayHit, &lightVertex.bsdf, &connectionThroughput)) {
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
					lightVertex.dVCM *= MIS(nextEventRayHit.t * nextEventRayHit.t);
				const float factor = 1.f / MIS(AbsDot(lightVertex.bsdf.shadeN, lightRay.d));
				lightVertex.dVCM *= factor;
				lightVertex.dVC *= factor;
				lightVertex.dVM *= factor;

				// Store the vertex only if it isn't specular
				if (!lightVertex.bsdf.IsDelta()) {
					lightPathVertices.push_back(lightVertex);

					//------------------------------------------------------
					// Try to connect the light path vertex with the eye
					//------------------------------------------------------

					ConnectToEye(lightVertex, sampler->GetSample(sampleOffset + 1),
							lensPoint, sampleResults);
				}

				if (lightVertex.depth >= engine->maxLightPathDepth)
					break;

				//----------------------------------------------------------
				// Build the next vertex path ray
				//----------------------------------------------------------

				if (!Bounce(sampler, sampleOffset + 2, &lightVertex, &lightRay))
					break;

				++(lightVertex.depth);
			} else {
				// Ray lost in space...
				break;
			}
		}
	}
}

bool BiDirCPURenderThread::Bounce(Sampler *sampler, const u_int sampleOffset,
		PathVertexVM *pathVertex, Ray *nextEventRay) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;

	Vector sampledDir;
	BSDFEvent event;
	float bsdfPdfW, cosSampledDir;
	const Spectrum bsdfSample = pathVertex->bsdf.Sample(&sampledDir,
			sampler->GetSample(sampleOffset),
			sampler->GetSample(sampleOffset + 1),
			sampler->GetSample(sampleOffset + 2),
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
		const float prob = Max(bsdfSample.Filter(), engine->rrImportanceCap);
		if (sampler->GetSample(sampleOffset + 3) < prob) {
			bsdfPdfW *= prob;
			bsdfRevPdfW *= prob;
		} else
			return false;
	}

	pathVertex->throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
	assert (!pathVertex->throughput.IsNaN() && !pathVertex->throughput.IsInf());

	// New MIS weights
	if (event & SPECULAR) {
		pathVertex->dVCM = 0.f;
		const float factor = MIS(cosSampledDir / bsdfPdfW) * MIS(bsdfRevPdfW);
		pathVertex->dVC *= factor;
		pathVertex->dVM *= factor;
	} else {
		pathVertex->dVC = MIS(cosSampledDir / bsdfPdfW) * (pathVertex->dVC *
				MIS(bsdfRevPdfW) + pathVertex->dVCM + misVmWeightFactor);
		pathVertex->dVM = MIS(cosSampledDir / bsdfPdfW) * (pathVertex->dVM *
				MIS(bsdfRevPdfW) + pathVertex->dVCM * misVcWeightFactor + 1.f);
		pathVertex->dVCM = MIS(1.f / bsdfPdfW);
	}

	*nextEventRay = Ray(pathVertex->bsdf.hitPoint, sampledDir);

	return true;
}

void BiDirCPURenderThread::RenderFunc() {
	//SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(seed);
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();
	pixelCount = filmWidth * filmHeight;
	samplesCount = 0.0;

	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film);
	const unsigned int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		engine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		engine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex
	sampler->RequestSamples(sampleSize);

	// Disable vertex merging
	misVmWeightFactor = 0.f;
	misVcWeightFactor = 0.f;

	vector<SampleResult> sampleResults;
	vector<PathVertexVM> lightPathVertices;
	while (!boost::this_thread::interruption_requested()) {
		samplesCount += 1.0;
		sampleResults.clear();
		lightPathVertices.clear();

		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera->SampleLens(sampler->GetSample(3), sampler->GetSample(4),
				&lensPoint)) {
			sampler->NextSample(sampleResults);
			continue;
		}

		//----------------------------------------------------------------------
		// Trace light path
		//----------------------------------------------------------------------

		TraceLightPath(sampler, lensPoint, lightPathVertices, sampleResults);
		
		//----------------------------------------------------------------------
		// Trace eye path
		//----------------------------------------------------------------------

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

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			DirectLightSampling(sampler->GetSample(sampleOffset + 1),
					sampler->GetSample(sampleOffset + 2),
					sampler->GetSample(sampleOffset + 3),
					sampler->GetSample(sampleOffset + 4),
					sampler->GetSample(sampleOffset + 5),
					eyeVertex, &eyeSampleResult.radiance);

			//------------------------------------------------------------------
			// Connect vertex path ray with all light path vertices
			//------------------------------------------------------------------

			if (!eyeVertex.bsdf.IsDelta()) {
				for (vector<PathVertexVM>::const_iterator lightPathVertex = lightPathVertices.begin();
						lightPathVertex < lightPathVertices.end(); ++lightPathVertex)
					ConnectVertices(eyeVertex, *lightPathVertex, &eyeSampleResult,
							sampler->GetSample(sampleOffset + 6));
			}

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			if (!Bounce(sampler, sampleOffset + 7, &eyeVertex, &eyeRay))
				break;

			++(eyeVertex.depth);
		}

		sampleResults.push_back(eyeSampleResult);

		sampler->NextSample(sampleResults);
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[BiDirCPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
}
