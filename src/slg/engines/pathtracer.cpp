/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/engines/pathtracer.h"

using namespace std;
using namespace luxrays;
using namespace slg;

PathTracer::PathTracer() : pixelFilterDistribution(NULL) {
}

PathTracer::~PathTracer() {
	delete pixelFilterDistribution;
}

void PathTracer::InitPixelFilterDistribution(const Filter *pixelFilter) {
	// Compile sample distribution
	delete pixelFilterDistribution;
	pixelFilterDistribution = new FilterDistribution(pixelFilter, 64);
}

void  PathTracer::DeletePixelFilterDistribution() {
	delete pixelFilterDistribution;
	pixelFilterDistribution = NULL;
}

void PathTracer::ParseOptions(const luxrays::Properties &cfg, const luxrays::Properties &defaultProps) {
	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(defaultProps.Get("path.pathdepth.total")).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(defaultProps.Get("path.pathdepth.diffuse")).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(defaultProps.Get("path.pathdepth.glossy")).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(defaultProps.Get("path.pathdepth.specular")).Get<int>());

	// For compatibility with the past
	if (cfg.IsDefined("path.maxdepth") &&
			!cfg.IsDefined("path.pathdepth.total") &&
			!cfg.IsDefined("path.pathdepth.diffuse") &&
			!cfg.IsDefined("path.pathdepth.glossy") &&
			!cfg.IsDefined("path.pathdepth.specular")) {
		const u_int maxDepth = Max(0, cfg.Get("path.maxdepth").Get<int>());
		maxPathDepth.depth = maxDepth;
		maxPathDepth.diffuseDepth = maxDepth;
		maxPathDepth.glossyDepth = maxDepth;
		maxPathDepth.specularDepth = maxDepth;
	}

	// Russian Roulette settings
	rrDepth = (u_int)Max(1, cfg.Get(defaultProps.Get("path.russianroulette.depth")).Get<int>());
	rrImportanceCap = Clamp(cfg.Get(defaultProps.Get("path.russianroulette.cap")).Get<float>(), 0.f, 1.f);

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("path.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("path.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(defaultProps.Get("path.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);

	forceBlackBackground = cfg.Get(defaultProps.Get("path.forceblackbackground.enable")).Get<bool>();
	
	// Update sample size
	sampleBootSize = 5;
	sampleStepSize = 9;
	sampleSize = 
		sampleBootSize + // To generate eye ray
		(maxPathDepth.depth + 1) * sampleStepSize; // For each path vertex
}

void PathTracer::InitSampleResults(const Film *film, vector<SampleResult> &sampleResults) const {
	SampleResult &sampleResult = sampleResults[0];

	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH |
		Film::POSITION | Film::GEOMETRY_NORMAL | Film::SHADING_NORMAL | Film::MATERIAL_ID |
		Film::DIRECT_DIFFUSE | Film::DIRECT_GLOSSY | Film::EMISSION | Film::INDIRECT_DIFFUSE |
		Film::INDIRECT_GLOSSY | Film::INDIRECT_SPECULAR | Film::DIRECT_SHADOW_MASK |
		Film::INDIRECT_SHADOW_MASK | Film::UV | Film::RAYCOUNT | Film::IRRADIANCE |
		Film::OBJECT_ID,
		film->GetRadianceGroupCount());
	sampleResult.useFilmSplat = false;
}

bool PathTracer::DirectLightSampling(
		luxrays::IntersectionDevice *device, const Scene *scene,
		const float time,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathDepthInfo &depthInfo,
		const Spectrum &pathThroughput, const BSDF &bsdf,
		PathVolumeInfo volInfo,
		SampleResult *sampleResult) const {
	if (!bsdf.IsDelta()) {
		// Select the light strategy to use
		const LightStrategy *lightStrategy;
		if (bsdf.IsShadowCatcherOnlyInfiniteLights())
			lightStrategy = scene->lightDefs.GetInfiniteLightStrategy();
		else
			lightStrategy = scene->lightDefs.GetIlluminateLightStrategy();
		
		// Pick a light source to sample
		float lightPickPdf;
		const LightSource *light = lightStrategy->SampleLights(u0, &lightPickPdf);

		if (light) {
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

				// Create a new DepthInfo for the path to the light source
				PathDepthInfo directLightDepthInfo = depthInfo;
				directLightDepthInfo.IncDepths(event);
				
				if (!bsdfEval.Black()) {
					assert (!isnan(bsdfPdfW) && !isinf(bsdfPdfW));

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
						// Add the light contribution only if it is not a shadow catcher
						// (because, if the light is visible, the material will be
						// transparent in the case of a shadow catcher).

						if (!bsdf.IsShadowCatcher()) {
							// I'm ignoring volume emission because it is not sampled in
							// direct light step.
							const float directLightSamplingPdfW = directPdfW * lightPickPdf;
							const float factor = 1.f / directLightSamplingPdfW;

							if (directLightDepthInfo.GetRRDepth() >= rrDepth) {
								// Russian Roulette
								bsdfPdfW *= RenderEngine::RussianRouletteProb(bsdfEval, rrImportanceCap);
							}

							// MIS between direct light sampling and BSDF sampling
							//
							// Note: I have to avoid MIS on the last path vertex
							const bool misEnabled = !sampleResult->lastPathVertex &&
								(light->IsEnvironmental() || light->IsIntersectable()) &&
								CheckDirectHitVisibilityFlags(light, directLightDepthInfo, event);

							const float weight = misEnabled ? PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;
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

						return true;
					}
				}
			}
		}
	}

	return false;
}

bool PathTracer::CheckDirectHitVisibilityFlags(const LightSource *lightSource, const PathDepthInfo &depthInfo,
		const BSDFEvent lastBSDFEvent) const {
	if (depthInfo.depth == 0)
		return true;

	if ((lastBSDFEvent & DIFFUSE) && lightSource->IsVisibleIndirectDiffuse())
		return true;
	if ((lastBSDFEvent & GLOSSY) && lightSource->IsVisibleIndirectGlossy())
		return true;
	if ((lastBSDFEvent & SPECULAR) && lightSource->IsVisibleIndirectSpecular())
		return true;

	return false;
}

void PathTracer::DirectHitFiniteLight(const Scene *scene,  const PathDepthInfo &depthInfo,
		const BSDFEvent lastBSDFEvent, const Spectrum &pathThroughput,
		const float distance, const BSDF &bsdf,
		const float lastPdfW, SampleResult *sampleResult) const {
	const LightSource *lightSource = bsdf.GetLightSource();

	// Check if the light source is visible according the settings
	if (!CheckDirectHitVisibilityFlags(lightSource, depthInfo, lastBSDFEvent))
		return;
	
	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!(lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = scene->lightDefs.GetIlluminateLightStrategy()->SampleLightPdf(lightSource);
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		sampleResult->AddEmission(bsdf.GetLightID(), pathThroughput, weight * emittedRadiance);
	}
}

void PathTracer::DirectHitInfiniteLight(const Scene *scene,  const PathDepthInfo &depthInfo,
		const BSDFEvent lastBSDFEvent, const Spectrum &pathThroughput,
		const Vector &eyeDir, const float lastPdfW,	SampleResult *sampleResult) const {
	BOOST_FOREACH(EnvLightSource *envLight, scene->lightDefs.GetEnvLightSources()) {
		// Check if the light source is visible according the settings
		if (!CheckDirectHitVisibilityFlags(envLight, depthInfo, lastBSDFEvent))
			continue;

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

void PathTracer::GenerateEyeRay(const Camera *camera, const Film *film, Ray &eyeRay, Sampler *sampler, SampleResult &sampleResult) const {
	const float u0 = sampler->GetSample(0);
	const float u1 = sampler->GetSample(1);
	film->GetSampleXY(u0, u1, &sampleResult.filmX, &sampleResult.filmY);

	// Use fast pixel filtering, like the one used in TILEPATH.

	sampleResult.pixelX = Floor2UInt(sampleResult.filmX);
	sampleResult.pixelY = Floor2UInt(sampleResult.filmY);

	const float uSubPixelX = sampleResult.filmX - sampleResult.pixelX;
	const float uSubPixelY = sampleResult.filmY - sampleResult.pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	pixelFilterDistribution->SampleContinuous(uSubPixelX, uSubPixelY, &distX, &distY);

	sampleResult.filmX = sampleResult.pixelX + .5f + distX;
	sampleResult.filmY = sampleResult.pixelY + .5f + distY;

	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay,
		sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));
}

void PathTracer::RenderSample(luxrays::IntersectionDevice *device, const Scene *scene, const Film *film,
		Sampler *sampler, vector<SampleResult> &sampleResults) const {
	SampleResult &sampleResult = sampleResults[0];

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
	sampleResult.passThroughPath = true;

	// To keep track of the number of rays traced
	const double deviceRayCount = device->GetTotalRaysCount();

	Ray eyeRay;
	GenerateEyeRay(scene->camera, film, eyeRay, sampler, sampleResult);

	BSDFEvent lastBSDFEvent = SPECULAR; // SPECULAR is required to avoid MIS
	float lastPdfW = 1.f;
	Spectrum pathThroughput(1.f);
	PathVolumeInfo volInfo;
	PathDepthInfo depthInfo;
	BSDF bsdf;
	for (;;) {
		sampleResult.firstPathVertex = (depthInfo.depth == 0);
		const u_int sampleOffset = sampleBootSize + depthInfo.depth * sampleStepSize;

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
			if (!forceBlackBackground || !sampleResult.passThroughPath)
				DirectHitInfiniteLight(scene, depthInfo, lastBSDFEvent, pathThroughput,
						eyeRay.d, lastPdfW, &sampleResult);

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
			// The alpha value can be changed if the material is a shadow catcher (see below)
			sampleResult.alpha = 1.f;
			sampleResult.depth = eyeRayHit.t;
			sampleResult.position = bsdf.hitPoint.p;
			sampleResult.geometryNormal = bsdf.hitPoint.geometryN;
			sampleResult.shadingNormal = bsdf.hitPoint.shadeN;
			sampleResult.materialID = bsdf.GetMaterialID();
			sampleResult.objectID = bsdf.GetObjectID();
			sampleResult.uv = bsdf.hitPoint.uv;
		}
		sampleResult.lastPathVertex = depthInfo.IsLastPathVertex(maxPathDepth, bsdf.GetEventTypes());

		// Check if it is a light source
		if (bsdf.IsLightSource()) {
			DirectHitFiniteLight(scene, depthInfo, lastBSDFEvent, pathThroughput,
					eyeRayHit.t, bsdf, lastPdfW, &sampleResult);
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

		const bool isLightVisible = DirectLightSampling(
				device, scene,
				eyeRay.time,
				sampler->GetSample(sampleOffset + 1),
				sampler->GetSample(sampleOffset + 2),
				sampler->GetSample(sampleOffset + 3),
				sampler->GetSample(sampleOffset + 4),
				sampler->GetSample(sampleOffset + 5),
				depthInfo, 
				pathThroughput, bsdf, volInfo, &sampleResult);

		if (sampleResult.lastPathVertex)
			break;

		//------------------------------------------------------------------
		// Build the next vertex path ray
		//------------------------------------------------------------------

		Vector sampledDir;
		float cosSampledDir;
		Spectrum bsdfSample;
		if (bsdf.IsShadowCatcher() && isLightVisible) {
			bsdfSample = bsdf.ShadowCatcherSample(&sampledDir, &lastPdfW, &cosSampledDir, &lastBSDFEvent);

			if (sampleResult.firstPathVertex) {
				// In this case I have also to set the value of the alpha channel to 0.0
				sampleResult.alpha = 0.f;
			}
		} else {
			bsdfSample = bsdf.Sample(&sampledDir,
					sampler->GetSample(sampleOffset + 6),
					sampler->GetSample(sampleOffset + 7),
					&lastPdfW, &cosSampledDir, &lastBSDFEvent);
			sampleResult.passThroughPath = false;
		}

		assert (!bsdfSample.IsNaN() && !bsdfSample.IsInf());
		if (bsdfSample.Black())
			break;
		assert (!isnan(lastPdfW) && !isinf(lastPdfW));

		if (sampleResult.firstPathVertex)
			sampleResult.firstPathVertexEvent = lastBSDFEvent;

		// Increment path depth informations
		depthInfo.IncDepths(lastBSDFEvent);

		Spectrum throughputFactor(1.f);
		// Russian Roulette
		float rrProb;
		if (!(lastBSDFEvent & SPECULAR) && (depthInfo.GetRRDepth() >= rrDepth)) {
			rrProb = RenderEngine::RussianRouletteProb(bsdfSample, rrImportanceCap);
			if (rrProb < sampler->GetSample(sampleOffset + 8))
				break;

			// Increase path contribution
			throughputFactor /= rrProb;
		} else
			rrProb = 1.f;

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
	}

	sampleResult.rayCount = (float)(device->GetTotalRaysCount() - deviceRayCount);
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties PathTracer::ToProperties(const Properties &cfg) {
	Properties props;
	
	if (cfg.IsDefined("path.maxdepth") &&
			!cfg.IsDefined("path.pathdepth.total") &&
			!cfg.IsDefined("path.pathdepth.diffuse") &&
			!cfg.IsDefined("path.pathdepth.glossy") &&
			!cfg.IsDefined("path.pathdepth.specular")) {
		const u_int maxDepth = Max(0, cfg.Get("path.maxdepth").Get<int>());
		props << 
				Property("path.pathdepth.total")(maxDepth) <<
				Property("path.pathdepth.diffuse")(maxDepth) <<
				Property("path.pathdepth.glossy")(maxDepth) <<
				Property("path.pathdepth.specular")(maxDepth);
	} else {
		props <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.total")) <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.diffuse")) <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.glossy")) <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.specular"));
	}

	props <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")) <<
			cfg.Get(GetDefaultProps().Get("path.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("path.forceblackbackground.enable")) <<
			Sampler::ToProperties(cfg);

	return props;
}

const Properties &PathTracer::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.pathdepth.total")(6) <<
			Property("path.pathdepth.diffuse")(4) <<
			Property("path.pathdepth.glossy")(4) <<
			Property("path.pathdepth.specular")(6) <<
			Property("path.russianroulette.depth")(3) <<
			Property("path.russianroulette.cap")(.5f) <<
			Property("path.clamping.variance.maxvalue")(0.f) <<
			Property("path.forceblackbackground.enable")(false);

	return props;
}
