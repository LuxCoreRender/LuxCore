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

#include <algorithm>

#include "slg/slg.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathhybrid/pathhybrid.h"

#include "luxrays/core/intersectiondevice.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathState
//------------------------------------------------------------------------------

const u_int sampleBootSize = 4;
const u_int sampleStepSize = 9;

PathHybridState::PathHybridState(PathHybridRenderThread *renderThread,
		Film *film, luxrays::RandomGenerator *rndGen) : HybridRenderState(renderThread, film, rndGen), sampleResults(1) {
	PathHybridRenderEngine *renderEngine = (PathHybridRenderEngine *)renderThread->renderEngine;

	sampleResults[0].Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA, 1);

	// Setup the sampler
	const u_int sampleSize = sampleBootSize + renderEngine->maxPathDepth * sampleStepSize;
	sampler->RequestSamples(sampleSize);

	Init(renderThread);
}

void PathHybridState::Init(const PathHybridRenderThread *thread) {
	PathHybridRenderEngine *renderEngine = (PathHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	depth = 1;
	lastPdfW = 1.f;
	throuput = Spectrum(1.f);

	directLightRadiance = Spectrum();

	// Initialize eye ray
	Camera *camera = scene->camera;
	Film *film = thread->threadFilm;
	const u_int filmWidth = film->GetWidth();
	const u_int filmHeight = film->GetHeight();

	sampleResults[0].filmX = std::min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
	sampleResults[0].filmY = std::min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
	camera->GenerateRay(sampleResults[0].filmX, sampleResults[0].filmY, &nextPathVertexRay,
		sampler->GetSample(2), sampler->GetSample(3), 0.f);

	sampleResults[0].alpha = 1.f;
	sampleResults[0].radiancePerPixelNormalized[0] = Spectrum(0.f);
	lastSpecular = true;
}


void PathHybridState::DirectHitInfiniteLight(const Scene *scene, const Vector &eyeDir) {
	BOOST_FOREACH(EnvLightSource *el, scene->lightDefs.GetEnvLightSources()) {
		float directPdfW;
		const Spectrum envRadiance = el->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (!envRadiance.Black()) {
			if(!lastSpecular) {
				// MIS between BSDF sampling and direct light sampling
				sampleResults[0].radiancePerPixelNormalized[0] += throuput * PowerHeuristic(lastPdfW, directPdfW) * envRadiance;
			} else
				sampleResults[0].radiancePerPixelNormalized[0] += throuput * envRadiance;
		}
	}
}

void PathHybridState::DirectHitFiniteLight(const Scene *scene, const float distance, const BSDF &bsdf) {
	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!lastSpecular) {
			const float lightPickProb = scene->lightDefs.GetLightStrategy()->SampleLightPdf(bsdf.GetLightSource());
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		sampleResults[0].radiancePerPixelNormalized[0] +=  throuput * weight * emittedRadiance;
	}
}

void PathHybridState::DirectLightSampling(const PathHybridRenderThread *renderThread,
		const float u0, const float u1, const float u2,
		const float u3, const BSDF &bsdf) {
	if (!bsdf.IsDelta()) {
		PathHybridRenderEngine *renderEngine = (PathHybridRenderEngine *)renderThread->renderEngine;
		Scene *scene = renderEngine->renderConfig->scene;

		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = scene->lightDefs.GetLightStrategy()->SampleLights(u0, &lightPickPdf);

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
				directLightRay = Ray(bsdf.hitPoint.p, lightRayDir,
						epsilon, distance - epsilon);

				const float directLightSamplingPdfW = directPdfW * lightPickPdf;
				const float factor = 1.f / directLightSamplingPdfW;

				if (depth >= renderEngine->rrDepth) {
					// Russian Roulette
					bsdfPdfW *= RenderEngine::RussianRouletteProb(bsdfEval, renderEngine->rrImportanceCap);
				}

				// MIS between direct light sampling and BSDF sampling
				const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

				directLightRadiance = (weight * factor) * throuput * lightRadiance * bsdfEval;
			} else
				directLightRadiance = Spectrum();
		} else
			directLightRadiance = Spectrum();
	} else
		directLightRadiance = Spectrum();
}

bool PathHybridState::FinalizeRay(const PathHybridRenderThread *renderThread,
		const Ray *ray, const RayHit *rayHit, BSDF *bsdf, const float u0, Spectrum *radiance) {
	if (rayHit->Miss())
		return true;
	else {
		PathHybridRenderEngine *renderEngine = (PathHybridRenderEngine *)renderThread->renderEngine;
		Scene *scene = renderEngine->renderConfig->scene;

		// I have to check if it is a hit over a pass-through point
		bsdf->Init(false, *scene, *ray, *rayHit, u0, NULL);

		// Check if it is a pass-through point
		Spectrum t = bsdf->GetPassThroughTransparency();
		if (!t.Black()) {
			*radiance *= t;

			// It is a pass-through material, continue to trace the ray. I do
			// this on the CPU.

			Ray newRay(*ray);
			newRay.mint = rayHit->t + MachineEpsilon::E(rayHit->t);
			RayHit newRayHit;			
			Spectrum connectionThroughput;
			if (scene->Intersect(renderThread->device, false, u0, &newRay, &newRayHit,
					bsdf, &connectionThroughput)) {
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

void PathHybridState::SplatSample(const PathHybridRenderThread *renderThread) {
	sampler->NextSample(sampleResults);

	Init(renderThread);
}

void PathHybridState::GenerateRays(HybridRenderThread *renderThread) {
	PathHybridRenderThread *thread = (PathHybridRenderThread *)renderThread;

	if (!directLightRadiance.Black())
		thread->PushRay(directLightRay);
	thread->PushRay(nextPathVertexRay);
}

double PathHybridState::CollectResults(HybridRenderThread *renderThread) {
	PathHybridRenderThread *thread = (PathHybridRenderThread *)renderThread;
	PathHybridRenderEngine *renderEngine = (PathHybridRenderEngine *)thread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	// Was a shadow ray traced ?
	if (!directLightRadiance.Black()) {
		// It was, check the result
		const Ray *shadowRay;
		const RayHit *shadowRayHit;
		thread->PopRay(&shadowRay, &shadowRayHit);

		BSDF bsdf;
		// "(depth - 1 - 1)" is used because it is a shadow ray of previous path vertex
		const unsigned int sampleOffset = sampleBootSize + (depth - 1 - 1) * sampleStepSize;
		const bool miss = FinalizeRay(thread, shadowRay, shadowRayHit, &bsdf,
				sampler->GetSample(sampleOffset + 5), &directLightRadiance);
		if (miss) {
			// The light source is visible, add the direct light sampling component
			sampleResults[0].radiancePerPixelNormalized[0] += directLightRadiance;
		}
	}

	// The next path vertex ray
	const Ray *ray;
	const RayHit *rayHit;
	thread->PopRay(&ray, &rayHit);

	BSDF bsdf;
	const unsigned int sampleOffset = sampleBootSize + (depth - 1) * sampleStepSize;
	const bool miss = FinalizeRay(thread, ray, rayHit, &bsdf, sampler->GetSample(sampleOffset),
			&throuput);

	if (miss) {
		// Nothing was hit, look for infinitelight
		DirectHitInfiniteLight(scene, ray->d);

		if (depth == 1)
			sampleResults[0].alpha = 0.f;

		SplatSample(thread);
		return 1.0;
	} else {
		// Something was hit

		// Check if a light source was hit
		if (bsdf.IsLightSource())
			DirectHitFiniteLight(scene, rayHit->t, bsdf);

		// Direct light sampling (initialize directLightRay)
		DirectLightSampling(thread,
				sampler->GetSample(sampleOffset + 1),
				sampler->GetSample(sampleOffset + 2),
				sampler->GetSample(sampleOffset + 3),
				sampler->GetSample(sampleOffset + 4),
				bsdf);

		// Build the next vertex path ray
		Vector sampledDir;
		BSDFEvent event;
		float cosSampledDir;
		const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
				sampler->GetSample(sampleOffset + 6),
				sampler->GetSample(sampleOffset + 7),
				&lastPdfW, &cosSampledDir, &event);
		if (bsdfSample.Black()) {
			SplatSample(thread);
			return 1.0;
		} else {
			lastSpecular = ((event & SPECULAR) != 0);

			if ((depth >= renderEngine->rrDepth) && !lastSpecular) {
				// Russian Roulette
				const float prob = RenderEngine::RussianRouletteProb(bsdfSample, renderEngine->rrImportanceCap);
				if (sampler->GetSample(sampleOffset + 8) < prob)
					lastPdfW *= prob;
				else {
					SplatSample(thread);
					return 1.0;
				}
			}

			++depth;

			// Check if I have reached the max. path depth
			if (depth > renderEngine->maxPathDepth) {
				SplatSample(thread);
				return 1.0;
			} else {
				throuput *= bsdfSample;
				assert (!throuput.IsNaN() && !throuput.IsInf());

				nextPathVertexRay = Ray(bsdf.hitPoint.p, sampledDir);
			}
		}
	}

	return 0.0;
}
