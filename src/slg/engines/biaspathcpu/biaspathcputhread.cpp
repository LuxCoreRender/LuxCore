/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/core/mc.h"
#include "luxrays/core/randomgen.h"

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

bool BiasPathCPURenderThread::DirectLightSampling(
		const bool firstPathVertex, const BSDFEvent pathBSDFEvent,
		const LightSource *light, const float lightPickPdf,
		const float u0, const float u1,
		const float u2, const float u3,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector lightRayDir;
	float distance, directPdfW;
	Spectrum lightRadiance = light->Illuminate(*scene, bsdf.hitPoint.p,
			u0, u1, u2, &lightRayDir, &distance, &directPdfW);

	if ((lightRadiance.Y() > engine->lowLightThreashold) && (distance > engine->nearStartLight)) {
		BSDFEvent event;
		float bsdfPdfW;
		Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW);

		const float cosThetaToLight = AbsDot(lightRayDir, bsdf.hitPoint.shadeN);
		const float directLightSamplingPdfW = directPdfW * lightPickPdf;
		const float factor = cosThetaToLight / directLightSamplingPdfW;

		// MIS between direct light sampling and BSDF sampling
		const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);
		//const float weight = (bsdfPdfW > engine->pdfRejectValue) ?
		//	PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

		const Spectrum illumRadiance = (weight * factor) * pathThroughput * lightRadiance * bsdfEval;

		if (!illumRadiance.Black()) {
			const float epsilon = Max(MachineEpsilon::E(bsdf.hitPoint.p), MachineEpsilon::E(distance));
			Ray shadowRay(bsdf.hitPoint.p, lightRayDir,
					epsilon,
					distance - epsilon);
			RayHit shadowRayHit;
			BSDF shadowBsdf;
			Spectrum connectionThroughput;
			// Check if the light source is visible
			if (!scene->Intersect(device, false, u3, &shadowRay,
					&shadowRayHit, &shadowBsdf, &connectionThroughput)) {
				const Spectrum radiance = connectionThroughput * illumRadiance;
				sampleResult->radiancePerPixelNormalized[light->GetID()] += radiance;

				if (firstPathVertex) {
					if (bsdf.GetEventTypes() & DIFFUSE)
						sampleResult->directDiffuse += radiance;
					else
						sampleResult->directGlossy += radiance;
				} else {
					if (pathBSDFEvent & DIFFUSE)
						sampleResult->indirectDiffuse += radiance;
					else if (pathBSDFEvent & GLOSSY)
						sampleResult->indirectGlossy += radiance;
					else if (pathBSDFEvent & SPECULAR)
						sampleResult->indirectSpecular += radiance;
				}

				return true;
			}
		}
	}

	return false;
}

bool BiasPathCPURenderThread::DirectLightSamplingONE(
		RandomGenerator *rndGen,
		const bool firstPathVertex, const BSDFEvent pathBSDFEvent,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Pick a light source to sample
	float lightPickPdf;
	const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lightPickPdf);

	const bool illuminated = DirectLightSampling(firstPathVertex, pathBSDFEvent,
			light, lightPickPdf,
			rndGen->floatValue(), rndGen->floatValue(),
			rndGen->floatValue(), rndGen->floatValue(),
			pathThroughput, bsdf, sampleResult);

	if (firstPathVertex && !illuminated)
		sampleResult->directShadowMask += 1.f;

	return illuminated;
}

void BiasPathCPURenderThread::DirectLightSamplingALL(
		RandomGenerator *rndGen,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	const u_int lightsSize = scene->GetLightCount();
	for (u_int i = 0; i < lightsSize; ++i) {
		const LightSource *light = scene->GetLightByIndex(i);
		const int samples = light->GetSamples();
		const u_int samplesToDo = (samples < 0) ? engine->directLightSamples : ((u_int)samples);

		const float scaleFactor = 1.f / (samplesToDo * samplesToDo);
		const Spectrum lightPathTrough = pathThroughput * scaleFactor;
		for (u_int sampleY = 0; sampleY < samplesToDo; ++sampleY) {
			for (u_int sampleX = 0; sampleX < samplesToDo; ++sampleX) {
				float u0, u1;
				SampleGrid(rndGen, samplesToDo, sampleX, sampleY, &u0, &u1);

				const bool illuminated = DirectLightSampling(true, NONE,
						light, 1.f, u0, u1,
						rndGen->floatValue(), rndGen->floatValue(),
						lightPathTrough, bsdf, sampleResult);
				if (!illuminated)
					sampleResult->directShadowMask += scaleFactor;
			}
		}
	}
}

void BiasPathCPURenderThread::AddEmission(const bool firstPathVertex, const BSDFEvent pathBSDFEvent,
		const u_int lightID, SampleResult *sampleResult, const Spectrum &emission) const {
	sampleResult->radiancePerPixelNormalized[lightID] += emission;

	if (firstPathVertex)
		sampleResult->emission += emission;
	else {
		sampleResult->indirectShadowMask = 0.f;

		if (pathBSDFEvent & DIFFUSE)
			sampleResult->indirectDiffuse += emission;
		else if (pathBSDFEvent & GLOSSY)
			sampleResult->indirectGlossy += emission;
		else if (pathBSDFEvent & SPECULAR)
			sampleResult->indirectSpecular += emission;
	}
}

bool BiasPathCPURenderThread::DirectHitFiniteLight(const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent, const BSDFEvent pathBSDFEvent,
		const Spectrum &pathThroughput, const float distance, const BSDF &bsdf,
		const float lastPdfW, SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	if (!firstPathVertex &&
			(((pathBSDFEvent & DIFFUSE) && bsdf.IsVisibleIndirectDiffuse()) ||
			((pathBSDFEvent & GLOSSY) && bsdf.IsVisibleIndirectGlossy()) ||
			((pathBSDFEvent & SPECULAR) && bsdf.IsVisibleIndirectSpecular())))
			return false;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (emittedRadiance.Y() > engine->lowLightThreashold) {
		float weight;
		if (!(lastBSDFEvent & SPECULAR)) {
			// This PDF used for MIS is correct because lastSpecular is always
			// true when using DirectLightSamplingALL()
			const float lightPickProb = scene->SampleAllLightPdf(bsdf.GetLightSource());
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		const Spectrum radiance = weight * pathThroughput * emittedRadiance;
		AddEmission(firstPathVertex, pathBSDFEvent, bsdf.GetLightID(), sampleResult, radiance);

		return true;
	}

	return false;
}

bool BiasPathCPURenderThread::DirectHitInfiniteLight(const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent, const BSDFEvent pathBSDFEvent,
		const Spectrum &pathThroughput, const Vector &eyeDir, const float lastPdfW,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Infinite light
	bool illuminated = false;
	float directPdfW;
	const InfiniteLightBase *envLight = scene->envLight;
	if (envLight && (firstPathVertex ||
			(((pathBSDFEvent & DIFFUSE) && envLight->IsVisibleIndirectDiffuse()) ||
			((pathBSDFEvent & GLOSSY) && envLight->IsVisibleIndirectGlossy()) ||
			((pathBSDFEvent & SPECULAR) && envLight->IsVisibleIndirectSpecular())))) {
		const Spectrum envRadiance = envLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (envRadiance.Y() > engine->lowLightThreashold) {
			float weight;
			if(!(lastBSDFEvent & SPECULAR)) {
				// This PDF used for MIS is correct because lastSpecular is always
				// true when using DirectLightSamplingALL()

				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(lastPdfW, directPdfW);
			} else
				weight = 1.f;

			const Spectrum radiance = weight * pathThroughput * envRadiance;
			AddEmission(firstPathVertex, pathBSDFEvent, envLight->GetID(), sampleResult, radiance);
			
			illuminated = true;
		}
	}

	// Sun light
	const SunLight *sunLight = scene->sunLight;
	if (sunLight && (firstPathVertex ||
			(((pathBSDFEvent & DIFFUSE) && sunLight->IsVisibleIndirectDiffuse()) ||
			((pathBSDFEvent & GLOSSY) && sunLight->IsVisibleIndirectGlossy()) ||
			((pathBSDFEvent & SPECULAR) && sunLight->IsVisibleIndirectSpecular())))) {
		const Spectrum sunRadiance = sunLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (sunRadiance.Y() > engine->lowLightThreashold) {
			float weight;
			if(!(lastBSDFEvent & SPECULAR)) {
				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(lastPdfW, directPdfW);
			} else
				weight = 1.f;
				
			const Spectrum radiance = weight * pathThroughput * sunRadiance;
			AddEmission(firstPathVertex, pathBSDFEvent, sunLight->GetID(), sampleResult, radiance);

			illuminated = true;
		}
	}

	return illuminated;
}

bool BiasPathCPURenderThread::ContinueTracePath(RandomGenerator *rndGen,
		PathDepthInfo depthInfo, Ray ray,
		Spectrum pathThroughput, BSDFEvent lastBSDFEvent, float lastPdfW,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	const BSDFEvent pathBSDFEvent = lastBSDFEvent;
	BSDF bsdf;
	bool illuminated = false;
	for (;;) {
		RayHit rayHit;
		Spectrum connectionThroughput;
		if (!scene->Intersect(device, false, rndGen->floatValue(),
				&ray, &rayHit, &bsdf, &connectionThroughput)) {
			// Nothing was hit, look for infinitelight
			illuminated |= DirectHitInfiniteLight(false, lastBSDFEvent, pathBSDFEvent,
					pathThroughput * connectionThroughput, ray.d,	lastPdfW, sampleResult);
			break;
		}
		pathThroughput *= connectionThroughput;

		// Something was hit

		// Check if it is visible in indirect paths
		if (((pathBSDFEvent & DIFFUSE) && !bsdf.IsVisibleIndirectDiffuse()) ||
				((pathBSDFEvent & GLOSSY) && !bsdf.IsVisibleIndirectGlossy()) ||
				((pathBSDFEvent & SPECULAR) && !bsdf.IsVisibleIndirectSpecular()))
			break;

		// Check if it is a light source
		if (bsdf.IsLightSource() && (rayHit.t > engine->nearStartLight)) {
			illuminated |= DirectHitFiniteLight(false, lastBSDFEvent, pathBSDFEvent,
					pathThroughput, rayHit.t, bsdf, lastPdfW, sampleResult);
		}

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		if (!bsdf.IsDelta())
			illuminated |= DirectLightSamplingONE(rndGen, false, pathBSDFEvent, pathThroughput, bsdf, sampleResult);

		//----------------------------------------------------------------------
		// Build the next vertex path ray
		//----------------------------------------------------------------------

		Vector sampledDir;
		float cosSampledDir;
		const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
				rndGen->floatValue(),
				rndGen->floatValue(),
				&lastPdfW, &cosSampledDir, &lastBSDFEvent);
		if (bsdfSample.Black())// || (lastPdfW < engine->pdfRejectValue))
			break;

		// Check if I have to stop because of path depth
		depthInfo.IncDepths(lastBSDFEvent);
		if (!depthInfo.CheckDepths(engine->maxPathDepth))
			break;

		pathThroughput *= bsdfSample * (cosSampledDir / lastPdfW);
		assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

		ray = Ray(bsdf.hitPoint.p, sampledDir);
	}

	return illuminated;
}

// NOTE: bsdf.hitPoint.passThroughEvent is modified by this method
void BiasPathCPURenderThread::SampleComponent(RandomGenerator *rndGen,
		const BSDFEvent requestedEventTypes, const u_int size,
		const luxrays::Spectrum &pathThroughput, BSDF &bsdf,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;

	const float scaleFactor = 1.f / (size * size);
	for (u_int sampleY = 0; sampleY < size; ++sampleY) {
		for (u_int sampleX = 0; sampleX < size; ++sampleX) {
			float u0, u1;
			SampleGrid(rndGen, size, sampleX, sampleY, &u0, &u1);
			bsdf.hitPoint.passThroughEvent = rndGen->floatValue();

			Vector sampledDir;
			BSDFEvent event;
			float pdfW, cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir, u0, u1,
					&pdfW, &cosSampledDir, &event, requestedEventTypes);
			if (bsdfSample.Black())// || (pdfW < engine->pdfRejectValue))
				continue;

			// Check if I have to stop because of path depth
			PathDepthInfo depthInfo;
			depthInfo.IncDepths(event);
			if (!depthInfo.CheckDepths(engine->maxPathDepth))
				continue;

			const Spectrum continuepathThroughput = pathThroughput * bsdfSample * (scaleFactor * cosSampledDir / pdfW);
			assert (!continuepathThroughput.IsNaN() && !continuepathThroughput.IsInf());

			Ray continueRay(bsdf.hitPoint.p, sampledDir);
			const bool illuminated = ContinueTracePath(rndGen, depthInfo, continueRay,
					continuepathThroughput, event, pdfW, sampleResult);

			if (!illuminated)
				sampleResult->indirectShadowMask += scaleFactor;
		}
	}
}

void BiasPathCPURenderThread::TraceEyePath(RandomGenerator *rndGen, const Ray &ray,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Ray eyeRay = ray;
	RayHit eyeRayHit;
	Spectrum pathThroughput(1.f);
	BSDF bsdf;
	if (!scene->Intersect(device, false, rndGen->floatValue(),
			&eyeRay, &eyeRayHit, &bsdf, &pathThroughput)) {
		// Nothing was hit, look for env. lights

		// SPECULAR is required to avoid MIS
		DirectHitInfiniteLight(true, SPECULAR, NONE, pathThroughput, eyeRay.d,
				1.f, sampleResult);

		sampleResult->alpha = 0.f;
		sampleResult->depth = std::numeric_limits<float>::infinity();
		sampleResult->position = Point(
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
		sampleResult->geometryNormal = Normal(
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
		sampleResult->shadingNormal = Normal(
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
		sampleResult->materialID = std::numeric_limits<u_int>::max();
		sampleResult->uv = UV(std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
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
			DirectHitFiniteLight(true, SPECULAR, NONE, pathThroughput,
					eyeRayHit.t, bsdf, 1.f, sampleResult);
		}

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		if (!bsdf.IsDelta()) {
			if (engine->lightSamplingStrategyONE)
				DirectLightSamplingONE(rndGen, true, NONE, pathThroughput, bsdf, sampleResult);
			else
				DirectLightSamplingALL(rndGen, pathThroughput, bsdf, sampleResult);
		}

		//----------------------------------------------------------------------
		// Split the path
		//----------------------------------------------------------------------

		const BSDFEvent materialEventTypes = bsdf.GetEventTypes();
		int materialSamples = bsdf.GetSamples();

		//----------------------------------------------------------------------
		// Sample the diffuse component
		//
		// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
		//----------------------------------------------------------------------

		if ((engine->maxPathDepth.diffuseDepth > 0) && (materialEventTypes & DIFFUSE)) {
			const u_int diffuseSamples = (materialSamples < 0) ? engine->diffuseSamples : ((u_int)materialSamples);

			if (diffuseSamples > 0) {
				SampleComponent(rndGen, DIFFUSE | REFLECT | TRANSMIT,
						diffuseSamples, pathThroughput, bsdf, sampleResult);
			}
		}

		//----------------------------------------------------------------------
		// Sample the glossy component
		//
		// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
		//----------------------------------------------------------------------

		if ((engine->maxPathDepth.glossyDepth > 0) && (materialEventTypes & GLOSSY)) {
			const u_int glossySamples = (materialSamples < 0) ? engine->glossySamples : ((u_int)materialSamples);

			if (glossySamples > 0) {
				SampleComponent(rndGen, GLOSSY | REFLECT | TRANSMIT, 
						glossySamples, pathThroughput, bsdf, sampleResult);
			}
		}

		//----------------------------------------------------------------------
		// Sample the specular component
		//
		// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
		//----------------------------------------------------------------------

		if ((engine->maxPathDepth.specularDepth > 0) && (materialEventTypes & SPECULAR)) {
			const u_int speculaSamples = (materialSamples < 0) ? engine->specularSamples : ((u_int)materialSamples);

			if (speculaSamples > 0) {
				SampleComponent(rndGen, SPECULAR | REFLECT | TRANSMIT,
						speculaSamples, pathThroughput, bsdf, sampleResult);
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
		Film::INDIRECT_SHADOW_MASK | Film::UV | Film::RAYCOUNT,
		engine->film->GetRadianceGroupCount());

	// Set to 0.0 all result colors
	sampleResult.emission = Spectrum();
	for (u_int i = 0; i < sampleResult.radiancePerPixelNormalized.size(); ++i)
		sampleResult.radiancePerPixelNormalized[i] = Spectrum();
	sampleResult.directDiffuse = Spectrum();
	sampleResult.directGlossy = Spectrum();
	sampleResult.indirectDiffuse = Spectrum();
	sampleResult.indirectGlossy = Spectrum();
	sampleResult.indirectSpecular = Spectrum();
	sampleResult.directShadowMask = 0.f;
	sampleResult.indirectShadowMask = 0.f;

	// To keep track of the number of rays traced
	const double deviceRayCount = device->GetTotalRaysCount();

	sampleResult.filmX = xOffset + x + .5f + u0;
	sampleResult.filmY = yOffset + y + .5f + u1;
	Ray eyeRay;
	engine->renderConfig->scene->camera->GenerateRay(sampleResult.filmX, sampleResult.filmY,
			&eyeRay, rndGen->floatValue(), rndGen->floatValue());

	// Trace the path
	TraceEyePath(rndGen, eyeRay, &sampleResult);

	// Radiance clamping
	for (u_int i = 0; i < sampleResult.radiancePerPixelNormalized.size(); ++i)
		sampleResult.radiancePerPixelNormalized[i] = sampleResult.radiancePerPixelNormalized[i].Clamp(0.f, engine->radianceClampMaxValue);

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
	while (engine->NextTile(&tile, tileFilm) && !interruptionRequested) {
		// Render the tile
		tileFilm->Reset();
		const u_int tileWidth = Min(engine->tileRepository->tileSize, filmWidth - tile->xStart);
		const u_int tileHeight = Min(engine->tileRepository->tileSize, filmHeight - tile->yStart);
		//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Tile: "
		//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
		//		"(" << tileWidth << ", " << tileHeight << ")");

		//----------------------------------------------------------------------
		// Render the tile
		//----------------------------------------------------------------------

		for (u_int y = 0; y < tileHeight && !interruptionRequested; ++y) {
			for (u_int x = 0; x < tileWidth && !interruptionRequested; ++x) {
				if (tile->sampleIndex >= 0) {
					const u_int sampleX = tile->sampleIndex % engine->aaSamples;
					const u_int sampleY = tile->sampleIndex / engine->aaSamples;
					RenderPixelSample(rndGen, x, y, tile->xStart, tile->yStart, sampleX, sampleY);
				} else {
					for (u_int sampleY = 0; sampleY < engine->aaSamples; ++sampleY) {
						for (u_int sampleX = 0; sampleX < engine->aaSamples; ++sampleX) {
							RenderPixelSample(rndGen, x, y, tile->xStart, tile->yStart, sampleX, sampleY);
						}
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
