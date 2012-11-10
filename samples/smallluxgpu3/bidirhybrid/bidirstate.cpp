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

#include "smalllux.h"
#include "renderconfig.h"
#include "bidirhybrid/bidirhybrid.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/opencl/utils.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

//------------------------------------------------------------------------------
// BiDirState
//------------------------------------------------------------------------------

static inline float MIS(const float a) {
	//return a; // Balance heuristic
	return a * a; // Power heuristic
}

BiDirState::BiDirState(BiDirHybridRenderEngine *renderEngine,
		Film *film, RandomGenerator *rndGen) {
	// Setup the sampler
	sampler = renderEngine->renderConfig->AllocSampler(rndGen, film);
	const unsigned int sampleBootSize = 11;
	const unsigned int sampleLightStepSize = 6;
	const unsigned int sampleEyeStepSize = 11;
	const unsigned int sampleSize = 
		sampleBootSize + // To generate the initial light vertex and trace eye ray
		renderEngine->maxLightPathDepth * sampleLightStepSize + // For each light vertex
		renderEngine->maxEyePathDepth * sampleEyeStepSize; // For each eye vertex
	sampler->RequestSamples(sampleSize);
}

BiDirState::~BiDirState() {
	delete sampler;
}

void BiDirState::ConnectToEye(BiDirHybridRenderThread *renderThread,
		const unsigned int pixelCount, 
		const PathVertex &lightVertex, const float u0,
		const Point &lensPoint) {
	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

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
			if (lightVertex.depth >= renderEngine->rrDepth) {
				// Russian Roulette
				const float prob = Max(bsdfEval.Filter(), renderEngine->rrImportanceCap);
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
			const float weight = 1.f / (MIS(cameraPdfA / pixelCount) *
				(lightVertex.d0 + lightVertex.d1vc * MIS(bsdfRevPdfW)) + 1.f); // Balance heuristic (MIS)
			const Spectrum radiance = weight * lightVertex.throughput * fluxToRadianceFactor * bsdfEval;

			// add the ray to trace and the result
			sampleValue.push_back(u0);
			renderThread->PushRay(eyeRay);
			AddSampleResult(sampleResults, PER_SCREEN_NORMALIZED, scrX, scrY,
					radiance, 1.f);
		}
	}
}

void BiDirState::GenerateRays(BiDirHybridRenderThread *renderThread) {
	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = renderThread->threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();
	const unsigned int pixelCount = filmWidth * filmHeight;

	const unsigned int sampleBootSize = 11;
	const unsigned int sampleLightStepSize = 6;
	//const unsigned int sampleEyeStepSize = 11;

	sampleResults.clear();
	vector<PathVertex> lightPathVertices;
	vector<PathVertex> eyePathVertices;

	// Sample a point on the camera lens
	Point lensPoint;
	if (!camera->SampleLens(sampler->GetSample(3), sampler->GetSample(4),
			&lensPoint))
		return;

	//----------------------------------------------------------------------
	// Trace light path
	//----------------------------------------------------------------------

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->SampleAllLights(sampler->GetSample(2), &lightPickPdf);

	// Initialize the light path
	PathVertex lightVertex;
	float lightEmitPdfW, lightDirectPdfW, cosThetaAtLight;
	Ray nextEventRay;
	lightVertex.throughput = light->Emit(scene,
		sampler->GetSample(5), sampler->GetSample(6), sampler->GetSample(7), sampler->GetSample(8),
		&nextEventRay.o, &nextEventRay.d, &lightEmitPdfW, &lightDirectPdfW, &cosThetaAtLight);
	if (!lightVertex.throughput.Black()) {
		lightEmitPdfW *= lightPickPdf;
		lightDirectPdfW *= lightPickPdf;

		lightVertex.throughput /= lightEmitPdfW;
		assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

		// I don't store the light vertex 0 because direct lighting will take
		// care of this kind of paths
		lightVertex.d0 = MIS(lightDirectPdfW / lightEmitPdfW);
		const float usedCosLight = light->IsEnvironmental() ? 1.f : cosThetaAtLight;
		lightVertex.d1vc = MIS(usedCosLight / lightEmitPdfW);

		lightVertex.depth = 1;
		while (lightVertex.depth <= renderEngine->maxLightPathDepth) {
			const unsigned int sampleOffset = sampleBootSize + (lightVertex.depth - 1) * sampleLightStepSize;

			RayHit nextEventRayHit;
			Spectrum connectionThroughput;
			if (scene->Intersect(true, true, sampler->GetSample(sampleOffset),
					&nextEventRay, &nextEventRayHit, &lightVertex.bsdf, &connectionThroughput)) {
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
					lightVertex.d0 *= MIS(nextEventRayHit.t * nextEventRayHit.t);
				const float factor = MIS(1.f / AbsDot(lightVertex.bsdf.shadeN, nextEventRay.d));
				lightVertex.d0 *= factor;
				lightVertex.d1vc *= factor;

				// Store the vertex only if it isn't specular
				if (!lightVertex.bsdf.IsDelta()) {
					lightPathVertices.push_back(lightVertex);

					//------------------------------------------------------
					// Try to connect the light path vertex with the eye
					//------------------------------------------------------
					ConnectToEye(renderThread, pixelCount, lightVertex, sampler->GetSample(sampleOffset + 1), lensPoint);
				}

				if (lightVertex.depth >= renderEngine->maxLightPathDepth)
					break;

				//----------------------------------------------------------
				// Build the next vertex path ray
				//----------------------------------------------------------

				Vector sampledDir;
				BSDFEvent event;
				float bsdfPdfW, cosSampledDir;
				const Spectrum bsdfSample = lightVertex.bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 2),
						sampler->GetSample(sampleOffset + 3),
						sampler->GetSample(sampleOffset + 4),
						&bsdfPdfW, &cosSampledDir, &event);
				if (bsdfSample.Black())
					break;

				float bsdfRevPdfW;
				if (event & SPECULAR)
					bsdfRevPdfW = bsdfPdfW;
				else
					lightVertex.bsdf.Pdf(sampledDir, NULL, &bsdfRevPdfW);

				if (lightVertex.depth >= renderEngine->rrDepth) {
					// Russian Roulette
					const float prob = Max(bsdfSample.Filter(), renderEngine->rrImportanceCap);
					if (sampler->GetSample(sampleOffset + 5) < prob) {
						bsdfPdfW *= prob;
						bsdfRevPdfW *= prob;
					} else
						break;
				}

				lightVertex.throughput *= bsdfSample * (cosSampledDir / bsdfPdfW);
				assert (!lightVertex.throughput.IsNaN() && !lightVertex.throughput.IsInf());

				// New MIS weights
				if (event & SPECULAR) {
					lightVertex.d0 = 0.f;
					lightVertex.d1vc *= (cosSampledDir / bsdfPdfW) * bsdfRevPdfW;
				} else {
					lightVertex.d1vc = (cosSampledDir / bsdfPdfW) * (lightVertex.d1vc *
							bsdfRevPdfW + lightVertex.d0);
					lightVertex.d0 = 1.f / bsdfPdfW;
				}

				nextEventRay = Ray(lightVertex.bsdf.hitPoint, sampledDir);
				++(lightVertex.depth);
			} else {
				// Ray lost in space...
				break;
			}
		}
	}
}

void BiDirState::CollectResults(BiDirHybridRenderThread *renderThread) {
	if (sampleResults.size() == 0)
		sampler->NextSample(sampleResults);

	BiDirHybridRenderEngine *renderEngine = renderThread->renderEngine;
	Scene *scene = renderEngine->renderConfig->scene;

	// Elaborate the RayHit results

	vector<bool> validResults(sampleResults.size());
	for (u_int i = 0; i < sampleResults.size(); ++i) {
		const Ray *ray;
		const RayHit *rayHit;
		renderThread->PopRay(&ray, &rayHit);

		if (rayHit->Miss())
			validResults[i] = true;
		else {
			// I have to check if it is an hit over a pass-through point
			BSDF bsdf(false, // true or false, here, doesn't really matter
					*scene, *ray, *rayHit, sampleValue[i]);

			// Check if it is pass-through point
			if (bsdf.IsPassThrough()) {
				// It is a pass-through material, continue to trace the ray. I do
				// this on the CPU.

				Ray newRay(*ray);
				newRay.mint = rayHit->t + MachineEpsilon::E(rayHit->t);
				RayHit newRayHit;
				Spectrum connectionThroughput;
				if (scene->Intersect(false, // true or false, here, doesn't really matter
						true, sampleValue[i], &newRay, &newRayHit, &bsdf, &connectionThroughput)) {
					// Something was hit
					validResults[i] = false;
				} else {
					sampleResults[i].radiance *= connectionThroughput;
					validResults[i] = true;
				}
			} else
				validResults[i] = false;
		}
	}

	// Splat only valid sample results
	vector<SampleResult> validSampleResults;
	for (u_int i = 0; i < sampleResults.size(); ++i) {
		if (validResults[i])
			validSampleResults.push_back(sampleResults[i]);
	}

	sampler->NextSample(validSampleResults);
}
