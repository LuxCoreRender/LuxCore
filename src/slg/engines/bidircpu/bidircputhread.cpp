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
// (http://www.davidovic.cz and http://www.smallvcm.com)

#include <boost/format.hpp>
#include <boost/function.hpp>

#include "luxrays/utils/thread.h"

#include "slg/engines/bidircpu/bidircpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirCPU RenderThread
//------------------------------------------------------------------------------

const Film::FilmChannels BiDirCPURenderThread::eyeSampleResultsChannels({
	Film::RADIANCE_PER_PIXEL_NORMALIZED, Film::ALPHA, Film::DEPTH,
	Film::POSITION, Film::GEOMETRY_NORMAL, Film::SHADING_NORMAL, Film::MATERIAL_ID,
	Film::UV, Film::OBJECT_ID, Film::SAMPLECOUNT, Film::CONVERGENCE,
	Film::MATERIAL_ID_COLOR, Film::ALBEDO, Film::AVG_SHADING_NORMAL, Film::NOISE
});

const Film::FilmChannels BiDirCPURenderThread::lightSampleResultsChannels({
	Film::RADIANCE_PER_SCREEN_NORMALIZED
}); 

BiDirCPURenderThread::BiDirCPURenderThread(BiDirCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device) {
}

void BiDirCPURenderThread::AOVWarmUp(RandomGenerator *rndGen) {
	if (threadIndex == 0)
		SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] AOV warmup started");
	
	const double start = WallClockTime();
	double lastProgressPrint = start;
		
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;

	SobolSampler sampler(rndGen, engine->film, engine->sampleSplatter, true, 0.f, 0.f,
		16, 16, 1, 1,
		engine->aovWarmupSamplerSharedData);

	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 3;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		engine->maxEyePathDepth * sampleStepSize; // For each path vertex
	sampler.RequestSamples(ONLY_AOV_SAMPLE, sampleSize);

	// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	const Film::FilmChannels sampleResultsChannels({
		Film::ALBEDO, Film::AVG_SHADING_NORMAL
	});

	sampleResult.Init(&sampleResultsChannels, engine->film->GetRadianceGroupCount());

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = engine->maxEyePathDepth;
	maxPathDepthInfo.diffuseDepth = engine->maxEyePathDepth;
	maxPathDepthInfo.glossyDepth = engine->maxEyePathDepth;
	maxPathDepthInfo.specularDepth = engine->maxEyePathDepth;

	while (sampler.GetPassCount() < engine->aovWarmupSPP) {
		if (boost::this_thread::interruption_requested())
			return;

		sampleResult.filmX = sampler.GetSample(0);
		sampleResult.filmY = sampler.GetSample(1);

		const float timeSample = sampler.GetSample(4);
		const float time = scene->camera->GenerateRayTime(timeSample);

		Ray eyeRay;
		PathVolumeInfo volInfo;
		camera->GenerateRay(time,
				sampleResult.filmX, sampleResult.filmY, &eyeRay,
				&volInfo, sampler.GetSample(2), sampler.GetSample(3));

		sampleResult.albedo = Spectrum(); // Just in case albedoToDo is never true
		sampleResult.shadingNormal = Normal();

		Spectrum pathThroughput(1.f);
		u_int depth = 0;
		BSDF bsdf;
		while (depth < engine->maxEyePathDepth) {
			sampleResult.firstPathVertex = (depth == 0);

			const u_int sampleOffset = sampleBootSize + depth * sampleStepSize;

			// NOTE: I account for volume emission only with path tracing (i.e. here and
			// not in any other place)
			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			const bool hit = scene->Intersect(device,
					EYE_RAY | (sampleResult.firstPathVertex ? CAMERA_RAY : INDIRECT_RAY),
					&volInfo, sampler.GetSample(sampleOffset),
					&eyeRay, &eyeRayHit, &bsdf,
					&connectionThroughput, &pathThroughput, &sampleResult);
			pathThroughput *= connectionThroughput;

			if (!hit) {
				// Nothing was hit
				break;
			}

			//------------------------------------------------------------------
			// Something was hit
			//------------------------------------------------------------------

			if (bsdf.IsAlbedoEndPoint(engine->albedoSpecularSetting,
					engine->albedoSpecularGlossinessThreshold)) {
				sampleResult.albedo = pathThroughput * bsdf.Albedo();
				sampleResult.shadingNormal = bsdf.hitPoint.shadeN;
				break;
			}

			// Check if I reached the max. depth
			if (depth == engine->maxEyePathDepth)
				break;

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			Vector sampledDir;
			float cosSampledDir, lastPdfW;
			BSDFEvent lastBSDFEvent;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler.GetSample(sampleOffset + 1),
						sampler.GetSample(sampleOffset + 2),
						&lastPdfW, &cosSampledDir, &lastBSDFEvent);

			assert (!bsdfSample.IsNaN() && !bsdfSample.IsInf());
			if (bsdfSample.Black())
				break;
			assert (!isnan(lastPdfW) && !isinf(lastPdfW));

			pathThroughput *= bsdfSample;
			assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

			// Update volume information
			volInfo.Update(lastBSDFEvent, bsdf);

			eyeRay.Update(bsdf.GetRayOrigin(sampledDir), sampledDir);
		}

		sampler.NextSample(sampleResults);

		if (threadIndex == 0) {
			const double end = WallClockTime();
			const double delta = end - lastProgressPrint;

			if (delta > 2.0) {
				const u_int *filmSubRegion = engine->film->GetSubRegion();
				const u_int subRegionWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
				const u_int subRegionHeight = filmSubRegion[3] - filmSubRegion[2] + 1;

				const double samplesSec = sampler.GetPassCount() * (double)(subRegionWidth * subRegionHeight) / (1000000.0 * (end -start));

				SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] AOV warmup progress: " <<
						sampler.GetPassCount() << "/" << engine->aovWarmupSPP << " pass"
						" (" << boost::str(boost::format("%3.2fM") % samplesSec) << " samples/sec)");
				
				lastProgressPrint = WallClockTime();
			}
		}
#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif
	}
	
	if (threadIndex == 0) {
		const double end = WallClockTime();
		
		const u_int *filmSubRegion = engine->film->GetSubRegion();
		const u_int subRegionWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
		const u_int subRegionHeight = filmSubRegion[3] - filmSubRegion[2] + 1;
		
		const double samplesSec = engine->aovWarmupSPP * (double)(subRegionWidth * subRegionHeight) / (1000000.0 * (end -start));
		
		SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] AOV warmup done: " <<
				boost::str(boost::format("%3.2fM") % samplesSec) << " samples/sec");
	}
}

SampleResult &BiDirCPURenderThread::AddResult(vector<SampleResult> &sampleResults, const bool fromLight) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	SampleResult &sampleResult = sampleResults[size];

	sampleResult.Init(
			fromLight ? &lightSampleResultsChannels : &eyeSampleResultsChannels,
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
			const Point shadowRayOrig = eyeVertex.bsdf.GetRayOrigin(p2pDir);
			const Vector shadowRayOrigP2P(lightVertex.bsdf.hitPoint.p - shadowRayOrig);
			const float shadowRayDistanceSquared = shadowRayOrigP2P.LengthSquared();
			const float shadowRayDistance = sqrtf(shadowRayDistanceSquared);
			const Vector shadowRayDir = shadowRayOrigP2P / shadowRayDistance;

			Ray p2pRay(shadowRayOrig, shadowRayDir,
					0.f,
					shadowRayDistance,
					time);
			p2pRay.UpdateMinMaxWithEpsilon();

			RayHit p2pRayHit;
			BSDF bsdfConn;
			Spectrum connectionThroughput;
			PathVolumeInfo volInfo = eyeVertex.volInfo; // I need to use a copy here
			if (!scene->Intersect(device, LIGHT_RAY | INDIRECT_RAY | SHADOW_RAY, &volInfo, u0, &p2pRay, &p2pRayHit, &bsdfConn,
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
				const float lightBsdfPdfA = PdfWtoA(lightBsdfPdfW,  p2pDistance, cosThetaAtCamera);

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
	// I don't connect camera invisible objects with the eye
	if (lightVertex.bsdf.IsCameraInvisible())
		return;

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
		scene->camera->ClampRay(&eyeRay);
		eyeRay.UpdateMinMaxWithEpsilon();

		float filmX, filmY;
		if (scene->camera->GetSamplePosition(&eyeRay, &filmX, &filmY)) {
			// I have to flip the direction of the traced ray because
			// the information inside PathVolumeInfo are about the path from
			// the light toward the camera (i.e. ray.o would be in the wrong
			// place).
			Ray traceRay(lightVertex.bsdf.GetRayOrigin(-eyeRay.d), -eyeRay.d,
					eyeDistance - eyeRay.maxt,
					eyeDistance - eyeRay.mint,
					time);
			RayHit traceRayHit;

			BSDF bsdfConn;
			Spectrum connectionThroughput;
			PathVolumeInfo volInfo = lightVertex.volInfo; // I need to use a copy here
			if (!scene->Intersect(device, LIGHT_RAY | CAMERA_RAY, &volInfo, u0, &traceRay, &traceRayHit, &bsdfConn,
					&connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				if (lightVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = RenderEngine::RussianRouletteProb(bsdfEval, engine->rrImportanceCap);
					bsdfRevPdfW *= prob;
				}

				const float cosToCamera = Dot(lightVertex.bsdf.hitPoint.shadeN, -eyeDir);
				float cameraPdfW, fluxToRadianceFactor;
				scene->camera->GetPDF(eyeRay, eyeDistance, filmX, filmY, &cameraPdfW, &fluxToRadianceFactor);
				const float cameraPdfA = PdfWtoA(cameraPdfW, eyeDistance, cosToCamera);
				// Was:
				//  const float fluxToRadianceFactor = cameraPdfA;	
				//	
				// but now BSDF::Evaluate() follows LuxRender habit to return the	
				// result multiplied by cosThetaToLight
				//
				// However this is not true for volumes (see bug
				// report http://forums.luxcorerender.org/viewtopic.php?f=4&t=1146&start=10#p13491)
				fluxToRadianceFactor *= lightVertex.bsdf.IsVolume() ? fabsf(cosToCamera) : 1.f;

				const float weightLight = MIS(cameraPdfA) *
					(misVmWeightFactor + lightVertex.dVCM + lightVertex.dVC * MIS(bsdfRevPdfW));
				const float misWeight = 1.f / (weightLight + 1.f);

				const Spectrum radiance = (misWeight * fluxToRadianceFactor) *
					connectionThroughput * lightVertex.throughput * bsdfEval;

				SampleResult &sampleResult = AddResult(sampleResults, true);
				sampleResult.filmX = filmX;
				sampleResult.filmY = filmY;
				
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
		const Normal landingNormal = eyeVertex.bsdf.hitPoint.intoObject ? eyeVertex.bsdf.hitPoint.geometryN : -eyeVertex.bsdf.hitPoint.geometryN;
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->SampleLights(u0,
				eyeVertex.bsdf.hitPoint.p,
				landingNormal,
				eyeVertex.bsdf.IsVolume(),
				&lightPickPdf);

		if (light) {
			Ray shadowRay;
			float directPdfW, emissionPdfW, cosThetaAtLight;
			const Spectrum lightRadiance = light->Illuminate(*scene, eyeVertex.bsdf,
					time, u1, u2, u3, shadowRay, directPdfW, &emissionPdfW,
					&cosThetaAtLight);

			if (!lightRadiance.Black()) {
				BSDFEvent event;
				float bsdfPdfW, bsdfRevPdfW;
				const Spectrum bsdfEval = eyeVertex.bsdf.Evaluate(shadowRay.d, &event, &bsdfPdfW, &bsdfRevPdfW);

				if (!bsdfEval.Black()) {
					RayHit shadowRayHit;
					BSDF shadowBsdf;
					Spectrum connectionThroughput;
					PathVolumeInfo volInfo = eyeVertex.volInfo; // I need to use a copy here
					// Check if the light source is visible
					if (!scene->Intersect(device, EYE_RAY | SHADOW_RAY, &volInfo, u4,
							&shadowRay, &shadowRayHit, &shadowBsdf, &connectionThroughput)) {
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

						const float cosThetaToLight = AbsDot(shadowRay.d, eyeVertex.bsdf.hitPoint.shadeN);
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

						assert (eyeSampleResult.IsValid());
					}
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

	const float lightPickPdf = scene->lightDefs.GetEmitLightStrategy()->SampleLightPdf(light,
			eyeVertex.bsdf.hitPoint.p, eyeVertex.bsdf.hitPoint.geometryN,
			eyeVertex.bsdf.IsVolume());

	// MIS weight
	const float weightCamera = MIS(directPdfA * lightPickPdf) * eyeVertex.dVCM +
		MIS(emissionPdfW * lightPickPdf) * eyeVertex.dVC;
	const float misWeight = 1.f / (weightCamera + 1.f);

	*radiance += misWeight * eyeVertex.throughput * lightRadiance;
	
	assert (radiance->IsValid());
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
			const Spectrum lightRadiance = el->GetRadiance(*scene,
					(eyeVertex.depth == 1) ? nullptr : &eyeVertex.bsdf,
					eyeVertex.bsdf.hitPoint.fixedDir, &directPdfA, &emissionPdfW);

			DirectHitLight(el, lightRadiance, directPdfA, emissionPdfW,
					eyeVertex, &eyeSampleResult.radiance[el->GetID()]);
		}
	}
}

bool BiDirCPURenderThread::TraceLightPath(const float time,
		Sampler *sampler, const Point &lensPoint,
		vector<PathVertexVM> &lightPathVertices,
		vector<SampleResult> &sampleResults) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Select one light source
	// BiDir can use only a single strategy, emit in this case
	float lightPickPdf;
	const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->
			SampleLights(sampler->GetSample(2), &lightPickPdf);
	if (!light)
		return false;

	// Initialize the light path
	PathVertexVM lightVertex;
	lightVertex.lightID = light->GetID();
	
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray lightRay;
	lightVertex.throughput = light->Emit(*scene,
			time, sampler->GetSample(5), sampler->GetSample(6),
			sampler->GetSample(7), sampler->GetSample(8), sampler->GetSample(9),
			lightRay, lightEmitPdfW,
			&lightDirectPdfW, &cosThetaAtLight);
	if (!lightVertex.throughput.Black()) {
		lightVertex.volInfo.AddVolume(light->volume);

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
			const bool hit = scene->Intersect(device, LIGHT_RAY | INDIRECT_RAY,
					&lightVertex.volInfo, sampler->GetSample(sampleOffset),
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
	
	return true;
}

bool BiDirCPURenderThread::Bounce(const float time, Sampler *sampler,
		const u_int sampleOffset, PathVertexVM *pathVertex, Ray *nextEventRay) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;

	Vector sampledDir;
	BSDFEvent &event = pathVertex->bsdfEvent;
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

	*nextEventRay = Ray(pathVertex->bsdf.GetRayOrigin(sampledDir), sampledDir);
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

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen

	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	Camera *camera = scene->camera;
	PhotonGICache *photonGICache = engine->photonGICache;

	// Albedo and Normal AOV warm up
	if (engine->aovWarmupSPP > 0)
		AOVWarmUp(rndGen);
	
	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, engine->film, engine->sampleSplatter,
			engine->samplerSharedData, Properties());
	const u_int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		engine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		engine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex
	sampler->SetThreadIndex(threadIndex);
	sampler->RequestSamples(PIXEL_NORMALIZED_AND_SCREEN_NORMALIZED, sampleSize);

	VarianceClamping varianceClamping(engine->sqrtVarianceClampMaxValue);

	// Disable vertex merging
	misVmWeightFactor = 0.f;
	misVcWeightFactor = 0.f;

	vector<SampleResult> sampleResults;
	vector<PathVertexVM> lightPathVertices;

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
		lightPathVertices.clear();

		const float timeSample = sampler->GetSample(12);
		const float time = scene->camera->GenerateRayTime(timeSample);

		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera->SampleLens(time, sampler->GetSample(3), sampler->GetSample(4),
				&lensPoint)) {
			assert (SampleResult::IsAllValid(sampleResults));

			sampler->NextSample(sampleResults);
			continue;
		}

		//----------------------------------------------------------------------
		// Trace light path
		//----------------------------------------------------------------------

		const bool validLightPath = TraceLightPath(time, sampler, lensPoint, lightPathVertices, sampleResults);
		assert (SampleResult::IsAllValid(sampleResults));

		if (validLightPath) {
			//------------------------------------------------------------------
			// Trace eye path
			//------------------------------------------------------------------

			PathVertexVM eyeVertex;
			SampleResult &eyeSampleResult = AddResult(sampleResults, false);

			eyeSampleResult.filmX = sampler->GetSample(0);
			eyeSampleResult.filmY = sampler->GetSample(1);
			Ray eyeRay;
			camera->GenerateRay(time,
					eyeSampleResult.filmX, eyeSampleResult.filmY, &eyeRay,
					&eyeVertex.volInfo, sampler->GetSample(10), sampler->GetSample(11));

			// Required by MIS weights update
			eyeVertex.bsdf.hitPoint.fixedDir = -eyeRay.d;
			eyeVertex.throughput = Spectrum(1.f);
			float cameraPdfW;
			scene->camera->GetPDF(eyeRay, 0.f, eyeSampleResult.filmX, eyeSampleResult.filmY, &cameraPdfW, nullptr);
			eyeVertex.dVCM = MIS(1.f / cameraPdfW);
			eyeVertex.dVC = 0.f;
			eyeVertex.dVM = 0.f;

			eyeVertex.depth = 1;
			bool albedoToDo = true;
			eyeSampleResult.albedo = Spectrum(); // Just in case albedoToDo is never true
			eyeSampleResult.shadingNormal = Normal();
			bool photonGICausticCacheUsed = false;
			bool isTransmittedEyePath = true;
			while (eyeVertex.depth <= engine->maxEyePathDepth) {
				eyeSampleResult.firstPathVertex = (eyeVertex.depth == 1);
				eyeSampleResult.lastPathVertex = (eyeVertex.depth == engine->maxEyePathDepth);

				const u_int sampleOffset = sampleBootSize + engine->maxLightPathDepth * sampleLightStepSize +
					(eyeVertex.depth - 1) * sampleEyeStepSize;

				// NOTE: I account for volume emission only with path tracing (i.e. here and
				// not in any other place)
				RayHit eyeRayHit;
				Spectrum connectionThroughput;
				const bool hit = scene->Intersect(device,
						EYE_RAY | (eyeSampleResult.firstPathVertex ? CAMERA_RAY : INDIRECT_RAY),
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
						eyeSampleResult.depth = numeric_limits<float>::infinity();
						eyeSampleResult.position = Point(
								numeric_limits<float>::infinity(),
								numeric_limits<float>::infinity(),
								numeric_limits<float>::infinity());
						eyeSampleResult.geometryNormal = Normal();
						eyeSampleResult.shadingNormal = Normal();
						eyeSampleResult.materialID = 0;
						eyeSampleResult.objectID = 0;
						eyeSampleResult.uv = UV(numeric_limits<float>::infinity(),
								numeric_limits<float>::infinity());
					} else if (isTransmittedEyePath) {
						// I set to 0.0 also the alpha all purely transmitted paths hitting nothing
						eyeSampleResult.alpha = 0.f;
					}
					break;
				}
				eyeVertex.throughput *= connectionThroughput;

				// Something was hit

				if (albedoToDo && eyeVertex.bsdf.IsAlbedoEndPoint(engine->albedoSpecularSetting,
						engine->albedoSpecularGlossinessThreshold)) {
					eyeSampleResult.albedo = eyeVertex.throughput * eyeVertex.bsdf.Albedo();
					eyeSampleResult.shadingNormal = eyeVertex.bsdf.hitPoint.shadeN;
					albedoToDo = false;
				}

				if (eyeSampleResult.firstPathVertex) {
					eyeSampleResult.alpha = 1.f;
					eyeSampleResult.depth = eyeRayHit.t;
					eyeSampleResult.position = eyeVertex.bsdf.hitPoint.p;
					eyeSampleResult.geometryNormal = eyeVertex.bsdf.hitPoint.geometryN;
					eyeSampleResult.materialID = eyeVertex.bsdf.GetMaterialID();
					eyeSampleResult.objectID = eyeVertex.bsdf.GetObjectID();
					eyeSampleResult.uv = eyeVertex.bsdf.hitPoint.GetUV(0);
				}

				// Update MIS constants
				const float factor = 1.f / MIS(AbsDot(eyeVertex.bsdf.hitPoint.shadeN, eyeVertex.bsdf.hitPoint.fixedDir));
				eyeVertex.dVCM *= MIS(eyeRayHit.t * eyeRayHit.t) * factor;
				eyeVertex.dVC *= factor;
				eyeVertex.dVM *= factor;

				// Check if it is a light source
				if (eyeVertex.bsdf.IsLightSource() &&
					// Avoid to render caustic path if PhotonGI caustic cache
					// has been used (for SDS paths)
					!photonGICausticCacheUsed){
					DirectHitLight(true, eyeVertex, eyeSampleResult);
				}

				// Note: pass-through check is done inside Scene::Intersect()
		
				//--------------------------------------------------------------
				// Check if I can use the photon cache
				//--------------------------------------------------------------

				if (photonGICache) {
					const bool isPhotonGIEnabled = photonGICache->IsPhotonGIEnabled(eyeVertex.bsdf);

					// Check if the cache is enabled for this material
					if (isPhotonGIEnabled) {
						// TODO: add support for AOVs (possible ?)

						if (photonGICache->IsCausticEnabled() && (eyeVertex.depth > 1)) {
							const SpectrumGroup causticRadiance = photonGICache->ConnectWithCausticPaths(eyeVertex.bsdf);

							if (!causticRadiance.Black())
								eyeSampleResult.radiance.AddWeighted(eyeVertex.throughput, causticRadiance);
							
							photonGICausticCacheUsed = true;
						}
					}
				}

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

				assert (eyeSampleResult.IsValid());

				//--------------------------------------------------------------
				// Connect vertex path ray with all light path vertices
				//--------------------------------------------------------------

				if (!eyeVertex.bsdf.IsDelta()) {
					for (vector<PathVertexVM>::const_iterator lightPathVertex = lightPathVertices.begin();
							lightPathVertex < lightPathVertices.end(); ++lightPathVertex)
						ConnectVertices(time, eyeVertex, *lightPathVertex, eyeSampleResult,
								sampler->GetSample(sampleOffset + 6));
					
					assert (eyeSampleResult.IsValid());
				}

				//--------------------------------------------------------------
				// Build the next vertex path ray
				//--------------------------------------------------------------

				if (!Bounce(time, sampler, sampleOffset + 7, &eyeVertex, &eyeRay))
					break;
				
				isTransmittedEyePath = isTransmittedEyePath && (eyeVertex.bsdfEvent & TRANSMIT);

#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}
		}
		
		assert (SampleResult::IsAllValid(sampleResults));

		// Variance clamping
		if (varianceClamping.hasClamping()) {
			for(u_int i = 0; i < sampleResults.size(); ++i)
				varianceClamping.Clamp(*(engine->film), sampleResults[i]);

			assert (SampleResult::IsAllValid(sampleResults));
		}

		sampler->NextSample(sampleResults);

		// Check halt conditions
		if (engine->film->GetConvergence() == 1.f)
			break;

		if (photonGICache) {
			try {
				const u_int spp = engine->film->GetTotalEyeSampleCount() / engine->film->GetPixelCount();
				photonGICache->Update(threadIndex, spp);
			} catch (boost::thread_interrupted &ti) {
				// I have been interrupted, I must stop
				break;
			}
		}
	}

	delete sampler;
	delete rndGen;

	threadDone = true;

	// This is done to interrupt thread pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	for (u_int i = 0; i < engine->renderThreads.size(); ++i) {
		try {
			engine->renderThreads[i]->Interrupt();
		} catch(...) {
			// Ignore any exception
		}
	}

	//SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] Rendering thread halted");
}
