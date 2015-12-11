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

#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/volumes/volume.h"
#include "slg/utils/varianceclamping.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathCPU RenderThread
//------------------------------------------------------------------------------

PathCPURenderThread::PathCPURenderThread(PathCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device) {
}

void PathCPURenderThread::DirectLightSampling(
		const float time,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		PathVolumeInfo volInfo, const u_int pathVertexCount,
		SampleResult *sampleResult) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	if (!bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(u0, &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW;
		Spectrum lightRadiance = light->Illuminate(*scene, bsdf.hitPoint.p,
				u1, u2, u3, &lightRayDir, &distance, &directPdfW);
		assert (!lightRadiance.IsNaN() && !lightRadiance.IsInf());

		if (!lightRadiance.Black()) {
			assert (!isnan(directPdfW) && !isinf(directPdfW));

			BSDFEvent event;
			float bsdfPdfW;
			Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW);
			assert (!bsdfEval.IsNaN() && !bsdfEval.IsInf());

			if (!bsdfEval.Black()) {
				assert (!isnan(bsdfPdfW) && !isnan(bsdfPdfW));

				Ray shadowRay(bsdf.hitPoint.p, lightRayDir,
						0.f,
						distance,
						time);
				shadowRay.UpdateMinMaxWithEpsilon();
				RayHit shadowRayHit;
				BSDF shadowBsdf;
				Spectrum connectionThroughput;
				// Check if the light source is visible
				if (!scene->Intersect(device, false, &volInfo, u4, &shadowRay,
						&shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					// I'm ignoring volume emission because it is not sampled in
					// direct light step.
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;
					const float factor = 1.f / directLightSamplingPdfW;

					// The +1 is there to account the current path vertex used for DL
					if (pathVertexCount + 1 >= engine->rrDepth) {
						// Russian Roulette
						bsdfPdfW *= RenderEngine::RussianRouletteProb(bsdfEval, engine->rrImportanceCap);
					}

					// MIS between direct light sampling and BSDF sampling
					//
					// Note: I have to avoiding MIS on the last path vertex
					const float weight = (!sampleResult->lastPathVertex &&  (light->IsEnvironmental() || light->IsIntersectable())) ? 
						PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

					const Spectrum incomingRadiance = bsdfEval * (weight * factor) * connectionThroughput * lightRadiance;

					sampleResult->AddDirectLight(light->GetID(), event, pathThroughput, incomingRadiance, 1.f);

					// The first path vertex is not handled by AddDirectLight(). This is valid
					// for irradiance AOV only if it is not a SPECULAR material.
					//
					// Note: irradiance samples the light sources only here (i.e. no
					// direct hit, no MIS, it would be useless)
					//
					// Note: RR is ignored here because it can not happen on first path vertex
					if ((sampleResult->firstPathVertex) && !(bsdf.GetEventTypes() & SPECULAR))
						sampleResult->irradiance =
								(INV_PI * fabsf(Dot(bsdf.hitPoint.shadeN, shadowRay.d)) *
								factor) * connectionThroughput * lightRadiance;
				}
			}
		}
	}
}

void PathCPURenderThread::DirectHitFiniteLight(const BSDFEvent lastBSDFEvent,
		const Spectrum &pathThroughput, const float distance, const BSDF &bsdf,
		const float lastPdfW, SampleResult *sampleResult) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!(lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = scene->lightDefs.GetLightStrategy()->SampleLightPdf(bsdf.GetLightSource());
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		sampleResult->AddEmission(bsdf.GetLightID(), pathThroughput, weight * emittedRadiance);
	}
}

void PathCPURenderThread::DirectHitInfiniteLight(const BSDFEvent lastBSDFEvent,
		const Spectrum &pathThroughput, const Vector &eyeDir, const float lastPdfW,
		SampleResult *sampleResult) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	BOOST_FOREACH(EnvLightSource *envLight, scene->lightDefs.GetEnvLightSources()) {
		float directPdfW;
		const Spectrum envRadiance = envLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (!envRadiance.Black()) {
			float weight;
			if(!(lastBSDFEvent & SPECULAR)) {
				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(lastPdfW, directPdfW);
			} else
				weight = 1.f;

			sampleResult->AddEmission(envLight->GetID(), pathThroughput, weight * envRadiance);
		}
	}
}

void PathCPURenderThread::GenerateEyeRay(Ray &eyeRay, Sampler *sampler, SampleResult &sampleResult) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;

	const float u0 = sampler->GetSample(0);
	const float u1 = sampler->GetSample(1);
	threadFilm->GetSampleXY(u0, u1, &sampleResult.filmX, &sampleResult.filmY);

	if (engine->useFastPixelFilter) {
		// Use fast pixel filtering, like the one used in BIASPATH.

		sampleResult.pixelX = Floor2UInt(sampleResult.filmX);
		sampleResult.pixelY = Floor2UInt(sampleResult.filmY);

		const float uSubPixelX = sampleResult.filmX - sampleResult.pixelX;
		const float uSubPixelY = sampleResult.filmY - sampleResult.pixelY;

		// Sample according the pixel filter distribution
		float distX, distY;
		engine->pixelFilterDistribution->SampleContinuous(uSubPixelX, uSubPixelY, &distX, &distY);

		sampleResult.filmX = sampleResult.pixelX + .5f + distX;
		sampleResult.filmY = sampleResult.pixelY + .5f + distY;
	}

	Camera *camera = engine->renderConfig->scene->camera;
	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay,
		sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));
}

void PathCPURenderThread::RenderFunc() {
	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	const u_int filmWidth = threadFilm->GetWidth();
	const u_int filmHeight = threadFilm->GetHeight();

	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, threadFilm, engine->sampleSplatter,
			engine->samplerSharedData);
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 9;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		(engine->maxPathDepth + 1) * sampleStepSize; // For each path vertex
	sampler->RequestSamples(sampleSize);
	
	VarianceClamping varianceClamping(engine->sqrtVarianceClampMaxValue);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH |
		Film::POSITION | Film::GEOMETRY_NORMAL | Film::SHADING_NORMAL | Film::MATERIAL_ID |
		Film::DIRECT_DIFFUSE | Film::DIRECT_GLOSSY | Film::EMISSION | Film::INDIRECT_DIFFUSE |
		Film::INDIRECT_GLOSSY | Film::INDIRECT_SPECULAR | Film::DIRECT_SHADOW_MASK |
		Film::INDIRECT_SHADOW_MASK | Film::UV | Film::RAYCOUNT | Film::IRRADIANCE |
		Film::OBJECT_ID,
		engine->film->GetRadianceGroupCount());

	// I can not use engine->renderConfig->GetProperty() here because the
	// RenderConfig properties cache is not thread safe
	const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>() *
		filmWidth * filmHeight;

	sampleResult.useFilmSplat = !(engine->useFastPixelFilter);
	for(u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		// Set to 0.0 all result colors
		sampleResult.emission = Spectrum();
		for (u_int i = 0; i < sampleResult.radiance.size(); ++i)
			sampleResult.radiance[i] = Spectrum();
		sampleResult.directDiffuse = Spectrum();
		sampleResult.directGlossy = Spectrum();
		sampleResult.indirectDiffuse = Spectrum();
		sampleResult.indirectGlossy = Spectrum();
		sampleResult.indirectSpecular = Spectrum();
		sampleResult.directShadowMask = 1.f;
		sampleResult.indirectShadowMask = 1.f;
		sampleResult.irradiance = Spectrum();

		// To keep track of the number of rays traced
		const double deviceRayCount = device->GetTotalRaysCount();

		Ray eyeRay;
		GenerateEyeRay(eyeRay, sampler, sampleResult);

		u_int pathVertexCount = 1;
		BSDFEvent lastBSDFEvent = SPECULAR; // SPECULAR is required to avoid MIS
		float lastPdfW = 1.f;
		Spectrum pathThroughput(1.f);
		PathVolumeInfo volInfo;
		BSDF bsdf;
		for (;;) {
			sampleResult.firstPathVertex = (pathVertexCount == 1);
			sampleResult.lastPathVertex = (pathVertexCount == engine->maxPathDepth);

			const u_int sampleOffset = sampleBootSize + (pathVertexCount - 1) * sampleStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			const bool hit = scene->Intersect(device, false,
					&volInfo, sampler->GetSample(sampleOffset),
					&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput,
					&pathThroughput, &sampleResult);
			pathThroughput *= connectionThroughput;
			// Note: pass-through check is done inside Scene::Intersect()

			if (!hit) {
				// Nothing was hit, look for env. lights
				DirectHitInfiniteLight(lastBSDFEvent, pathThroughput, eyeRay.d,
						lastPdfW, &sampleResult);

				if (sampleResult.firstPathVertex) {
					sampleResult.alpha = 0.f;
					sampleResult.depth = std::numeric_limits<float>::infinity();
					sampleResult.position = Point(
							std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity());
					sampleResult.geometryNormal = Normal(
							std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity());
					sampleResult.shadingNormal = Normal(
							std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity());
					sampleResult.materialID = std::numeric_limits<u_int>::max();
					sampleResult.objectID = std::numeric_limits<u_int>::max();
					sampleResult.uv = UV(std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity());
				}
				break;
			}

			// Something was hit
			if (sampleResult.firstPathVertex) {
				sampleResult.alpha = 1.f;
				sampleResult.depth = eyeRayHit.t;
				sampleResult.position = bsdf.hitPoint.p;
				sampleResult.geometryNormal = bsdf.hitPoint.geometryN;
				sampleResult.shadingNormal = bsdf.hitPoint.shadeN;
				sampleResult.materialID = bsdf.GetMaterialID();
				sampleResult.objectID = bsdf.GetObjectID();
				sampleResult.uv = bsdf.hitPoint.uv;
			}

			// Check if it is a light source
			if (bsdf.IsLightSource()) {
				DirectHitFiniteLight(lastBSDFEvent, pathThroughput, eyeRayHit.t,
						bsdf, lastPdfW, &sampleResult);
			}

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			// I avoid to do DL on the last vertex otherwise it introduces a lot of
			// noise because I can not use MIS.
			// I handle as a special case when the path vertex is both the first
			// and the last: I do direct light sampling without MIS.
			if (sampleResult.lastPathVertex && !sampleResult.firstPathVertex)
				break;

			DirectLightSampling(
					eyeRay.time,
					sampler->GetSample(sampleOffset + 1),
					sampler->GetSample(sampleOffset + 2),
					sampler->GetSample(sampleOffset + 3),
					sampler->GetSample(sampleOffset + 4),
					sampler->GetSample(sampleOffset + 5),
					pathThroughput, bsdf, volInfo, pathVertexCount, &sampleResult);

			if (sampleResult.lastPathVertex)
				break;

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			Vector sampledDir;
			float cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
					sampler->GetSample(sampleOffset + 6),
					sampler->GetSample(sampleOffset + 7),
					&lastPdfW, &cosSampledDir, &lastBSDFEvent);
			assert (!bsdfSample.IsNaN() && !bsdfSample.IsInf());
			if (bsdfSample.Black())
				break;
			assert (!isnan(lastPdfW) && !isnan(lastPdfW));

			if (sampleResult.firstPathVertex)
				sampleResult.firstPathVertexEvent = lastBSDFEvent;

			Spectrum throughputFactor(1.f);
			const float rrProb = RenderEngine::RussianRouletteProb(bsdfSample, engine->rrImportanceCap);
			if (pathVertexCount >= engine->rrDepth) {
				// Russian Roulette
				if (rrProb < sampler->GetSample(sampleOffset + 8))
					break;

				// Increase path contribution
				throughputFactor /= rrProb;
			}

			// PDF clamping (or better: scaling)
			throughputFactor *= min(1.f, (lastBSDFEvent & SPECULAR) ? 1.f : (lastPdfW / engine->pdfClampValue));
			throughputFactor *= bsdfSample;

			pathThroughput *= throughputFactor;
			assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

			// This is valid for irradiance AOV only if it is not a SPECULAR material and
			// first path vertex. Set or update sampleResult.irradiancePathThroughput
			if (sampleResult.firstPathVertex) {
				if (!(bsdf.GetEventTypes() & SPECULAR))
					sampleResult.irradiancePathThroughput = INV_PI * fabsf(Dot(bsdf.hitPoint.shadeN, sampledDir)) / rrProb;
				else
					sampleResult.irradiancePathThroughput = Spectrum();
			} else
				sampleResult.irradiancePathThroughput *= throughputFactor;

			// Update volume information
			volInfo.Update(lastBSDFEvent, bsdf);

			eyeRay.Update(bsdf.hitPoint.p, sampledDir);
			++pathVertexCount;
		}

		sampleResult.rayCount = (float)(device->GetTotalRaysCount() - deviceRayCount);

		// Variance clamping
		if (varianceClamping.hasClamping())
			varianceClamping.Clamp(*threadFilm, sampleResult);

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif

		if ((haltDebug > 0u) && (steps >= haltDebug))
			break;
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
