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

#include "slg/engines/biaspathcpu/biaspathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathCPU RenderThread
//------------------------------------------------------------------------------

BiasPathCPURenderThread::BiasPathCPURenderThread(BiasPathCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUTileRenderThread(engine, index, device) {
}

void BiasPathCPURenderThread::SampleGrid(RandomGenerator *rndGen, const u_int size,
		const u_int ix, const u_int iy, float *u0, float *u1) const {
	*u0 = rndGen->floatValue();
	*u1 = rndGen->floatValue();

	if (size > 1) {
		const float idim = 1.f / size;
		*u0 = (ix + *u0) * idim;
		*u1 = (iy + *u1) * idim;
	}
}

void BiasPathCPURenderThread::DirectLightSampling(
		const LightSource *light, const float lightPickPdf,
		const float u0, const float u1,
		const float u2, const float u3,
		const float time,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		PathVolumeInfo volInfo, SampleResult *sampleResult, const float lightScale) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector lightRayDir;
	float distance, directPdfW;
	Spectrum lightRadiance = light->Illuminate(*scene, bsdf.hitPoint.p,
			u0, u1, u2, &lightRayDir, &distance, &directPdfW);
	const float cosThetaToLight = AbsDot(lightRayDir, bsdf.hitPoint.shadeN);

	if ((lightRadiance.Y() * cosThetaToLight / directPdfW > engine->lowLightThreashold) &&
			(distance > engine->nearStartLight)) {
		BSDFEvent event;
		float bsdfPdfW;
		const Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW);

		if (!bsdfEval.Black()) {
			const float directLightSamplingPdfW = directPdfW * lightPickPdf;
			const float factor = 1.f / directLightSamplingPdfW;

			// MIS between direct light sampling and BSDF sampling
			const float weight = (!sampleResult->lastPathVertex && (light->IsEnvironmental() || light->IsIntersectable())) ? 
							PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

			const float epsilon = Max(MachineEpsilon::E(bsdf.hitPoint.p), MachineEpsilon::E(distance));
			Ray shadowRay(bsdf.hitPoint.p, lightRayDir,
					epsilon,
					distance - epsilon,
					time);
			RayHit shadowRayHit;
			BSDF shadowBsdf;
			Spectrum connectionThroughput;
			// Check if the light source is visible
			if (!scene->Intersect(device, false, &volInfo, u3, &shadowRay,
					&shadowRayHit, &shadowBsdf, &connectionThroughput)) {
				// I'm ignoring volume emission because it is not sampled in
				// direct light step.
				const Spectrum incomingRadiance = bsdfEval * (lightScale * weight * factor) * connectionThroughput * lightRadiance;
				sampleResult->AddDirectLight(light->GetID(), event, pathThroughput, incomingRadiance, lightScale);

				// The first path vertex is not handled by AddDirectLight(). This is valid
				// for irradiance AOV only if it is not a SPECULAR material.
				//
				// Note: irradiance samples the light sources only here (i.e. no
				// direct hit, no MIS, it would be useless)
				if ((sampleResult->firstPathVertex) && !(bsdf.GetEventTypes() & SPECULAR))
					sampleResult->irradiance +=
								(INV_PI * fabsf(Dot(bsdf.hitPoint.shadeN, shadowRay.d)) *
								lightScale * factor) * connectionThroughput * lightRadiance;
			}
		}
	}
}

void BiasPathCPURenderThread::DirectLightSamplingONE(
		const float time,
		RandomGenerator *rndGen,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		const PathVolumeInfo &volInfo, SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Pick a light source to sample
	float lightPickPdf;
	const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(rndGen->floatValue(), &lightPickPdf);

	DirectLightSampling(
			light, lightPickPdf,
			rndGen->floatValue(), rndGen->floatValue(),
			rndGen->floatValue(), rndGen->floatValue(),
			time,
			pathThroughput, bsdf, volInfo, sampleResult, 1.f);
}

void BiasPathCPURenderThread::DirectLightSamplingALL(
		const float time,
		const u_int sampleCount,
		RandomGenerator *rndGen,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		const PathVolumeInfo &volInfo, SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	for (u_int i = 0; i < sampleCount; ++i) {
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(rndGen->floatValue(), &lightPickPdf);
		const int samples = light->GetSamples();
		const u_int samplesToDo = (samples < 0) ? engine->directLightSamples : ((u_int)samples);

		const float scaleFactor = 1.f / (samplesToDo * samplesToDo * sampleCount);
		for (u_int sampleY = 0; sampleY < samplesToDo; ++sampleY) {
			for (u_int sampleX = 0; sampleX < samplesToDo; ++sampleX) {
				float u0, u1;
				SampleGrid(rndGen, samplesToDo, sampleX, sampleY, &u0, &u1);

				DirectLightSampling(
						light, lightPickPdf, u0, u1,
						rndGen->floatValue(), rndGen->floatValue(), time,
						pathThroughput, bsdf, volInfo, sampleResult, scaleFactor);
			}
		}
	}
}

void BiasPathCPURenderThread::DirectHitFiniteLight(const BSDFEvent lastBSDFEvent,
		const Spectrum &pathThroughput, const float distance, const BSDF &bsdf,
		const float lastPdfW, SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	if (!sampleResult->firstPathVertex &&
			(((sampleResult->firstPathVertexEvent & DIFFUSE) && !bsdf.IsVisibleIndirectDiffuse()) ||
			((sampleResult->firstPathVertexEvent & GLOSSY) && !bsdf.IsVisibleIndirectGlossy()) ||
			((sampleResult->firstPathVertexEvent & SPECULAR) && !bsdf.IsVisibleIndirectSpecular())))
			return;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!(lastBSDFEvent & SPECULAR)) {
			// This PDF used for MIS is correct because lastSpecular is always
			// true when using DirectLightSamplingALL()
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

void BiasPathCPURenderThread::DirectHitEnvLight(const BSDFEvent lastBSDFEvent,
		const Spectrum &pathThroughput, const Vector &eyeDir, const float lastPdfW,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Infinite light
	BOOST_FOREACH(EnvLightSource *envLight, scene->lightDefs.GetEnvLightSources()) {
		float directPdfW;
		if (sampleResult->firstPathVertex ||
				(((sampleResult->firstPathVertexEvent & DIFFUSE) && envLight->IsVisibleIndirectDiffuse()) ||
				((sampleResult->firstPathVertexEvent & GLOSSY) && envLight->IsVisibleIndirectGlossy()) ||
				((sampleResult->firstPathVertexEvent & SPECULAR) && envLight->IsVisibleIndirectSpecular()))) {
			const Spectrum envRadiance = envLight->GetRadiance(*scene, -eyeDir, &directPdfW);
			if (!envRadiance.Black()) {
				float weight;
				if(!(lastBSDFEvent & SPECULAR)) {
					// This PDF used for MIS is correct because lastSpecular is always
					// true when using DirectLightSamplingALL()

					// MIS between BSDF sampling and direct light sampling
					weight = PowerHeuristic(lastPdfW, directPdfW);
				} else
					weight = 1.f;

				sampleResult->AddEmission(envLight->GetID(), pathThroughput, weight * envRadiance);
			}
		}
	}
}

void BiasPathCPURenderThread::ContinueTracePath(RandomGenerator *rndGen,
		PathDepthInfo depthInfo, Ray ray,
		Spectrum pathThroughput, BSDFEvent lastBSDFEvent, float lastPdfW,
		PathVolumeInfo *volInfo, SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	BSDF bsdf;
	for (;;) {
		RayHit rayHit;
		Spectrum connectionThroughput;
		const bool hit = scene->Intersect(device, false,
				volInfo, rndGen->floatValue(),
				&ray, &rayHit, &bsdf, &connectionThroughput,
				&pathThroughput, sampleResult);
		pathThroughput *= connectionThroughput;
		// Update also irradiance AOV path throughput
		sampleResult->irradiancePathThroughput *= connectionThroughput;

		// Note: pass-through check is done inside Scene::Intersect()

		if (!hit) {
			// Nothing was hit, look for env. lights
			DirectHitEnvLight(lastBSDFEvent, pathThroughput,
					ray.d, lastPdfW, sampleResult);
			break;
		}

		// Something was hit

		// Check if it is visible in indirect paths
		if (((sampleResult->firstPathVertexEvent & DIFFUSE) && !bsdf.IsVisibleIndirectDiffuse()) ||
				((sampleResult->firstPathVertexEvent & GLOSSY) && !bsdf.IsVisibleIndirectGlossy()) ||
				((sampleResult->firstPathVertexEvent & SPECULAR) && !bsdf.IsVisibleIndirectSpecular()))
			break;

		// Check if it is a light source
		if (bsdf.IsLightSource() && (rayHit.t > engine->nearStartLight))
			DirectHitFiniteLight(lastBSDFEvent,
					pathThroughput, rayHit.t, bsdf, lastPdfW, sampleResult);

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		// I avoid to do DL on the last vertex otherwise it introduces a lot of
		// noise because I can not use MIS
		sampleResult->lastPathVertex = depthInfo.IsLastPathVertex(engine->maxPathDepth, bsdf.GetEventTypes());
		if (sampleResult->lastPathVertex)
			break;

		if (!bsdf.IsDelta())
			DirectLightSamplingONE(ray.time, rndGen, pathThroughput, bsdf, *volInfo, sampleResult);

		//----------------------------------------------------------------------
		// Build the next path vertex ray
		//----------------------------------------------------------------------

		Vector sampledDir;
		float cosSampledDir;
		const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
				rndGen->floatValue(),
				rndGen->floatValue(),
				&lastPdfW, &cosSampledDir, &lastBSDFEvent);
		if (bsdfSample.Black())
			break;

		// Increment path depth informations
		depthInfo.IncDepths(lastBSDFEvent);

		// Update volume information
		volInfo->Update(lastBSDFEvent, bsdf);

		const Spectrum throughputFactor = bsdfSample * min(1.f, (lastBSDFEvent & SPECULAR) ? 1.f : (lastPdfW / engine->pdfClampValue));
		pathThroughput *= throughputFactor;
		assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

		// Update also irradiance AOV path throughput
		sampleResult->irradiancePathThroughput *= throughputFactor;

		ray.Update(bsdf.hitPoint.p, sampledDir);
	}
}

// NOTE: bsdf.hitPoint.passThroughEvent is modified by this method
void BiasPathCPURenderThread::SampleComponent(
		const float time, RandomGenerator *rndGen,
		const BSDFEvent requestedEventTypes, const u_int size,
		const luxrays::Spectrum &pathThroughput, BSDF &bsdf,
		const PathVolumeInfo &startVolInfo, SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;

	const float scaleFactor = 1.f / (size * size);
	float indirectShadowMask = sampleResult->indirectShadowMask;
	for (u_int sampleY = 0; sampleY < size; ++sampleY) {
		for (u_int sampleX = 0; sampleX < size; ++sampleX) {
			sampleResult->indirectShadowMask = 1.f;

			float u0, u1;
			SampleGrid(rndGen, size, sampleX, sampleY, &u0, &u1);
			bsdf.hitPoint.passThroughEvent = rndGen->floatValue();

			Vector sampledDir;
			BSDFEvent event;
			float pdfW, cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir, u0, u1,
					&pdfW, &cosSampledDir, &event, requestedEventTypes);
			if (!bsdfSample.Black()) {
				PathDepthInfo depthInfo;
				depthInfo.IncDepths(event);

				// Update information about the first path BSDF 
				sampleResult->firstPathVertexEvent = event;

				// Update volume information
				PathVolumeInfo volInfo = startVolInfo;
				volInfo.Update(event, bsdf);

				const float pdfFactor = scaleFactor * min(1.f, (event & SPECULAR) ? 1.f : (pdfW / engine->pdfClampValue));
				const Spectrum continuePathThroughput = pathThroughput * bsdfSample * pdfFactor;
				assert (!continuePathThroughput.IsNaN() && !continuePathThroughput.IsInf());

				// This is valid for irradiance AOV only if it is not a SPECULAR
				// material. Set sampleResult.irradiancePathThroughput
				if (!(bsdf.GetEventTypes() & SPECULAR))
					sampleResult->irradiancePathThroughput = INV_PI * fabsf(Dot(bsdf.hitPoint.shadeN, sampledDir)) * scaleFactor;
				else
					sampleResult->irradiancePathThroughput = Spectrum();

				Ray continueRay(bsdf.hitPoint.p, sampledDir);
				continueRay.time = time;

				ContinueTracePath(rndGen, depthInfo, continueRay,
						continuePathThroughput, event, pdfW, &volInfo, sampleResult);
			}

			// sampleResult->indirectShadowMask requires special handling: the
			// end result must be the average of all path results
			indirectShadowMask += scaleFactor * sampleResult->indirectShadowMask;
		}
	}

	sampleResult->indirectShadowMask = indirectShadowMask;
}

void BiasPathCPURenderThread::TraceEyePath(RandomGenerator *rndGen, const Ray &ray,
		PathVolumeInfo *volInfo, SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	sampleResult->firstPathVertex = true;
	sampleResult->lastPathVertex = false;
	sampleResult->firstPathVertexEvent = NONE;

	Ray eyeRay = ray;
	RayHit eyeRayHit;
	Spectrum pathThroughput(1.f);
	Spectrum connectionThroughput;
	BSDF bsdf;
	const bool hit = scene->Intersect(device, false,
			volInfo, rndGen->floatValue(),
			&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput, &pathThroughput, 
			sampleResult);
	pathThroughput *= connectionThroughput;

	if (!hit) {
		// Nothing was hit, look for env. lights

		// SPECULAR is required to avoid MIS
		DirectHitEnvLight(SPECULAR, pathThroughput, eyeRay.d,
				1.f, sampleResult);

		sampleResult->alpha = 0.f;
		sampleResult->depth = numeric_limits<float>::infinity();
		sampleResult->position = Point(
				numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity());
		sampleResult->geometryNormal = Normal(
				numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity());
		sampleResult->shadingNormal = Normal(
				numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity());
		sampleResult->materialID = numeric_limits<u_int>::max();
		sampleResult->uv = UV(numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity());
	} else {
		// Something was hit
		sampleResult->alpha = 1.f;
		sampleResult->depth = eyeRayHit.t;
		sampleResult->position = bsdf.hitPoint.p;
		sampleResult->geometryNormal = bsdf.hitPoint.geometryN;
		sampleResult->shadingNormal = bsdf.hitPoint.shadeN;
		sampleResult->materialID = bsdf.GetMaterialID();
		sampleResult->uv = bsdf.hitPoint.uv;

		// Check if it is a light source
		if (bsdf.IsLightSource() && (eyeRayHit.t > engine->nearStartLight)) {
			// SPECULAR is required to avoid MIS
			DirectHitFiniteLight(SPECULAR, pathThroughput,
					eyeRayHit.t, bsdf, 1.f, sampleResult);
		}

		// Note: pass-through check is done inside Scene::Intersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		const BSDFEvent materialEventTypes = bsdf.GetEventTypes();
		sampleResult->lastPathVertex = 
				(engine->maxPathDepth.depth <= 1) ||
				((engine->maxPathDepth.diffuseDepth <= 1) && (materialEventTypes & DIFFUSE)) ||
				((engine->maxPathDepth.glossyDepth <= 1) && (materialEventTypes & GLOSSY)) ||
				((engine->maxPathDepth.specularDepth <= 1) && (materialEventTypes & SPECULAR));

		if (!bsdf.IsDelta())
			DirectLightSamplingALL(eyeRay.time, engine->firstVertexLightSampleCount, rndGen,
					pathThroughput, bsdf, *volInfo, sampleResult);

		//----------------------------------------------------------------------
		// Split the path
		//----------------------------------------------------------------------

		sampleResult->indirectShadowMask = 0.f;

		if (!sampleResult->lastPathVertex) {
			sampleResult->firstPathVertex = false;
			int materialSamples = bsdf.GetSamples();

			//------------------------------------------------------------------
			// Sample the diffuse component
			//
			// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
			//------------------------------------------------------------------

			if ((engine->maxPathDepth.diffuseDepth > 0) && (materialEventTypes & DIFFUSE)) {
				const u_int diffuseSamples = (materialSamples < 0) ? engine->diffuseSamples : ((u_int)materialSamples);

				if (diffuseSamples > 0) {
					SampleComponent(eyeRay.time, rndGen, DIFFUSE | REFLECT | TRANSMIT,
							diffuseSamples, pathThroughput, bsdf, *volInfo, sampleResult);
				}
			}

			//------------------------------------------------------------------
			// Sample the glossy component
			//
			// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
			//------------------------------------------------------------------

			if ((engine->maxPathDepth.glossyDepth > 0) && (materialEventTypes & GLOSSY)) {
				const u_int glossySamples = (materialSamples < 0) ? engine->glossySamples : ((u_int)materialSamples);

				if (glossySamples > 0) {
					SampleComponent(eyeRay.time, rndGen, GLOSSY | REFLECT | TRANSMIT, 
							glossySamples, pathThroughput, bsdf, *volInfo, sampleResult);
				}
			}

			//------------------------------------------------------------------
			// Sample the specular component
			//
			// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
			//------------------------------------------------------------------

			if ((engine->maxPathDepth.specularDepth > 0) && (materialEventTypes & SPECULAR)) {
				const u_int specularSamples = (materialSamples < 0) ? engine->specularSamples : ((u_int)materialSamples);

				if (specularSamples > 0) {
					SampleComponent(eyeRay.time, rndGen, SPECULAR | REFLECT | TRANSMIT,
							specularSamples, pathThroughput, bsdf, *volInfo, sampleResult);
				}
			}
		}
	}
}

void BiasPathCPURenderThread::RenderPixelSample(RandomGenerator *rndGen,
		const u_int x, const u_int y,
		const u_int xOffset, const u_int yOffset,
		const u_int sampleX, const u_int sampleY) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;

	float u0, u1;
	SampleGrid(rndGen, engine->aaSamples, sampleX, sampleY, &u0, &u1);

	// Sample according the pixel filter distribution
	engine->pixelFilterDistribution->SampleContinuous(u0, u1, &u0, &u1);

	SampleResult sampleResult(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH |
		Film::POSITION | Film::GEOMETRY_NORMAL | Film::SHADING_NORMAL | Film::MATERIAL_ID |
		Film::DIRECT_DIFFUSE | Film::DIRECT_GLOSSY | Film::EMISSION | Film::INDIRECT_DIFFUSE |
		Film::INDIRECT_GLOSSY | Film::INDIRECT_SPECULAR | Film::DIRECT_SHADOW_MASK |
		Film::INDIRECT_SHADOW_MASK | Film::UV | Film::RAYCOUNT | Film::IRRADIANCE,
		engine->film->GetRadianceGroupCount());

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

	sampleResult.filmX = xOffset + x + .5f + u0;
	sampleResult.filmY = yOffset + y + .5f + u1;
	Ray eyeRay;
	engine->renderConfig->scene->camera->GenerateRay(sampleResult.filmX, sampleResult.filmY,
			&eyeRay, rndGen->floatValue(), rndGen->floatValue(), rndGen->floatValue());

	// Trace the path
	PathVolumeInfo volInfo;
	TraceEyePath(rndGen, eyeRay, &volInfo, &sampleResult);

	// Radiance clamping
	if (engine->radianceClampMaxValue > 0.f)
		sampleResult.ClampRadiance(engine->radianceClampMaxValue);

	sampleResult.rayCount = (float)(device->GetTotalRaysCount() - deviceRayCount);

	tileFilm->AddSampleCount(1.0);
	tileFilm->AddSample(x, y, sampleResult);
}

void BiasPathCPURenderThread::RenderFunc() {
	//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Film *film = engine->film;
	const u_int filmWidth = film->GetWidth();
	const u_int filmHeight = film->GetHeight();

	//--------------------------------------------------------------------------
	// Extract the tile to render
	//--------------------------------------------------------------------------

	TileRepository::Tile *tile = NULL;
	bool interruptionRequested = boost::this_thread::interruption_requested();
	while (engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tile, tileFilm) && !interruptionRequested) {
		// Render the tile
		tileFilm->Reset();
		const u_int tileWidth = Min(engine->tileRepository->tileWidth, filmWidth - tile->xStart);
		const u_int tileHeight = Min(engine->tileRepository->tileHeight, filmHeight - tile->yStart);
		//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Tile: "
		//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
		//		"(" << tileWidth << ", " << tileHeight << ")");

		//----------------------------------------------------------------------
		// Render the tile
		//----------------------------------------------------------------------

		for (u_int y = 0; y < tileHeight && !interruptionRequested; ++y) {
			for (u_int x = 0; x < tileWidth && !interruptionRequested; ++x) {
				for (u_int sampleY = 0; sampleY < engine->aaSamples; ++sampleY) {
					for (u_int sampleX = 0; sampleX < engine->aaSamples; ++sampleX) {
						RenderPixelSample(rndGen, x, y, tile->xStart, tile->yStart, sampleX, sampleY);
					}
				}

				interruptionRequested = boost::this_thread::interruption_requested();
#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}
		}
	}

	delete rndGen;

	//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
