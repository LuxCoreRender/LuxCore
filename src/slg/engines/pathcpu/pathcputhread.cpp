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

#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/core/mc.h"

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
		const bool firstPathVertex, const BSDFEvent pathBSDFEvent,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		const int depth, SampleResult *sampleResult) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
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

					if (depth >= engine->rrDepth) {
						// Russian Roulette
						bsdfPdfW *= RenderEngine::RussianRouletteProb(bsdfEval, engine->rrImportanceCap);
					}

					// MIS between direct light sampling and BSDF sampling
					const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

					const Spectrum radiance = (weight * factor) * pathThroughput * connectionThroughput * lightRadiance * bsdfEval;
					sampleResult->radiancePerPixelNormalized[light->GetID()] += radiance;

					if (firstPathVertex) {
						sampleResult->directShadowMask = 0.f;

						if (bsdf.GetEventTypes() & DIFFUSE)
							sampleResult->directDiffuse += radiance;
						else
							sampleResult->directGlossy += radiance;
					} else {
						sampleResult->indirectShadowMask = 0.f;

						if (pathBSDFEvent & DIFFUSE)
							sampleResult->indirectDiffuse += radiance;
						else if (pathBSDFEvent & GLOSSY)
							sampleResult->indirectGlossy += radiance;
						else if (pathBSDFEvent & SPECULAR)
							sampleResult->indirectSpecular += radiance;
					}
				}
			}
		}
	} else {
		if (firstPathVertex)
			sampleResult->directShadowMask = 0.f;
	}
}

void PathCPURenderThread::DirectHitFiniteLight(const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent, const BSDFEvent pathBSDFEvent,
		const Spectrum &pathThroughput, const float distance, const BSDF &bsdf,
		const float lastPdfW, SampleResult *sampleResult) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!(lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = scene->SampleAllLightPdf(bsdf.GetLightSource());
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		const Spectrum radiance = weight * pathThroughput * emittedRadiance;
		sampleResult->radiancePerPixelNormalized[bsdf.GetLightID()] += radiance;

		if (firstPathVertex)
			sampleResult->emission += radiance;
		else {
			sampleResult->indirectShadowMask = 1.f;

			if (pathBSDFEvent & DIFFUSE)
				sampleResult->indirectDiffuse += radiance;
			else if (pathBSDFEvent & GLOSSY)
				sampleResult->indirectGlossy += radiance;
			else if (pathBSDFEvent & SPECULAR)
				sampleResult->indirectSpecular += radiance;
		}
	}
}

void PathCPURenderThread::DirectHitInfiniteLight(const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent, const BSDFEvent pathBSDFEvent,
		const Spectrum &pathThroughput, const Vector &eyeDir, const float lastPdfW,
		SampleResult *sampleResult) {
	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Infinite light
	float directPdfW;
	if (scene->envLight) {
		const Spectrum envRadiance = scene->envLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (!envRadiance.Black()) {
			float weight;
			if(!(lastBSDFEvent & SPECULAR)) {
				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(lastPdfW, directPdfW);
			} else
				weight = 1.f;

			const Spectrum radiance = weight * pathThroughput * envRadiance;
			sampleResult->radiancePerPixelNormalized[scene->envLight->GetID()] += radiance;

			if (firstPathVertex)
				sampleResult->emission += radiance;
			else {
				sampleResult->indirectShadowMask = 1.f;

				if (pathBSDFEvent & DIFFUSE)
					sampleResult->indirectDiffuse += radiance;
				else if (pathBSDFEvent & GLOSSY)
					sampleResult->indirectGlossy += radiance;
				else if (pathBSDFEvent & SPECULAR)
					sampleResult->indirectSpecular += radiance;
			}
		}
	}

	// Sun light
	if (scene->sunLight) {
		const Spectrum sunRadiance = scene->sunLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (!sunRadiance.Black()) {
			float weight;
			if(!(lastBSDFEvent & SPECULAR)) {
				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(lastPdfW, directPdfW);
			} else
				weight = 1.f;
				
			const Spectrum radiance = weight * pathThroughput * sunRadiance;
			sampleResult->radiancePerPixelNormalized[scene->sunLight->GetID()] += radiance;

			if (firstPathVertex)
				sampleResult->emission += radiance;
			else {
				sampleResult->indirectShadowMask = 1.f;

				if (pathBSDFEvent & DIFFUSE)
					sampleResult->indirectDiffuse += radiance;
				else if (pathBSDFEvent & GLOSSY)
					sampleResult->indirectGlossy += radiance;
				if (pathBSDFEvent & SPECULAR)
					sampleResult->indirectSpecular += radiance;
			}
		}
	}
}

void PathCPURenderThread::RenderFunc() {
	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Scene *scene = engine->renderConfig->scene;
	PerspectiveCamera *camera = scene->camera;
	Film *film = threadFilm;
	const unsigned int filmWidth = film->GetWidth();
	const unsigned int filmHeight = film->GetHeight();

	// Setup the sampler
	double metropolisSharedTotalLuminance, metropolisSharedSampleCount;
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, film,
			&metropolisSharedTotalLuminance, &metropolisSharedSampleCount);
	const unsigned int sampleBootSize = 4;
	const unsigned int sampleStepSize = 9;
	const unsigned int sampleSize = 
		sampleBootSize + // To generate eye ray
		engine->maxPathDepth * sampleStepSize; // For each path vertex
	sampler->RequestSamples(sampleSize);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH |
		Film::POSITION | Film::GEOMETRY_NORMAL | Film::SHADING_NORMAL | Film::MATERIAL_ID |
		Film::DIRECT_DIFFUSE | Film::DIRECT_GLOSSY | Film::EMISSION | Film::INDIRECT_DIFFUSE |
		Film::INDIRECT_GLOSSY | Film::INDIRECT_SPECULAR | Film::DIRECT_SHADOW_MASK |
		Film::INDIRECT_SHADOW_MASK | Film::UV, engine->film->GetRadianceGroupCount());

	while (!boost::this_thread::interruption_requested()) {
		// Set to 0.0 all result colors
		sampleResult.emission = Spectrum();
		for (u_int i = 0; i < sampleResult.radiancePerPixelNormalized.size(); ++i)
			sampleResult.radiancePerPixelNormalized[i] = Spectrum();
		sampleResult.directDiffuse = Spectrum();
		sampleResult.directGlossy = Spectrum();
		sampleResult.indirectDiffuse = Spectrum();
		sampleResult.indirectGlossy = Spectrum();
		sampleResult.indirectSpecular = Spectrum();
		sampleResult.directShadowMask = 1.f;
		sampleResult.indirectShadowMask = 1.f;

		Ray eyeRay;
		sampleResult.filmX = Min(sampler->GetSample(0) * filmWidth, (float)(filmWidth - 1));
		sampleResult.filmY = Min(sampler->GetSample(1) * filmHeight, (float)(filmHeight - 1));
		camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay,
			sampler->GetSample(2), sampler->GetSample(3));

		int depth = 1;
		BSDFEvent pathBSDFEvent = NONE;
		BSDFEvent lastBSDFEvent = SPECULAR; // SPECULAR is required to avoid MIS
		float lastPdfW = 1.f;
		Spectrum pathThroughput(1.f, 1.f, 1.f);
		BSDF bsdf;
		while (depth <= engine->maxPathDepth) {
			const bool firstPathVertex = (depth == 1);
			const unsigned int sampleOffset = sampleBootSize + (depth - 1) * sampleStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			if (!scene->Intersect(device, false, sampler->GetSample(sampleOffset),
					&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput)) {
				// Nothing was hit, look for infinitelight
				DirectHitInfiniteLight(firstPathVertex, lastBSDFEvent, pathBSDFEvent,
						pathThroughput * connectionThroughput, eyeRay.d,
						lastPdfW, &sampleResult);

				if (firstPathVertex) {
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
					sampleResult.uv = UV(std::numeric_limits<float>::infinity(),
							std::numeric_limits<float>::infinity());
					sampleResult.directShadowMask = 0.f;
					sampleResult.indirectShadowMask = 0.f;
				}
				break;
			}
			pathThroughput *= connectionThroughput;

			// Something was hit

			if (firstPathVertex) {
				sampleResult.alpha = 1.f;
				sampleResult.depth = eyeRayHit.t;
				sampleResult.position = bsdf.hitPoint.p;
				sampleResult.geometryNormal = bsdf.hitPoint.geometryN;
				sampleResult.shadingNormal = bsdf.hitPoint.shadeN;
				sampleResult.materialID = bsdf.GetMaterialID();
				sampleResult.uv = bsdf.hitPoint.uv;
			}

			// Check if it is a light source
			if (bsdf.IsLightSource()) {
				DirectHitFiniteLight(firstPathVertex, lastBSDFEvent, pathBSDFEvent, pathThroughput,
						eyeRayHit.t, bsdf, lastPdfW, &sampleResult);
			}

			// Note: pass-through check is done inside SceneIntersect()

			//------------------------------------------------------------------
			// Direct light sampling
			//------------------------------------------------------------------

			DirectLightSampling(firstPathVertex, pathBSDFEvent, 
					sampler->GetSample(sampleOffset + 1),
					sampler->GetSample(sampleOffset + 2),
					sampler->GetSample(sampleOffset + 3),
					sampler->GetSample(sampleOffset + 4),
					sampler->GetSample(sampleOffset + 5),
					pathThroughput, bsdf, depth, &sampleResult);

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			Vector sampledDir;
			float cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
					sampler->GetSample(sampleOffset + 6),
					sampler->GetSample(sampleOffset + 7),
					&lastPdfW, &cosSampledDir, &lastBSDFEvent);
			if (bsdfSample.Black())
				break;

			if (firstPathVertex)
				pathBSDFEvent = lastBSDFEvent;

			if ((depth >= engine->rrDepth) && !(lastBSDFEvent & SPECULAR)) {
				// Russian Roulette
				const float prob = RenderEngine::RussianRouletteProb(bsdfSample, engine->rrImportanceCap);
				if (sampler->GetSample(sampleOffset + 8) < prob)
					lastPdfW *= prob;
				else
					break;
			}

			pathThroughput *= bsdfSample * (cosSampledDir / lastPdfW);
			assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

			eyeRay = Ray(bsdf.hitPoint.p, sampledDir);
			++depth;
		}

		sampleResult.indirectShadowMask = 0.f;

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif
	}

	delete sampler;
	delete rndGen;

	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
