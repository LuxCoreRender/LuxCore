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

void BiasPathCPURenderThread::DirectLightSampling(
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	
	if (!bsdf.IsDelta()) {
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->SampleAllLights(u0, &lightPickPdf);

		Vector lightRayDir;
		float distance, directPdfW;
		Spectrum lightRadiance = light->Illuminate(*scene, bsdf.hitPoint.p,
				u1, u2, u3, &lightRayDir, &distance, &directPdfW);

		if (!lightRadiance.Black()) {
			BSDFEvent event;
			float bsdfPdfW;
			Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW);

			if (!bsdfEval.Black()) {
				const float epsilon = Max(MachineEpsilon::E(bsdf.hitPoint.p), MachineEpsilon::E(distance));
				Ray shadowRay(bsdf.hitPoint.p, lightRayDir,
						epsilon,
						distance - epsilon);
				RayHit shadowRayHit;
				BSDF shadowBsdf;
				Spectrum connectionThroughput;
				// Check if the light source is visible
				if (!scene->Intersect(device, false, u4, &shadowRay,
						&shadowRayHit, &shadowBsdf, &connectionThroughput)) {
					const float cosThetaToLight = AbsDot(lightRayDir, bsdf.hitPoint.shadeN);
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;
					const float factor = cosThetaToLight / directLightSamplingPdfW;

					// MIS between direct light sampling and BSDF sampling
					const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

					*radiance += (weight * factor) * pathThrouput * connectionThroughput * lightRadiance * bsdfEval;
				}
			}
		}
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

	if (!emittedRadiance.Black()) {
		float weight;
		if (!lastSpecular) {
			const float lightPickProb = scene->PickLightPdf();
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
		if (!envRadiance.Black()) {
			if(!lastSpecular) {
				// MIS between BSDF sampling and direct light sampling
				*radiance += pathThrouput * PowerHeuristic(lastPdfW, directPdfW) * envRadiance;
			} else
				*radiance += pathThrouput * envRadiance;
		}
	}

	// Sun light
	if (scene->sunLight) {
		const Spectrum sunRadiance = scene->sunLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (!sunRadiance.Black()) {
			if(!lastSpecular) {
				// MIS between BSDF sampling and direct light sampling
				*radiance += pathThrouput * PowerHeuristic(lastPdfW, directPdfW) * sunRadiance;
			} else
				*radiance += pathThrouput * sunRadiance;
		}
	}
}

void BiasPathCPURenderThread::ContinueTracePath(RandomGenerator *rndGen, int depth, Ray ray,
		Spectrum pathThrouput, float lastPdfW, bool lastSpecular,
		luxrays::Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	BSDF bsdf;
	while (depth <= engine->maxPathDepth) {
		RayHit rayHit;
		Spectrum connectionThroughput;
		if (!scene->Intersect(device, false, rndGen->floatValue(),
				&ray, &rayHit, &bsdf, &connectionThroughput)) {
			// Nothing was hit, look for infinitelight
			DirectHitInfiniteLight(lastSpecular, pathThrouput * connectionThroughput, ray.d,
					lastPdfW, radiance);
			break;
		}
		pathThrouput *= connectionThroughput;

		// Something was hit

		// Check if it is a light source
		if (bsdf.IsLightSource()) {
			DirectHitFiniteLight(lastSpecular, pathThrouput,
					rayHit.t, bsdf, lastPdfW, radiance);
		}

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		DirectLightSampling(rndGen->floatValue(),
				rndGen->floatValue(),
				rndGen->floatValue(),
				rndGen->floatValue(),
				rndGen->floatValue(),
				pathThrouput, bsdf, radiance);

		//----------------------------------------------------------------------
		// Build the next vertex path ray
		//----------------------------------------------------------------------

		Vector sampledDir;
		BSDFEvent event;
		float cosSampledDir;
		const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
				rndGen->floatValue(),
				rndGen->floatValue(),
				&lastPdfW, &cosSampledDir, &event);
		if (bsdfSample.Black())
			break;

		lastSpecular = ((event & SPECULAR) != 0);

		pathThrouput *= bsdfSample * (cosSampledDir / lastPdfW);
		assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

		ray = Ray(bsdf.hitPoint.p, sampledDir);
		++depth;
	}

	assert (!radiance->IsNaN() && !radiance->IsInf());
}

luxrays::Spectrum BiasPathCPURenderThread::SampleComponent(luxrays::RandomGenerator *rndGen,
		 const BSDFEvent requestedEventTypes, const u_int size, const int depth, const BSDF &bsdf) {
	Spectrum radiance;

	for (u_int sampleY = 0; sampleY < size; ++sampleY) {
		for (u_int sampleX = 0; sampleX < size; ++sampleX) {
			float u0, u1;
			SampleGrid(rndGen, size, sampleX, sampleY, &u0, &u1);

			Vector sampledDir;
			BSDFEvent event;
			float pdfW, cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
					rndGen->floatValue(),
					rndGen->floatValue(),
					&pdfW, &cosSampledDir, &event, requestedEventTypes);
			if (bsdfSample.Black())
				continue;

			const bool continueLastSpecular = ((event & SPECULAR) != 0);
			const Spectrum continuePathThrouput = bsdfSample * (cosSampledDir / pdfW);
			assert (!continuePathThrouput.IsNaN() && !continuePathThrouput.IsInf());

			Ray continueRay(bsdf.hitPoint.p, sampledDir);
			ContinueTracePath(rndGen, depth + 1, continueRay, continuePathThrouput,
					pdfW, continueLastSpecular, &radiance);
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

	int depth = 1;
	bool lastSpecular = true;
	float lastPdfW = 1.f;
	Spectrum pathThrouput(1.f, 1.f, 1.f);
	BSDF bsdf;
	while (depth <= engine->maxPathDepth) {
		RayHit eyeRayHit;
		Spectrum connectionThroughput;
		if (!scene->Intersect(device, false, rndGen->floatValue(),
				&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput)) {
			// Nothing was hit, look for infinitelight
			DirectHitInfiniteLight(lastSpecular, pathThrouput * connectionThroughput, eyeRay.d,
					lastPdfW, radiance);

			if (depth == 1)
				*alpha = 0.f;
			break;
		}
		pathThrouput *= connectionThroughput;

		// Something was hit

		// Check if it is a light source
		if (bsdf.IsLightSource()) {
			DirectHitFiniteLight(lastSpecular, pathThrouput,
					eyeRayHit.t, bsdf, lastPdfW, radiance);
		}

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		DirectLightSampling(rndGen->floatValue(),
				rndGen->floatValue(),
				rndGen->floatValue(),
				rndGen->floatValue(),
				rndGen->floatValue(),
				pathThrouput, bsdf, radiance);

		//----------------------------------------------------------------------
		// Split the path
		//----------------------------------------------------------------------

		if (bsdf.IsDelta()) {
			// Continue to trace the initial path

			Vector sampledDir;
			BSDFEvent event;
			float cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
					rndGen->floatValue(),
					rndGen->floatValue(),
					&lastPdfW, &cosSampledDir, &event);
			if (bsdfSample.Black())
				break;

			lastSpecular = ((event & SPECULAR) != 0);

			pathThrouput *= bsdfSample * (cosSampledDir / lastPdfW);
			assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

			eyeRay = Ray(bsdf.hitPoint.p, sampledDir);
			++depth;
		} else {
			// Split the initial path
			const BSDFEvent materialEventTypes = bsdf.GetEventTypes();

			//------------------------------------------------------------------
			// Sample the diffuse component
			//------------------------------------------------------------------

			if ((engine->diffuseSamples > 0) && ((materialEventTypes & (DIFFUSE | REFLECT)) == (DIFFUSE | REFLECT)))
				*radiance += pathThrouput * SampleComponent(rndGen, DIFFUSE | REFLECT, engine->diffuseSamples, depth, bsdf);

			//------------------------------------------------------------------
			// Sample the glossy component
			//------------------------------------------------------------------

			if ((engine->glossySamples > 0) && ((materialEventTypes & (GLOSSY | REFLECT)) == (GLOSSY | REFLECT)))
				*radiance += pathThrouput * SampleComponent(rndGen, GLOSSY | REFLECT, engine->glossySamples, depth, bsdf);

			//------------------------------------------------------------------
			// Sample the refraction component
			//------------------------------------------------------------------

			if ((engine->refractionSamples > 0) && (materialEventTypes & TRANSMIT))
				*radiance += pathThrouput * SampleComponent(rndGen, TRANSMIT, engine->refractionSamples, depth, bsdf);

			break;
		}
	}

	assert (!radiance->IsNaN() && !radiance->IsInf());
	assert (!isnan(*alpha) && !isinf(*alpha));
}

void BiasPathCPURenderThread::RenderFunc() {
	//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
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
				for (u_int sampleY = 0; sampleY < engine->aaSamples && !interruptionRequested; ++sampleY) {
					for (u_int sampleX = 0; sampleX < engine->aaSamples && !interruptionRequested; ++sampleX) {
						float u0, u1;
						SampleGrid(rndGen, engine->aaSamples, sampleX, sampleY, &u0, &u1);

						// Sample according the pixel filter distribution
						filterDistribution.SampleContinuous(u0, u1, &u0, &u1);

						const float screenX = tile->xStart + x + .5f + u0;
						const float screenY = tile->yStart + y + .5f + u1;
						Ray eyeRay;
						camera->GenerateRay(screenX, screenY, &eyeRay,
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

						interruptionRequested = boost::this_thread::interruption_requested();

#ifdef WIN32
						// Work around Windows bad scheduling
						renderThread->yield();
#endif
					}
				}
			}
		}
	}

	delete rndGen;

	//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
