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

#include <boost/function.hpp>

#include "slg/engines/pathtracer.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/samplers/metropolis.h"
#include "slg/utils/varianceclamping.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathTracer
//------------------------------------------------------------------------------

PathTracerThreadState::PathTracerThreadState(IntersectionDevice *dev,
		Sampler *eSampler, Sampler *lSampler,
		const Scene *scn, Film *flm,
		const VarianceClamping *varClamping,
		const bool useFilmSplat) : device(dev),
		eyeSampler(eSampler), lightSampler(lSampler), scene(scn), film(flm),
		varianceClamping(varClamping) {
	// Initialize Eye SampleResults
	eyeSampleResults.resize(1);
	PathTracer::InitEyeSampleResults(film, eyeSampleResults, useFilmSplat);

	eyeSampleCount = 0.0;
	// Using 1.0 instead of 0.0 to avoid a division by zero
	lightSampleCount = 1.0;
}

PathTracerThreadState::~PathTracerThreadState() {
}

//------------------------------------------------------------------------------
// PathTracer
//------------------------------------------------------------------------------

const Film::FilmChannels PathTracer::eyeSampleResultsChannels({
	Film::RADIANCE_PER_PIXEL_NORMALIZED, Film::ALPHA, Film::DEPTH,
	Film::POSITION, Film::GEOMETRY_NORMAL, Film::SHADING_NORMAL, Film::MATERIAL_ID,
	Film::DIRECT_DIFFUSE, Film::DIRECT_DIFFUSE_REFLECT, Film::DIRECT_DIFFUSE_TRANSMIT,
	Film::DIRECT_GLOSSY, Film::DIRECT_GLOSSY_REFLECT, Film::DIRECT_GLOSSY_TRANSMIT,
	Film::EMISSION,
	Film::INDIRECT_DIFFUSE, Film::INDIRECT_DIFFUSE_REFLECT, Film::INDIRECT_DIFFUSE_TRANSMIT,
	Film::INDIRECT_GLOSSY, Film::INDIRECT_GLOSSY_REFLECT, Film::INDIRECT_GLOSSY_TRANSMIT,
	Film::INDIRECT_SPECULAR, Film::INDIRECT_SPECULAR_REFLECT, Film::INDIRECT_SPECULAR_TRANSMIT,
	Film::DIRECT_SHADOW_MASK, Film::INDIRECT_SHADOW_MASK, Film::UV, Film::RAYCOUNT,
	Film::IRRADIANCE, Film::OBJECT_ID, Film::SAMPLECOUNT, Film::CONVERGENCE,
	Film::MATERIAL_ID_COLOR, Film::ALBEDO, Film::AVG_SHADING_NORMAL, Film::NOISE
});

const Film::FilmChannels PathTracer::lightSampleResultsChannels({
	Film::RADIANCE_PER_SCREEN_NORMALIZED
}); 

PathTracer::PathTracer() : pixelFilterDistribution(nullptr),
		photonGICache(nullptr) {
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

void PathTracer::InitEyeSampleResults(const Film *film, vector<SampleResult> &sampleResults,
		const bool useFilmSplat) {
	SampleResult &sampleResult = sampleResults[0];

	sampleResult.Init(&eyeSampleResultsChannels, film->GetRadianceGroupCount());
	sampleResult.useFilmSplat = useFilmSplat;
}

void PathTracer::ResetEyeSampleResults(vector<SampleResult> &sampleResults) {
	SampleResult &sampleResult = sampleResults[0];

	// Set to 0.0 all result colors
	sampleResult.emission = Spectrum();
	for (u_int i = 0; i < sampleResult.radiance.Size(); ++i)
		sampleResult.radiance[i] = Spectrum();
	sampleResult.directDiffuseReflect = Spectrum();
	sampleResult.directDiffuseTransmit = Spectrum();
	sampleResult.directGlossyReflect = Spectrum();
	sampleResult.directGlossyTransmit = Spectrum();
	sampleResult.indirectDiffuseReflect = Spectrum();
	sampleResult.indirectDiffuseTransmit = Spectrum();
	sampleResult.indirectGlossyReflect = Spectrum();
	sampleResult.indirectGlossyTransmit = Spectrum();
	sampleResult.indirectSpecularReflect = Spectrum();
	sampleResult.indirectSpecularTransmit = Spectrum();
	sampleResult.directShadowMask = 1.f;
	sampleResult.indirectShadowMask = 1.f;
	sampleResult.irradiance = Spectrum();
	sampleResult.albedo = Spectrum();

	sampleResult.rayCount = 0.f;
}

//------------------------------------------------------------------------------
// RenderEyeSample methods
//------------------------------------------------------------------------------

PathTracer::DirectLightResult PathTracer::DirectLightSampling(
		luxrays::IntersectionDevice *device, const Scene *scene,
		const float time,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const EyePathInfo &pathInfo, const Spectrum &pathThroughput,
		const BSDF &bsdf, SampleResult *sampleResult,
		const bool useBSDFEVal) const {
	if (!bsdf.IsDelta()) {
		// Select the light strategy to use
		const LightStrategy *lightStrategy;
		if (bsdf.IsShadowCatcherOnlyInfiniteLights())
			lightStrategy = scene->lightDefs.GetInfiniteLightStrategy();
		else
			lightStrategy = scene->lightDefs.GetIlluminateLightStrategy();
		
		// Pick a light source to sample
		const Normal landingNormal = bsdf.hitPoint.intoObject ? bsdf.hitPoint.shadeN : -bsdf.hitPoint.shadeN;
		float lightPickPdf;
		const LightSource *light = lightStrategy->SampleLights(u0,
				bsdf.hitPoint.p, landingNormal, bsdf.IsVolume(), &lightPickPdf);

		if (light) {
			Ray shadowRay;
			float directPdfW;
			Spectrum lightRadiance = light->Illuminate(*scene, bsdf,
					time, u1, u2, u3, shadowRay, directPdfW);
			assert (!lightRadiance.IsNaN() && !lightRadiance.IsInf());

			if (!lightRadiance.Black()) {
				assert (!isnan(directPdfW) && !isinf(directPdfW));

				BSDFEvent event;
				float bsdfPdfW;
				Spectrum bsdfEval;
				
				if (useBSDFEVal)
					bsdfEval = bsdf.Evaluate(shadowRay.d, &event, &bsdfPdfW);
				else {
					// This is used by BAKECPU and must be aligned with its BSDF sampling
					bsdfEval = Spectrum(Dot(shadowRay.d, bsdf.hitPoint.shadeN) * INV_PI);
					bsdfPdfW = INV_TWOPI;
					event = DIFFUSE | REFLECT;
				}
				assert (!bsdfEval.IsNaN() && !bsdfEval.IsInf());

				if (!bsdfEval.Black() &&
						(!hybridBackForwardEnable ||
						!pathInfo.IsCausticPath(event, bsdf.GetGlossiness(), hybridBackForwardGlossinessThreshold))) {
					assert (!isnan(bsdfPdfW) && !isinf(bsdfPdfW));
					
					// Create a new PathDepthInfo for the path to the light source
					PathDepthInfo directLightDepthInfo = pathInfo.depth;
					directLightDepthInfo.IncDepths(event);
					
					RayHit shadowRayHit;
					BSDF shadowBsdf;
					Spectrum connectionThroughput;
					// Create a new PathVolumeInfo for the path to the light source
					PathVolumeInfo volInfo = pathInfo.volume;
					// Check if the light source is visible
					if (!scene->Intersect(device, EYE_RAY | SHADOW_RAY, &volInfo, u4, &shadowRay,
							&shadowRayHit, &shadowBsdf, &connectionThroughput, nullptr,
							nullptr, true)) {
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

							// Account for material transparency
							bsdfPdfW *= light->GetAvgPassThroughTransparency();

							// MIS between direct light sampling and BSDF sampling
							//
							// Note: I have to avoid MIS on the last path vertex
							const bool misEnabled = !sampleResult->lastPathVertex &&
								(light->IsEnvironmental() || light->IsIntersectable()) &&
								CheckDirectHitVisibilityFlags(light, directLightDepthInfo, event) &&
								!shadowBsdf.hitPoint.throughShadowTransparency;

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

						return ILLUMINATED;
					} else
						return SHADOWED; 
				}
			}
		}
	}

	return NOT_VISIBLE;
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

void PathTracer::DirectHitFiniteLight(const Scene *scene,
		const EyePathInfo &pathInfo,
		const Spectrum &pathThroughput, const Ray &ray,
		const float distance, const BSDF &bsdf,
		SampleResult *sampleResult) const {
	const LightSource *lightSource = bsdf.GetLightSource();

	// Check if the light source is visible according the settings
	if (!CheckDirectHitVisibilityFlags(lightSource, pathInfo.depth, pathInfo.lastBSDFEvent) ||
			// If the material is shadow transparent, Direct Light sampling
			// will take care of transporting all emitted light
			bsdf.hitPoint.throughShadowTransparency)
		return;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (!emittedRadiance.Black()) {
		float weight;
		if (!(pathInfo.lastBSDFEvent & SPECULAR)) {
			const LightStrategy *lightStrategy = scene->lightDefs.GetIlluminateLightStrategy();
			const float lightPickProb = lightStrategy->SampleLightPdf(lightSource,
					ray.o, pathInfo.lastShadeN, pathInfo.lastFromVolume);

			// This is a specific check to avoid fireflies with DLSC
			if ((lightPickProb == 0.f) && lightSource->IsDirectLightSamplingEnabled() &&
					(lightStrategy->GetType() == TYPE_DLS_CACHE))
				return;

			const float directPdfW = PdfAtoW(directPdfA, distance,
					AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(pathInfo.lastBSDFPdfW * lightSource->GetAvgPassThroughTransparency(), directPdfW * lightPickProb);
		} else
			weight = 1.f;

		sampleResult->AddEmission(bsdf.GetLightID(), pathThroughput, weight * emittedRadiance);
	}
}

void PathTracer::DirectHitInfiniteLight(const Scene *scene,
		const EyePathInfo &pathInfo, const Spectrum &pathThroughput,
		const Ray &ray, const BSDF *bsdf, SampleResult *sampleResult) const {
	// If the material is shadow transparent, Direct Light sampling
	// will take care of transporting all emitted light
	if (bsdf && bsdf->hitPoint.throughShadowTransparency)
		return;

	BOOST_FOREACH(EnvLightSource *envLight, scene->lightDefs.GetEnvLightSources()) {
		// Check if the light source is visible according the settings
		if (!CheckDirectHitVisibilityFlags(envLight, pathInfo.depth, pathInfo.lastBSDFEvent))
			continue;

		float directPdfW;
		const Spectrum envRadiance = envLight->GetRadiance(*scene, bsdf, -ray.d, &directPdfW);
		if (!envRadiance.Black()) {
			float weight;
			if (!(pathInfo.lastBSDFEvent & SPECULAR)) {
				const float lightPickProb = scene->lightDefs.GetIlluminateLightStrategy()->
						SampleLightPdf(envLight, ray.o, pathInfo.lastShadeN, pathInfo.lastFromVolume);

				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(pathInfo.lastBSDFPdfW, directPdfW * lightPickProb);
			} else
				weight = 1.f;

			sampleResult->AddEmission(envLight->GetID(), pathThroughput, weight * envRadiance);
		}
	}	
}

void PathTracer::GenerateEyeRay(const Camera *camera, const Film *film, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const {
	const float filmX = sampler->GetSample(0);
	const float filmY = sampler->GetSample(1);

	// Use fast pixel filtering, like the one used in TILEPATH.

	const u_int *subRegion = film->GetSubRegion();
	sampleResult.pixelX = Min(Floor2UInt(filmX), subRegion[1]);
	sampleResult.pixelY = Min(Floor2UInt(filmY), subRegion[3]);
	assert (sampleResult.pixelX >= subRegion[0]);
	assert (sampleResult.pixelX <= subRegion[1]);
	assert (sampleResult.pixelY >= subRegion[2]);
	assert (sampleResult.pixelY <= subRegion[3]);

	const float uSubPixelX = filmX - sampleResult.pixelX;
	const float uSubPixelY = filmY - sampleResult.pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	pixelFilterDistribution->SampleContinuous(uSubPixelX, uSubPixelY, &distX, &distY);

	sampleResult.filmX = sampleResult.pixelX + .5f + distX;
	sampleResult.filmY = sampleResult.pixelY + .5f + distY;

	const float timeSample = sampler->GetSample(4);
	const float time = camera->GenerateRayTime(timeSample);

	camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3));
}

//------------------------------------------------------------------------------
// RenderEyePath methods
//------------------------------------------------------------------------------

void PathTracer::RenderEyePath(IntersectionDevice *device,
		const Scene *scene, Sampler *sampler, EyePathInfo &pathInfo,
		Ray &eyeRay,  const luxrays::Spectrum &eyeTroughput,
		vector<SampleResult> &sampleResults) const {
	// To keep track of the number of rays traced
	const double deviceRayCount = device->GetTotalRaysCount();

	// This is used by light strategy
	pathInfo.lastShadeN = Normal(eyeRay.d);

	SampleResult &sampleResult = sampleResults[0];
	bool photonGIShowIndirectPathMixUsed = false;
	bool photonGICausticCacheUsed = false;
	bool photonGICacheEnabledOnLastHit = false;
	bool albedoToDo = true;
	Spectrum pathThroughput(eyeTroughput);
	BSDF bsdf;
	for (;;) {
		sampleResult.firstPathVertex = (pathInfo.depth.depth == 0);
		const u_int sampleOffset = eyeSampleBootSize + pathInfo.depth.depth * eyeSampleStepSize;

		RayHit eyeRayHit;
		Spectrum connectionThroughput;
		const float passThrough = sampler->GetSample(sampleOffset);
		const bool hit = scene->Intersect(device,
				EYE_RAY | (sampleResult.firstPathVertex ? CAMERA_RAY : INDIRECT_RAY),
				&pathInfo.volume, passThrough,
				&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput,
				&pathThroughput, &sampleResult);
		pathThroughput *= connectionThroughput;
		// Note: pass-through check is done inside Scene::Intersect()

		const bool checkDirectLightHit =
				// Avoid to render caustic path if hybridBackForwardEnable
				(!hybridBackForwardEnable || !pathInfo.IsCausticPath()) &&
				// Avoid to render caustic path if PhotonGI caustic cache is enabled
				(!photonGICache ||
					photonGICache->IsDirectLightHitVisible(pathInfo, photonGICausticCacheUsed));

		if (!hit) {
			// Nothing was hit, look for env. lights
			if ((!(forceBlackBackground && pathInfo.isPassThroughPath) || !pathInfo.isPassThroughPath) &&
					checkDirectLightHit) {
				DirectHitInfiniteLight(scene, pathInfo, pathThroughput,
						eyeRay, sampleResult.firstPathVertex ? nullptr : &bsdf,
						&sampleResult);
			}

			if (sampleResult.firstPathVertex) {
				sampleResult.alpha = 0.f;
				sampleResult.depth = numeric_limits<float>::infinity();
				sampleResult.position = Point(
						numeric_limits<float>::infinity(),
						numeric_limits<float>::infinity(),
						numeric_limits<float>::infinity());
				sampleResult.geometryNormal = Normal();
				sampleResult.shadingNormal = Normal();
				sampleResult.materialID = 0;
				sampleResult.objectID = 0;
				sampleResult.uv = UV(numeric_limits<float>::infinity(),
						numeric_limits<float>::infinity());
				sampleResult.isHoldout = false;
			}
			break;
		}

		// Something was hit

		if (albedoToDo && bsdf.IsAlbedoEndPoint()) {
			sampleResult.albedo = pathThroughput * bsdf.Albedo();
			albedoToDo = false;
		}

		if (sampleResult.firstPathVertex) {
			// The alpha value can be changed if the material is a shadow catcher (see below)
			sampleResult.alpha = bsdf.IsHoldout() ? 0.f : 1.f;
			sampleResult.depth = eyeRayHit.t;
			sampleResult.position = bsdf.hitPoint.p;
			sampleResult.geometryNormal = bsdf.hitPoint.geometryN;
			sampleResult.shadingNormal = bsdf.hitPoint.shadeN;
			sampleResult.materialID = bsdf.GetMaterialID();
			sampleResult.objectID = bsdf.GetObjectID();
			sampleResult.uv = bsdf.hitPoint.GetUV(0);
			sampleResult.isHoldout = bsdf.IsHoldout();
		}
		sampleResult.lastPathVertex = pathInfo.depth.IsLastPathVertex(maxPathDepth, bsdf.GetEventTypes());

		//----------------------------------------------------------------------
		// Check if it is a baked material
		//----------------------------------------------------------------------

		if (bsdf.HasBakeMap(COMBINED)) {
			sampleResult.radiance[0] += pathThroughput * bsdf.GetBakeMapValue();
			break;
		} else if (bsdf.HasBakeMap(LIGHTMAP)) {
			sampleResult.radiance[0] += pathThroughput * bsdf.Albedo() * bsdf.GetBakeMapValue();
			break;
		}

		//----------------------------------------------------------------------
		// Check if it is a light source and I have to add light emission
		//----------------------------------------------------------------------

		if (bsdf.IsLightSource() && checkDirectLightHit) {
			DirectHitFiniteLight(scene, pathInfo, pathThroughput,
					eyeRay,  eyeRayHit.t, bsdf, &sampleResult);
		}

		//----------------------------------------------------------------------
		// Check if I can use the photon cache
		//----------------------------------------------------------------------

		if (photonGICache) {
			const bool isPhotonGIEnabled = photonGICache->IsPhotonGIEnabled(bsdf);

			// Check if one of the debug modes is enabled
			if (photonGICache->GetDebugType() == PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECT) {
				if (isPhotonGIEnabled) {
					const SpectrumGroup *group = photonGICache->GetIndirectRadiance(bsdf);
					if (group)
						sampleResult.radiance += *group;
				}
				break;
			} else if (photonGICache->GetDebugType() == PhotonGIDebugType::PGIC_DEBUG_SHOWCAUSTIC) {
				if (isPhotonGIEnabled)
					sampleResult.radiance += photonGICache->ConnectWithCausticPaths(bsdf);
					break;
			} else if (photonGICache->GetDebugType() == PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECTPATHMIX) {
				// Check if the cache is enabled for this material
				if (isPhotonGIEnabled) {
					if (photonGICacheEnabledOnLastHit &&
							(eyeRayHit.t > photonGICache->GetIndirectUsageThreshold(pathInfo.lastBSDFEvent,
								pathInfo.lastGlossiness,
								passThrough))) {
						sampleResult.radiance[0] = Spectrum(0.f, 0.f, 1.f);
						photonGIShowIndirectPathMixUsed = true;
						break;
					}

					photonGICacheEnabledOnLastHit = true;
				} else
					photonGICacheEnabledOnLastHit = false;
			} else {
				// Check if the cache is enabled for this material
				if (isPhotonGIEnabled) {
					// TODO: add support for AOVs (possible ?)

					if (photonGICache->IsCausticEnabled() && (!hybridBackForwardEnable || pathInfo.depth.depth != 0)) {
						const SpectrumGroup causticRadiance = photonGICache->ConnectWithCausticPaths(bsdf);

						if (!causticRadiance.Black()) {
							sampleResult.radiance.AddWeighted(pathThroughput, causticRadiance);
							photonGICausticCacheUsed = true;
						}
					}

					if (photonGICache->IsIndirectEnabled() && photonGICacheEnabledOnLastHit &&
							(eyeRayHit.t > photonGICache->GetIndirectUsageThreshold(pathInfo.lastBSDFEvent,
								pathInfo.lastGlossiness,
								// I hope to not introduce strange sample correlations
								// by using passThrough here
								passThrough))) {
						const SpectrumGroup *group = photonGICache->GetIndirectRadiance(bsdf);
						if (group)
							sampleResult.radiance.AddWeighted(pathThroughput, *group);
						// I can terminate the path, all done
						break;
					}

					photonGICacheEnabledOnLastHit = true;
				} else
					photonGICacheEnabledOnLastHit = false;
			}
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

		const DirectLightResult directLightResult = DirectLightSampling(
				device, scene,
				eyeRay.time,
				sampler->GetSample(sampleOffset + 1),
				sampler->GetSample(sampleOffset + 2),
				sampler->GetSample(sampleOffset + 3),
				sampler->GetSample(sampleOffset + 4),
				sampler->GetSample(sampleOffset + 5),
				pathInfo, 
				pathThroughput, bsdf, &sampleResult);

		if (sampleResult.lastPathVertex)
			break;

		//------------------------------------------------------------------
		// Build the next vertex path ray
		//------------------------------------------------------------------

		Vector sampledDir;
		float cosSampledDir;
		Spectrum bsdfSample;
		float bsdfPdfW;
		BSDFEvent bsdfEvent;
		if (bsdf.IsShadowCatcher() && (directLightResult != SHADOWED)) {
			bsdfSample = bsdf.ShadowCatcherSample(&sampledDir, &bsdfPdfW, &cosSampledDir, &bsdfEvent);

			if (sampleResult.firstPathVertex) {
				// In this case I have also to set the value of the alpha channel to 0.0
				sampleResult.alpha = 0.f;
			}
		} else {
			const Spectrum &shadowTransparency = bsdf.GetPassThroughShadowTransparency();
			if (!sampleResult.firstPathVertex && !shadowTransparency.Black() && !pathInfo.IsSpecularPath()) {
				sampledDir = -bsdf.hitPoint.fixedDir;
				bsdfSample = shadowTransparency;
				bsdfPdfW = pathInfo.lastBSDFPdfW;
				cosSampledDir = -1.f;
				bsdfEvent = pathInfo.lastBSDFEvent;
			} else {
				bsdfSample = bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 6),
						sampler->GetSample(sampleOffset + 7),
						&bsdfPdfW, &cosSampledDir, &bsdfEvent);
				pathInfo.isPassThroughPath = false;
			}
		}

		assert (!bsdfSample.IsNaN() && !bsdfSample.IsInf() && !bsdfSample.IsNeg());
		if (bsdfSample.Black())
			break;
		assert (!isnan(bsdfPdfW) && !isinf(bsdfPdfW) && (bsdfPdfW >= 0.f));

		if (sampleResult.firstPathVertex)
			sampleResult.firstPathVertexEvent = bsdfEvent;

		pathInfo.AddVertex(bsdf, bsdfEvent, bsdfPdfW, hybridBackForwardGlossinessThreshold);

		// Russian Roulette
		float rrProb = 1.f;
		if (pathInfo.UseRR(rrDepth)) {
			 rrProb = RenderEngine::RussianRouletteProb(bsdfSample, rrImportanceCap);
			if (rrProb < sampler->GetSample(sampleOffset + 8))
				break;

			// Increase path contribution
			bsdfSample /= rrProb;
		}

		pathThroughput *= bsdfSample;
		assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

		// This is valid for irradiance AOV only if it is not a SPECULAR material and
		// first path vertex. Set or update sampleResult.irradiancePathThroughput
		if (sampleResult.firstPathVertex) {
			if (!(bsdf.GetEventTypes() & SPECULAR))
				sampleResult.irradiancePathThroughput = INV_PI * AbsDot(bsdf.hitPoint.shadeN, sampledDir) / rrProb;
			else
				sampleResult.irradiancePathThroughput = Spectrum();
		} else
			sampleResult.irradiancePathThroughput *= bsdfSample;

		eyeRay.Update(bsdf.GetRayOrigin(sampledDir), sampledDir);
	}

	sampleResult.rayCount += (float)(device->GetTotalRaysCount() - deviceRayCount);

	if (sampleResult.isHoldout) {
		sampleResult.radiance.Clear();
		sampleResult.albedo = Spectrum();
	}

	if (photonGICache && (photonGICache->GetDebugType() == PhotonGIDebugType::PGIC_DEBUG_SHOWINDIRECTPATHMIX) &&
			!photonGIShowIndirectPathMixUsed)
		sampleResult.radiance[0] = Spectrum(1.f, 0.f, 0.f);
}

//------------------------------------------------------------------------------
// RenderEyeSample
//------------------------------------------------------------------------------

void PathTracer::RenderEyeSample(IntersectionDevice *device,
		const Scene *scene, const Film *film,
		Sampler *sampler, vector<SampleResult> &sampleResults) const {
	ResetEyeSampleResults(sampleResults);

	EyePathInfo pathInfo;
	Ray eyeRay;
	GenerateEyeRay(scene->camera, film, eyeRay, pathInfo.volume, sampler, sampleResults[0]);

	RenderEyePath(device, scene, sampler, pathInfo, eyeRay, Spectrum(1.f), sampleResults);
}

//------------------------------------------------------------------------------
// RenderLightSample methods
//------------------------------------------------------------------------------

SampleResult &PathTracer::AddLightSampleResult(vector<SampleResult> &sampleResults,
		const Film *film) {
	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	SampleResult &sampleResult = sampleResults[size];
	sampleResult.Init(&lightSampleResultsChannels, film->GetRadianceGroupCount());

	return sampleResult;
}

void PathTracer::ConnectToEye(IntersectionDevice *device,
		const Scene *scene,
		const Film *film, const float time,
		const float u0, const float u1, const float u2,
		const LightSource &light, const BSDF &bsdf, 
		const Spectrum &flux, const LightPathInfo &pathInfo,
		vector<SampleResult> &sampleResults) const {
	// I don't connect camera invisible objects with the eye
	if (bsdf.IsCameraInvisible() || bsdf.IsDelta())
		return;

	Vector eyeDir(bsdf.hitPoint.p - pathInfo.lensPoint);
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	Ray eyeRay(pathInfo.lensPoint, eyeDir,
			0.f,
			eyeDistance,
			time);
	scene->camera->ClampRay(&eyeRay);
	eyeRay.UpdateMinMaxWithEpsilon();

	float filmX, filmY;
	if (scene->camera->GetSamplePosition(&eyeRay, &filmX, &filmY)) {
		BSDFEvent event;
		const Spectrum bsdfEval = bsdf.Evaluate(-eyeDir, &event);

		if (!bsdfEval.Black()) {
			// I have to flip the direction of the traced ray because
			// the information inside PathVolumeInfo are about the path from
			// the light toward the camera (i.e. ray.o would be in the wrong
			// place).
			Ray traceRay(bsdf.GetRayOrigin(-eyeRay.d), -eyeRay.d,
					eyeDistance - eyeRay.maxt,
					eyeDistance - eyeRay.mint,
					time);
			traceRay.UpdateMinMaxWithEpsilon();
			RayHit traceRayHit;

			BSDF bsdfConn;
			Spectrum connectionThroughput;
			// Create a new PathVolumeInfo for the path to the light source
			PathVolumeInfo volInfo = pathInfo.volume;
			if (!scene->Intersect(device, LIGHT_RAY | CAMERA_RAY, &volInfo, u0, &traceRay, &traceRayHit, &bsdfConn,
					&connectionThroughput)) {
				// Nothing was hit, the light path vertex is visible

				float fluxToRadianceFactor;
				scene->camera->GetPDF(eyeRay, eyeDistance, filmX, filmY, nullptr, &fluxToRadianceFactor);

				SampleResult &sampleResult = AddLightSampleResult(sampleResults, film);
				sampleResult.filmX = filmX;
				sampleResult.filmY = filmY;

				sampleResult.pixelX = Floor2UInt(filmX);
				sampleResult.pixelY = Floor2UInt(filmY);

#if !defined(NDEBUG)
				const u_int *subRegion = film->GetSubRegion();
#endif
				assert (sampleResult.pixelX >= subRegion[0]);
				assert (sampleResult.pixelX <= subRegion[1]);
				assert (sampleResult.pixelY >= subRegion[2]);
				assert (sampleResult.pixelY <= subRegion[3]);

				sampleResult.isCaustic = pathInfo.IsCausticPath(event, bsdf.GetGlossiness(), hybridBackForwardGlossinessThreshold);

				// Add radiance from the light source
				sampleResult.radiance[light.GetID()] = connectionThroughput * flux * fluxToRadianceFactor * bsdfEval;
			}
		}
	}
}

//------------------------------------------------------------------------------
// RenderLightSample
//------------------------------------------------------------------------------

void PathTracer::RenderLightSample(IntersectionDevice *device,
		const Scene *scene, const Film *film,
		Sampler *sampler, vector<SampleResult> &sampleResults,
		const ConnectToEyeCallBackType &ConnectToEyeCallBack) const {
	sampleResults.clear();

	Spectrum lightPathFlux;

	const float timeSample = sampler->GetSample(8);
	const float time = scene->camera->GenerateRayTime(timeSample);

	// Select one light source
	float lightPickPdf;
	const LightSource *light = scene->lightDefs.GetEmitLightStrategy()->
			SampleLights(sampler->GetSample(0), &lightPickPdf);

	if (light) {
		// Initialize the light path
		Ray nextEventRay;
		float lightEmitPdfW;
		lightPathFlux = light->Emit(*scene,
				time, sampler->GetSample(1), sampler->GetSample(2),
				sampler->GetSample(3), sampler->GetSample(4), sampler->GetSample(5),
				nextEventRay, lightEmitPdfW);

		if (lightPathFlux.Black())
			return;

		lightPathFlux /= lightEmitPdfW * lightPickPdf;
		assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

		LightPathInfo pathInfo;

		// Sample a point on the camera lens
		if (!scene->camera->SampleLens(time, sampler->GetSample(6), sampler->GetSample(7),
				&pathInfo.lensPoint))
			return;

		//----------------------------------------------------------------------
		// Trace the light path
		//----------------------------------------------------------------------

		while (pathInfo.depth.depth < maxPathDepth.depth) {
			const u_int sampleOffset = lightSampleBootSize +  pathInfo.depth.depth * lightSampleStepSize;

			RayHit nextEventRayHit;
			BSDF bsdf;
			Spectrum connectionThroughput;
			const bool hit = scene->Intersect(device, LIGHT_RAY | INDIRECT_RAY, &pathInfo.volume, sampler->GetSample(sampleOffset),
					&nextEventRay, &nextEventRayHit, &bsdf,
					&connectionThroughput);
			if (!hit) {
				// Ray lost in space...
				break;
			}

			// Something was hit

			lightPathFlux *= connectionThroughput;

			//--------------------------------------------------------------
			// Try to connect the light path vertex with the eye
			//--------------------------------------------------------------

			if (ConnectToEyeCallBack){
				ConnectToEyeCallBack(pathInfo, bsdf, light->GetID(), lightPathFlux, sampleResults);
			} else {
				ConnectToEye(device, scene, film,
						nextEventRay.time,
						sampler->GetSample(sampleOffset + 1),
						sampler->GetSample(sampleOffset + 2),
						sampler->GetSample(sampleOffset + 3),
						*light, bsdf, lightPathFlux, pathInfo, sampleResults);
			}

			if (pathInfo.depth.depth == maxPathDepth.depth - 1)
				break;

			//--------------------------------------------------------------
			// Build the next vertex path ray
			//--------------------------------------------------------------

			float bsdfPdf;
			Vector sampledDir;
			BSDFEvent bsdfEvent;
			float cosSampleDir;
			Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler->GetSample(sampleOffset + 4),
						sampler->GetSample(sampleOffset + 5),
					&bsdfPdf, &cosSampleDir, &bsdfEvent);
			if (bsdfSample.Black())
				break;

			pathInfo.AddVertex(bsdf, bsdfEvent, hybridBackForwardGlossinessThreshold);

			// If it isn't anymore a (nearly) specular path, I can stop
			if (hybridBackForwardEnable && !pathInfo.IsSpecularPath() &&
					// This condition is added to "stabilize" Metropolis sampler
					// used in light tracing part of hybrid rendering. In this case
					// I render also some not-caustic sample to make an "easy"
					// computation of the avg. image luminance.
					(pathInfo.depth.diffuseDepth + pathInfo.depth.glossyDepth > 1))
				break;

			// Russian Roulette
			if (pathInfo.UseRR(rrDepth)) {
				// Russian Roulette
				const float rrProb = RenderEngine::RussianRouletteProb(bsdfSample, rrImportanceCap);
				if (rrProb < sampler->GetSample(sampleOffset + 6))
					break;

				// Increase path contribution
				bsdfSample /= rrProb;
			}

			lightPathFlux *= bsdfSample;
			assert (!lightPathFlux.IsNaN() && !lightPathFlux.IsInf());

			nextEventRay.Update(bsdf.GetRayOrigin(sampledDir), sampledDir);
		}
	}
}

//------------------------------------------------------------------------------
// RenderSample
//------------------------------------------------------------------------------

bool PathTracer::HasToRenderEyeSample(PathTracerThreadState &state) const {
	// Check if I have to trace an eye or light path
	if (hybridBackForwardEnable) {
		const double ratio = state.eyeSampleCount / state.lightSampleCount;
		if ((hybridBackForwardPartition == 1.f) ||
				(ratio < hybridBackForwardPartition)) {
			// Trace an eye path
			state.eyeSampleCount += 1.0;
			return true;
		} else {
			// Trace a light path
			state.lightSampleCount += 1.0;
			return false;
		}
	} else {
		state.eyeSampleCount += 1.0;
		return true;
	}
}

void PathTracer::ApplyVarianceClamp(const PathTracerThreadState &state,
		vector<SampleResult> &sampleResults) const {
	// Variance clamping
	if (state.varianceClamping->hasClamping()) {
		for(u_int i = 0; i < sampleResults.size(); ++i) {
			SampleResult &sampleResult = sampleResults[i];

			// I clamp only eye paths samples (variance clamping would cut
			// SDS path values due to high scale of PSR samples)
			if (sampleResult.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED))
				state.varianceClamping->Clamp(*state.film, sampleResult);
		}
	}
}

void PathTracer::RenderSample(PathTracerThreadState &state) const {
	// Check if I have to trace an eye or light path
	Sampler *sampler;
	vector<SampleResult> *sampleResults;
	if (HasToRenderEyeSample(state)) {
		// Trace an eye path
		sampler = state.eyeSampler;
		sampleResults = &state.eyeSampleResults;
	} else {
		// Trace a light path
		sampler = state.lightSampler;
		sampleResults = &state.lightSampleResults;
	}

	if (sampler == state.eyeSampler)
		RenderEyeSample(state.device, state.scene, state.film, state.eyeSampler, state.eyeSampleResults);
	else
		RenderLightSample(state.device, state.scene, state.film, state.lightSampler, state.lightSampleResults);

	// Variance clamping
	ApplyVarianceClamp(state, *sampleResults);

	sampler->NextSample(*sampleResults);
}

//------------------------------------------------------------------------------
// ParseOptions
//------------------------------------------------------------------------------

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
	
	hybridBackForwardEnable = cfg.Get(defaultProps.Get("path.hybridbackforward.enable")).Get<bool>();
	// hybridBackForwardGlossinessThreshold is used by LIGHTCPU when PSR is enabled
	// so I have always to set the value
	hybridBackForwardGlossinessThreshold = .05f;
	if (hybridBackForwardEnable) {
		hybridBackForwardPartition = Clamp(cfg.Get(defaultProps.Get("path.hybridbackforward.partition")).Get<float>(), 0.f, 1.f);
		hybridBackForwardGlossinessThreshold = Clamp(cfg.Get(defaultProps.Get("path.hybridbackforward.glossinessthreshold")).Get<float>(), 0.f, 1.f);
	}

	// Update eye sample size
	eyeSampleBootSize = 5;
	eyeSampleStepSize = 9;
	eyeSampleSize = 
		eyeSampleBootSize + // To generate eye ray
		(maxPathDepth.depth + 1) * eyeSampleStepSize; // For each path vertex
	
	// Update light sample size
	lightSampleBootSize = 9;
	lightSampleStepSize = 7;
	lightSampleSize = 
		lightSampleBootSize + // To generate eye ray
		maxPathDepth.depth * lightSampleStepSize; // For each path vertex
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
			cfg.Get(GetDefaultProps().Get("path.hybridbackforward.enable")) <<
			cfg.Get(GetDefaultProps().Get("path.hybridbackforward.partition")) <<
			cfg.Get(GetDefaultProps().Get("path.hybridbackforward.glossinessthreshold")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")) <<
			cfg.Get(GetDefaultProps().Get("path.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("path.forceblackbackground.enable")) <<
			Sampler::ToProperties(cfg);

	return props;
}

const Properties &PathTracer::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("path.hybridbackforward.enable")(false) <<
			Property("path.hybridbackforward.partition")(0.8) <<
			Property("path.hybridbackforward.glossinessthreshold")(.05f) <<
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
