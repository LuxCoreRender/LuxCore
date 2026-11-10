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

#include <thread>
#include <cassert>
#include <boost/format.hpp>

#include "luxrays/core/device.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/usings.h"
#include "luxrays/utils/thread.h"

#include "slg/engines/bidircpu/bidircpu.h"
#include "slg/cameras/camera.h"
#include "slg/lights/light.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace std::literals::chrono_literals;

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
		const u_int index, IntersectionDeviceRef device) :
		CPUNoTileRenderThread(engine, index, device) {
}

void BiDirCPURenderThread::AOVWarmUp(
	std::stop_token stop_token,
	const luxrays::RandomGeneratorUPtr & rndGen
) {
	if (threadIndex == 0)
		SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] AOV warmup started");

	const double start = WallClockTime();
	double lastProgressPrint = start;

	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	auto& scene = engine->renderConfig.GetScene();
	auto& camera = scene.GetCamera();

	SobolSampler sampler(rndGen, engine->GetFilm(), engine->GetSampleSplatter(), true, 0.f, 0.f,
		16, 16, 1, 1,
		engine->aovWarmupSamplerSharedData
	);

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

	sampleResult.Init(&sampleResultsChannels, engine->GetFilm().GetRadianceGroupCount());

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = engine->maxEyePathDepth;
	maxPathDepthInfo.diffuseDepth = engine->maxEyePathDepth;
	maxPathDepthInfo.glossyDepth = engine->maxEyePathDepth;
	maxPathDepthInfo.specularDepth = engine->maxEyePathDepth;

	while (sampler.GetPassCount() < engine->aovWarmupSPP) {
		if (stop_token.stop_requested())
			return;

		sampleResult.filmX = sampler.GetSample(0);
		sampleResult.filmY = sampler.GetSample(1);

		const float timeSample = sampler.GetSample(4);
		const float time = scene.GetCamera().GenerateRayTime(timeSample);

		Ray eyeRay;
		PathVolumeInfo volInfo;
		camera.GenerateRay(time,
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
			const bool hit = scene.Intersect(IntersectionDevicePtr(&device),
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
				const u_int *filmSubRegion = engine->GetFilm().GetSubRegion();
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
        std::this_thread::yield();
#endif
	}
	
	if (threadIndex == 0) {
		const double end = WallClockTime();
		
		const u_int *filmSubRegion = engine->GetFilm().GetSubRegion();
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
			engine->GetFilm().GetRadianceGroupCount());

	return sampleResult;
}

void BiDirCPURenderThread::ConnectVertices(const float time,
		const PathVertexVM &eyeVertex, const PathVertexVM &lightVertex,
		SampleResult &eyeSampleResult, const float u0) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	auto& scene = engine->renderConfig.GetScene();

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
			// For the connection event, we need to evaluate the volume based
			// on whether the shadow ray is going into the object or not
			bool connectionIntoObject = (Dot(lightVertex.bsdf.hitPoint.geometryN, -shadowRayDir) < 0.f);
			volInfo.SetCurrentVolume(connectionIntoObject ?
				lightVertex.bsdf.hitPoint.interiorVolume :
				lightVertex.bsdf.hitPoint.exteriorVolume
			);
			if (!scene.Intersect(
					luxrays::make_observer(device),
					LIGHT_RAY | INDIRECT_RAY | SHADOW_RAY,
					&volInfo, u0, &p2pRay, &p2pRayHit, &bsdfConn,
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
	auto& scene = engine->renderConfig.GetScene();

	Vector eyeDir;
	float eyeDistance = 0;
    if (scene.GetCamera().GetType() == Camera::ORTHOGRAPHIC){
		// Orthographic camera need to be handled separately,
		// lensPoint can not be pre-calculated in this case
		Point p = lightVertex.bsdf.hitPoint.p;
		eyeDir = scene.GetCamera().GetDir();
		// calculate distance from vertex to camera plane
		const float D = -eyeDir.x*lensPoint.x - eyeDir.y*lensPoint.y - eyeDir.z*lensPoint.z;
		eyeDistance = eyeDir.x*p.x + eyeDir.y*p.y + eyeDir.z*p.z + D;
		eyeDistance = fabsf(eyeDistance);
	} else {
		eyeDir = Vector(lightVertex.bsdf.hitPoint.p - lensPoint);
		eyeDistance = eyeDir.Length();
		eyeDir /= eyeDistance;
	}

	float bsdfPdfW, bsdfRevPdfW;
	BSDFEvent event;
	const Spectrum bsdfEval = lightVertex.bsdf.Evaluate(-eyeDir, &event, &bsdfPdfW, &bsdfRevPdfW);

	if (!bsdfEval.Black()) {
		float filmX, filmY;
		bool sampleSuccess;
		Ray eyeRay;
		
		if (scene.GetCamera().GetType() == Camera::ORTHOGRAPHIC){
			Vector rayDir(lightVertex.bsdf.hitPoint.p - lensPoint);
			eyeRay = Ray(lightVertex.bsdf.hitPoint.p, rayDir,
				0.f,
				eyeDistance,
				time);
			sampleSuccess = scene.GetCamera().ProjectToImage(&eyeRay, &filmX, &filmY);
		} else {
			eyeRay = Ray(lensPoint, eyeDir,
				0.f,
				eyeDistance,
				time);
			sampleSuccess = scene.GetCamera().GetSamplePosition(&eyeRay, &filmX, &filmY);
		}
		if (sampleSuccess) {
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
			if (!scene.Intersect(
					luxrays::make_observer(device),
					LIGHT_RAY | CAMERA_RAY,
					&volInfo, u0, &traceRay, &traceRayHit, &bsdfConn,
					&connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				if (lightVertex.depth >= engine->rrDepth) {
					// Russian Roulette
					const float prob = RenderEngine::RussianRouletteProb(bsdfEval, engine->rrImportanceCap);
					bsdfRevPdfW *= prob;
				}

				const float cosToCamera = Dot(lightVertex.bsdf.hitPoint.shadeN, -eyeDir);
				float cameraPdfW, fluxToRadianceFactor;
				scene.GetCamera().GetPDF(eyeRay, eyeDistance, filmX, filmY, &cameraPdfW, &fluxToRadianceFactor);
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
	auto& scene = engine->renderConfig.GetScene();
	
	if (!eyeVertex.bsdf.IsDelta()) {
		// Pick a light source to sample
		const Normal landingNormal = eyeVertex.bsdf.hitPoint.intoObject ? eyeVertex.bsdf.hitPoint.geometryN : -eyeVertex.bsdf.hitPoint.geometryN;
		float lightPickPdf;
		auto light = scene.GetLightSources().GetEmitLightStrategy().SampleLights(
			scene, u0,
			eyeVertex.bsdf.hitPoint.p,
			landingNormal,
			eyeVertex.bsdf.IsVolume(),
			&lightPickPdf
		);

		if (light) {
			Ray shadowRay;
			float directPdfW, emissionPdfW, cosThetaAtLight;
			const Spectrum lightRadiance = light->Illuminate(scene, eyeVertex.bsdf,
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
					if (!scene.Intersect(
						luxrays::make_observer(device),
						EYE_RAY | SHADOW_RAY, &volInfo, u4,
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
						// Disable MIS if we have gone trough a shadow transparent object
						const float misWeight = shadowBsdf.hitPoint.throughShadowTransparency ?
							1.f : (1.f / (weightLight + 1.f + weightCamera));

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
	LightSourceConstRef light,
	const Spectrum &lightRadiance,
	const float directPdfA,
	const float emissionPdfW,
	const PathVertexVM &eyeVertex,
	Spectrum *radiance
) const {
	if (lightRadiance.Black())
		return;

	if (eyeVertex.depth == 1) {
		*radiance += eyeVertex.throughput * lightRadiance;
		return;
	}

	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	auto& scene = engine->renderConfig.GetScene();

	const float lightPickPdf = scene.GetLightSources().GetEmitLightStrategy().SampleLightPdf(
		light,
		eyeVertex.bsdf.hitPoint.p,
		eyeVertex.bsdf.hitPoint.geometryN,
		eyeVertex.bsdf.IsVolume()
	);

	// MIS weight
	const float weightCamera = MIS(directPdfA * lightPickPdf) * eyeVertex.dVCM +
		MIS(emissionPdfW * lightPickPdf) * eyeVertex.dVC;
	const float misWeight = 1.f / (weightCamera + 1.f);

	*radiance += misWeight * eyeVertex.throughput * lightRadiance;

	assert (radiance->IsValid());
}

void BiDirCPURenderThread::DirectHitLight(
	const bool finiteLightSource,
	const PathVertexVM &eyeVertex,
	SampleResult &eyeSampleResult
) const {
	float directPdfA, emissionPdfW;
	if (finiteLightSource) {
		const Spectrum lightRadiance = eyeVertex.bsdf.GetEmittedRadiance(
			&directPdfA, &emissionPdfW
		);

		DirectHitLight(
			*eyeVertex.bsdf.GetLightSource(),
			lightRadiance,
			directPdfA,
			emissionPdfW,
			eyeVertex, &eyeSampleResult.radiance[eyeVertex.bsdf.GetLightID()]
		);
	} else {
		BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
		auto& scene = engine->renderConfig.GetScene();

		for(EnvLightSource& el: scene.GetLightSources().GetEnvLightSources()) {
			const Spectrum lightRadiance = el.GetRadiance(scene,
					(eyeVertex.depth == 1) ? nullptr : &eyeVertex.bsdf,
					eyeVertex.bsdf.hitPoint.fixedDir, &directPdfA, &emissionPdfW);

			DirectHitLight(el, lightRadiance, directPdfA, emissionPdfW,
					eyeVertex, &eyeSampleResult.radiance[el.GetID()]);
		}
	}
}

bool BiDirCPURenderThread::TraceLightPath(const float time,
		const SamplerUPtr& sampler, CameraConstRef camera,
		vector<PathVertexVM> &lightPathVertices,
		vector<SampleResult> &sampleResults) const {
	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	auto& scene = engine->renderConfig.GetScene();

	// Select one light source
	// BiDir can use only a single strategy, emit in this case
	float lightPickPdf;
	auto light = scene.GetLightSources().GetEmitLightStrategy().
			SampleLights(scene, sampler->GetSample(2), &lightPickPdf);
	if (!light)
		return false;

	// Initialize the light path
	PathVertexVM lightVertex;
	lightVertex.lightID = light->GetID();
	
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray lightRay;
	lightVertex.throughput = light->Emit(scene,
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
			const bool hit = scene.Intersect(
					luxrays::make_observer(device),
					LIGHT_RAY | INDIRECT_RAY,
					&lightVertex.volInfo, sampler->GetSample(sampleOffset),
					&lightRay, &nextEventRayHit, &lightVertex.bsdf,
					&connectionThroughput);

			if (hit) {
				// Something was hit
				
				// Check if it is something with a not black shadow transparency
				// and stop if it has. Direct light sampling will take care of
				// this kind of paths.
				if (!lightVertex.bsdf.GetPassThroughShadowTransparency().Black() & !lightVertex.bsdf.GetPassThroughShadowTransparencyOverride())
					break;

				// Update the new light vertex
				lightVertex.throughput *= connectionThroughput;
		
				// Infinite lights use MIS based on solid angle instead of area; distant
				// and laser lights emit per area of a disk, so they skip the distance factor too
				if((lightVertex.depth > 1) || !(light->IsEnvironmental() ||
						(light->GetType() == TYPE_SHARPDISTANT) || (light->GetType() == TYPE_DISTANT) ||
						(light->GetType() == TYPE_LASER)))
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

					// Sample a point on the camera lens
					Point lensPoint;
					camera.SampleLens(time, sampler->GetSample(3), sampler->GetSample(4), &lensPoint);

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

bool BiDirCPURenderThread::Bounce(const float time, const SamplerUPtr& sampler,
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

void BiDirCPURenderThread::RenderFunc(std::stop_token stop_token) {
#ifndef NDEBUG
	SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] Rendering thread started");
#endif

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	BiDirCPURenderEngine *engine = (BiDirCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen

	auto rndGen = std::make_unique<RandomGenerator>(engine->seedBase + 1 + threadIndex);
	auto& scene = engine->renderConfig.GetScene();
	auto& camera = scene.GetCamera();
	PhotonGICache *photonGICache = engine->photonGICache;

	// Albedo and Normal AOV warm up
	if (engine->aovWarmupSPP > 0)
		AOVWarmUp(stop_token, rndGen);

	// Setup the sampler
	auto sampler = engine->renderConfig.AllocSampler(
		rndGen,
		engine->GetFilm(),
		engine->GetSampleSplatter(),
		engine->samplerSharedData,
		Properties()
	);
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

	for(u_int steps = 0; !stop_token.stop_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!stop_token.stop_requested() && engine->pauseMode)
				std::this_thread::sleep_for(100ms);

			if (stop_token.stop_requested())
				break;
		}

		sampleResults.clear();
		lightPathVertices.clear();

		const float timeSample = sampler->GetSample(12);
		const float time = scene.GetCamera().GenerateRayTime(timeSample);

		/*
		// Sample a point on the camera lens
		Point lensPoint;
		if (!camera.SampleLens(time, sampler->GetSample(3), sampler->GetSample(4),
				&lensPoint)) {
			assert (SampleResult::IsAllValid(sampleResults));

			sampler->NextSample(sampleResults);
			continue;
		}
		*/

		//----------------------------------------------------------------------
		// Trace light path
		//----------------------------------------------------------------------

		const bool validLightPath = TraceLightPath(time, sampler, camera, lightPathVertices, sampleResults);
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
			camera.GenerateRay(time,
					eyeSampleResult.filmX, eyeSampleResult.filmY, &eyeRay,
					&eyeVertex.volInfo, sampler->GetSample(10), sampler->GetSample(11));

			// Required by MIS weights update
			eyeVertex.bsdf.hitPoint.fixedDir = -eyeRay.d;
			eyeVertex.throughput = Spectrum(1.f);
			float cameraPdfW;
			scene.GetCamera().GetPDF(eyeRay, 0.f, eyeSampleResult.filmX, eyeSampleResult.filmY, &cameraPdfW, nullptr);
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
				const bool hit = scene.Intersect(
						luxrays::make_observer(device),
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

				float t_MIS;
				if (eyeSampleResult.firstPathVertex) {
					eyeSampleResult.alpha = 1.f;
					eyeSampleResult.depth = eyeRayHit.t;
					eyeSampleResult.position = eyeVertex.bsdf.hitPoint.p;
					eyeSampleResult.geometryNormal = eyeVertex.bsdf.hitPoint.geometryN;
					eyeSampleResult.materialID = eyeVertex.bsdf.GetMaterialID();
					eyeSampleResult.objectID = eyeVertex.bsdf.GetObjectID();
					eyeSampleResult.uv = eyeVertex.bsdf.hitPoint.GetUV(0);
					// for the camera ray, we need to add the clipping distance
					// because eyeRayHit.t is measured from clipping start.
					// Otherwise, the brightness near the front clipping plane may be distorted.
					t_MIS = eyeRayHit.t + camera.clipHither;
				} 
				else{
					t_MIS = eyeRayHit.t;
				}

				// Update MIS constants
				const float factor = 1.f / MIS(AbsDot(eyeVertex.bsdf.hitPoint.shadeN, eyeVertex.bsdf.hitPoint.fixedDir));
				eyeVertex.dVCM *= MIS(t_MIS * t_MIS) * factor;
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
                std::this_thread::yield();
#endif
			}
		}
		
		assert (SampleResult::IsAllValid(sampleResults));

		// Variance clamping
		if (varianceClamping.hasClamping()) {
			for(u_int i = 0; i < sampleResults.size(); ++i)
				varianceClamping.Clamp(engine->GetFilm(), sampleResults[i]);

			assert (SampleResult::IsAllValid(sampleResults));
		}

		sampler->NextSample(sampleResults);

		// Check halt conditions
		if (engine->GetFilm().GetConvergence() == 1.f)
			break;

		if (photonGICache) {
			const u_int spp = engine->GetFilm().GetTotalEyeSampleCount() / engine->GetFilm().GetPixelCount();
			photonGICache->Update(threadIndex, spp);
                }
	} // ~for

	threadDone = true;

	// This is done to stop threads pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	if (photonGICache)
		photonGICache->FinishUpdate(threadIndex);

#ifndef NDEBUG
	SLG_LOG("[BiDirCPURenderThread::" << threadIndex << "] Rendering thread halted");
#endif
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
