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

void BiasPathCPURenderThread::SampleGrid(luxrays::RandomGenerator *rndGen, const u_int size,
		const u_int ix, const u_int iy, float *u0, float *u1) const {
	if (size == 1) {
		*u0 = rndGen->floatValue();
		*u1 = rndGen->floatValue();
	} else {
		const float idim = 1.f / size;
		*u0 = (ix + rndGen->floatValue()) * idim;
		*u1 = (iy + rndGen->floatValue()) * idim;
	}
}

bool BiasPathCPURenderThread::DirectLightSampling(
		const LightSource *light, const float lightPickPdf,
		const float u0, const float u1,
		const float u2, const float u3,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector lightRayDir;
	float distance, directPdfW;
	Spectrum lightRadiance = light->Illuminate(*scene, bsdf.hitPoint.p,
			u0, u1, u2, &lightRayDir, &distance, &directPdfW);

	if (!lightRadiance.Black() && (distance > engine->nearStartLight)) {
		BSDFEvent event;
		float bsdfPdfW;
		Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW);

		const float cosThetaToLight = AbsDot(lightRayDir, bsdf.hitPoint.shadeN);
		const float directLightSamplingPdfW = directPdfW * lightPickPdf;
		const float factor = cosThetaToLight / directLightSamplingPdfW;

		// MIS between direct light sampling and BSDF sampling
		const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

		const Spectrum illumRadiance = (weight * factor) * pathThrouput * lightRadiance * bsdfEval;

		if (illumRadiance.Y() > engine->lowLightThreashold) {
			const float epsilon = Max(MachineEpsilon::E(bsdf.hitPoint.p), MachineEpsilon::E(distance));
			Ray shadowRay(bsdf.hitPoint.p, lightRayDir,
					epsilon,
					distance - epsilon);
			RayHit shadowRayHit;
			BSDF shadowBsdf;
			Spectrum connectionThroughput;
			// Check if the light source is visible
			if (!scene->Intersect(device, false, u3, &shadowRay,
					&shadowRayHit, &shadowBsdf, &connectionThroughput))
				*radiance += connectionThroughput * illumRadiance;

			return true;
		}
	}

	return false;
}

void BiasPathCPURenderThread::DirectLightSamplingONE(
		RandomGenerator *rndGen,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Pick a light source to sample
	float lightPickPdf;
	const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lightPickPdf);

	DirectLightSampling(light, lightPickPdf, rndGen->floatValue(), rndGen->floatValue(),
			rndGen->floatValue(), rndGen->floatValue(), pathThrouput, bsdf, radiance);
}

void BiasPathCPURenderThread::DirectLightSamplingALL(
		RandomGenerator *rndGen,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	const u_int lightsSize = scene->GetLightCount();
	for (u_int i = 0; i < lightsSize; ++i) {
		const LightSource *light = scene->GetLightByIndex(i);
		const int samples = light->GetSamples();
		const u_int samplesToDo = (samples < 0) ? engine->directLightSamples : ((u_int)samples);

		Spectrum lightRadiance;
		u_int sampleCount = 0;
		for (u_int sampleY = 0; sampleY < samplesToDo; ++sampleY) {
			for (u_int sampleX = 0; sampleX < samplesToDo; ++sampleX) {
				float u0, u1;
				SampleGrid(rndGen, samplesToDo, sampleX, sampleY, &u0, &u1);

				if (DirectLightSampling(light, 1.f, u0, u1,
						rndGen->floatValue(), rndGen->floatValue(),
						pathThrouput, bsdf, &lightRadiance))
					++sampleCount;
			}
		}

		if (sampleCount > 0)
			*radiance += lightRadiance / sampleCount;
	}
}

void BiasPathCPURenderThread::DirectHitFiniteLight(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const float distance, const BSDF &bsdf, const float lastPdfW,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (emittedRadiance.Y() > engine->lowLightThreashold) {
		float weight;
		if (!lastSpecular) {
			// This PDF used for MIS is correct because lastSpecular is always
			// true when using DirectLightSamplingALL()
			const float lightPickProb = scene->SampleAllLightPdf(bsdf.GetLightSource());
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		*radiance +=  pathThrouput * weight * emittedRadiance;
	}
}

void BiasPathCPURenderThread::DirectHitInfiniteLight(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const Vector &eyeDir, const float lastPdfW, Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Infinite light
	float directPdfW;
	if (scene->envLight) {
		const Spectrum envRadiance = scene->envLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (envRadiance.Y() > engine->lowLightThreashold) {
			if(!lastSpecular) {
				// This PDF used for MIS is correct because lastSpecular is always
				// true when using DirectLightSamplingALL()

				// MIS between BSDF sampling and direct light sampling
				*radiance += pathThrouput * PowerHeuristic(lastPdfW, directPdfW) * envRadiance;
			} else
				*radiance += pathThrouput * envRadiance;
		}
	}

	// Sun light
	if (scene->sunLight) {
		const Spectrum sunRadiance = scene->sunLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (sunRadiance.Y() > engine->lowLightThreashold) {
			if(!lastSpecular) {
				// MIS between BSDF sampling and direct light sampling
				*radiance += pathThrouput * PowerHeuristic(lastPdfW, directPdfW) * sunRadiance;
			} else
				*radiance += pathThrouput * sunRadiance;
		}
	}
}

void BiasPathCPURenderThread::ContinueTracePath(RandomGenerator *rndGen,
		PathDepthInfo depthInfo, Ray ray,
		Spectrum pathThrouput, BSDFEvent lastBSDFEvent, float lastPdfW,
		luxrays::Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	BSDF bsdf;
	for (;;) {
		RayHit rayHit;
		Spectrum connectionThroughput;
		if (!scene->Intersect(device, false, rndGen->floatValue(),
				&ray, &rayHit, &bsdf, &connectionThroughput)) {
			// Nothing was hit, look for infinitelight
			DirectHitInfiniteLight(lastBSDFEvent & SPECULAR, pathThrouput * connectionThroughput, ray.d,
					lastPdfW, radiance);
			break;
		}
		pathThrouput *= connectionThroughput;

		// Something was hit

		// Check if it is visible in indirect paths
		if (((lastBSDFEvent & DIFFUSE) && !bsdf.IsVisibleIndirectDiffuse()) ||
				((lastBSDFEvent & GLOSSY) && !bsdf.IsVisibleIndirectGlossy()) ||
				((lastBSDFEvent & SPECULAR) && !bsdf.IsVisibleIndirectSpecular()))
			break;

		// Check if it is a light source
		if (bsdf.IsLightSource() && (rayHit.t > engine->nearStartLight)) {
			DirectHitFiniteLight(lastBSDFEvent & SPECULAR, pathThrouput,
					rayHit.t, bsdf, lastPdfW, radiance);
		}

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		if (!bsdf.IsDelta())
			DirectLightSamplingONE(rndGen, pathThrouput, bsdf, radiance);

		//----------------------------------------------------------------------
		// Build the next vertex path ray
		//----------------------------------------------------------------------

		Vector sampledDir;
		float cosSampledDir;
		const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
				rndGen->floatValue(),
				rndGen->floatValue(),
				&lastPdfW, &cosSampledDir, &lastBSDFEvent);
		if (bsdfSample.Y() <= engine->lowLightThreashold)
			break;

		// Check if I have to stop because of path depth
		depthInfo.IncDepths(lastBSDFEvent);
		if (!depthInfo.CheckDepths(engine->maxPathDepth))
			break;

		pathThrouput *= bsdfSample * (cosSampledDir / lastPdfW);
		assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

		ray = Ray(bsdf.hitPoint.p, sampledDir);
	}

	assert (!radiance->IsNaN() && !radiance->IsInf());
}

// NOTE: bsdf.hitPoint.passThroughEvent is modified by this method
luxrays::Spectrum BiasPathCPURenderThread::SampleComponent(luxrays::RandomGenerator *rndGen,
		 const BSDFEvent requestedEventTypes, const u_int size,
		const PathDepthInfo &baseDepthInfo, BSDF &bsdf) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;

	Spectrum radiance;
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
			if (bsdfSample.Y() <= engine->lowLightThreashold)
				continue;

			// Check if I have to stop because of path depth
			PathDepthInfo depthInfo = baseDepthInfo;
			depthInfo.IncDepths(event);
			if (!depthInfo.CheckDepths(engine->maxPathDepth))
				continue;

			const Spectrum continuePathThrouput = bsdfSample * (cosSampledDir / pdfW);
			assert (!continuePathThrouput.IsNaN() && !continuePathThrouput.IsInf());

			Ray continueRay(bsdf.hitPoint.p, sampledDir);
			ContinueTracePath(rndGen, depthInfo, continueRay,
					continuePathThrouput, event, pdfW, &radiance);
		}
	}

	return radiance / (size * size);
}

void BiasPathCPURenderThread::TraceEyePath(luxrays::RandomGenerator *rndGen, const Ray &ray,
		luxrays::Spectrum *radiance, float *alpha) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	*alpha = 1.f;

	Ray eyeRay = ray;

	PathDepthInfo depthInfo;
	bool lastSpecular = true;
	float lastPdfW = 1.f;
	Spectrum pathThrouput(1.f, 1.f, 1.f);
	BSDF bsdf;
	for (;;) {
		RayHit eyeRayHit;
		Spectrum connectionThroughput;
		if (!scene->Intersect(device, false, rndGen->floatValue(),
				&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput)) {
			// Nothing was hit, look for infinitelight
			DirectHitInfiniteLight(lastSpecular, pathThrouput * connectionThroughput, eyeRay.d,
					lastPdfW, radiance);

			if (depthInfo.depth == 1)
				*alpha = 0.f;
			break;
		}
		pathThrouput *= connectionThroughput;

		// Something was hit

		// Check if it is a light source
		if (bsdf.IsLightSource() && (eyeRayHit.t > engine->nearStartLight)) {
			DirectHitFiniteLight(lastSpecular, pathThrouput,
					eyeRayHit.t, bsdf, lastPdfW, radiance);
		}

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		if (!bsdf.IsDelta()) {
			if (engine->lightSamplingStrategyONE)
				DirectLightSamplingONE(rndGen, pathThrouput, bsdf, radiance);
			else
				DirectLightSamplingALL(rndGen, pathThrouput, bsdf, radiance);
		}

		//----------------------------------------------------------------------
		// Split the path
		//----------------------------------------------------------------------

		const BSDFEvent materialEventTypes = bsdf.GetEventTypes();
		if (bsdf.IsDelta() && ((materialEventTypes & (REFLECT | TRANSMIT)) != (REFLECT | TRANSMIT))) {
			// Continue to trace the initial path

			Vector sampledDir;
			BSDFEvent event;
			float cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
					rndGen->floatValue(),
					rndGen->floatValue(),
					&lastPdfW, &cosSampledDir, &event);
			if (bsdfSample.Y() <= engine->lowLightThreashold)
				break;

			// Check if I have to stop because of path depth
			depthInfo.IncDepths(event);
			if (!depthInfo.CheckDepths(engine->maxPathDepth))
				break;

			lastSpecular = (event & SPECULAR);

			pathThrouput *= bsdfSample * (cosSampledDir / lastPdfW);
			assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

			eyeRay = Ray(bsdf.hitPoint.p, sampledDir);
		} else {
			// Split the initial path

			int materialSamples = bsdf.GetSamples();

			//------------------------------------------------------------------
			// Sample the diffuse component
			//
			// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
			//------------------------------------------------------------------

			if ((engine->maxPathDepth.diffuseDepth > 0) && (materialEventTypes & DIFFUSE)) {
				const u_int diffuseSamples = (materialSamples < 0) ? engine->diffuseSamples : ((u_int)materialSamples);

				if (diffuseSamples > 0)
					*radiance += pathThrouput * SampleComponent(rndGen, DIFFUSE, diffuseSamples, depthInfo, bsdf);
			}

			//------------------------------------------------------------------
			// Sample the glossy component
			//
			// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
			//------------------------------------------------------------------

			if ((engine->maxPathDepth.glossyDepth > 0) && (materialEventTypes & GLOSSY)) {
				const u_int glossySamples = (materialSamples < 0) ? engine->glossySamples : ((u_int)materialSamples);

				if (glossySamples > 0)
					*radiance += pathThrouput * SampleComponent(rndGen, GLOSSY, glossySamples, depthInfo, bsdf);
			}

			//------------------------------------------------------------------
			// Sample the refraction component
			//
			// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
			//------------------------------------------------------------------

			if ((engine->maxPathDepth.specularDepth > 0) &&
					((materialEventTypes & (SPECULAR | REFLECT | TRANSMIT)) == (SPECULAR | REFLECT | TRANSMIT))) {
				const u_int speculaSamples = (materialSamples < 0) ? engine->specularSamples : ((u_int)materialSamples);

				if (speculaSamples > 0)
					*radiance += pathThrouput * SampleComponent(rndGen, SPECULAR | REFLECT | TRANSMIT, speculaSamples, depthInfo, bsdf);
			}

			break;
		}
	}

	assert (!radiance->IsNaN() && !radiance->IsInf());
	assert (!isnan(*alpha) && !isinf(*alpha));
}

void BiasPathCPURenderThread::RenderPixelSample(luxrays::RandomGenerator *rndGen,
		const FilterDistribution &filterDistribution,
		const u_int x, const u_int y,
		const u_int xOffset, const u_int yOffset,
		const u_int sampleX, const u_int sampleY) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;

	float u0, u1;
	SampleGrid(rndGen, engine->aaSamples, sampleX, sampleY, &u0, &u1);

	// Sample according the pixel filter distribution
	filterDistribution.SampleContinuous(u0, u1, &u0, &u1);

	const float screenX = xOffset + x + .5f + u0;
	const float screenY = yOffset + y + .5f + u1;
	Ray eyeRay;
	engine->renderConfig->scene->camera->GenerateRay(screenX, screenY, &eyeRay,
		rndGen->floatValue(), rndGen->floatValue());

	// Trace the path
	Spectrum radiance;
	float alpha;
	TraceEyePath(rndGen, eyeRay, &radiance, &alpha);

	// Clamping
	if (engine->clampValueEnabled)
		radiance = radiance.Clamp(0.f, engine->clampMaxValue);

	tileFilm->AddSampleCount(1.0);
	tileFilm->AddSample(PER_PIXEL_NORMALIZED, x, y, u0, u1,
			radiance, alpha);
}

void BiasPathCPURenderThread::RenderFunc() {
	//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Film *film = engine->film;
	const Filter *filter = film->GetFilter();
	const u_int filmWidth = film->GetWidth();
	const u_int filmHeight = film->GetHeight();
	const FilterDistribution filterDistribution(filter, 64);

	//--------------------------------------------------------------------------
	// Extract the tile to render
	//--------------------------------------------------------------------------

	const BiasPathCPURenderEngine::Tile *tile = NULL;
	bool interruptionRequested = boost::this_thread::interruption_requested();
	while ((tile = engine->NextTile(tile, tileFilm)) && !interruptionRequested) {
		// Render the tile
		tileFilm->Reset();
		const u_int tileWidth = Min(engine->tileSize, filmWidth - tile->xStart);
		const u_int tileHeight = Min(engine->tileSize, filmHeight - tile->yStart);
		//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Tile: "
		//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
		//		"(" << tileWidth << ", " << tileHeight << ")");

		for (u_int y = 0; y < tileHeight && !interruptionRequested; ++y) {
			for (u_int x = 0; x < tileWidth && !interruptionRequested; ++x) {
				if (tile->sampleIndex >= 0) {
					const u_int sampleX = tile->sampleIndex % engine->aaSamples;
					const u_int sampleY = tile->sampleIndex / engine->aaSamples;
					RenderPixelSample(rndGen, filterDistribution, x, y,
							tile->xStart, tile->yStart, sampleX, sampleY);
				} else {
					for (u_int sampleY = 0; sampleY < engine->aaSamples; ++sampleY) {
						for (u_int sampleX = 0; sampleX < engine->aaSamples; ++sampleX) {
							RenderPixelSample(rndGen, filterDistribution, x, y,
									tile->xStart, tile->yStart, sampleX, sampleY);
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
