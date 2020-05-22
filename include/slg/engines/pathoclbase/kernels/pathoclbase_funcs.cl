#line 2 "pathoclbase_funcs.cl"

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

// List of symbols defined at compile time:
//  PARAM_RAY_EPSILON_MIN
//  PARAM_RAY_EPSILON_MAX

/*void MangleMemory(__global unsigned char *ptr, const size_t size) {
	Seed seed;
	Rnd_Init(7 + get_global_id(0), &seed);

	for (uint i = 0; i < size; ++i)
		*ptr++ = (unsigned char)(Rnd_UintValue(&seed) & 0xff);
}*/

//------------------------------------------------------------------------------
// Init functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void InitSampleResult(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		__global float *pixelFilterDistribution
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	SampleResult_Init(&taskConfig->film, sampleResult);

	float filmX = Sampler_GetSample(taskConfig, IDX_SCREEN_X SAMPLER_PARAM);
	float filmY = Sampler_GetSample(taskConfig, IDX_SCREEN_Y SAMPLER_PARAM);

	// Metropolis return IDX_SCREEN_X and IDX_SCREEN_Y between [0.0, 1.0] instead
	// that in film pixels like RANDOM and SOBOL samplers
	if (taskConfig->sampler.type == METROPOLIS) {
		filmX = filmSubRegion0 + filmX * (filmSubRegion1 - filmSubRegion0 + 1);
		filmY = filmSubRegion2 + filmY * (filmSubRegion3 - filmSubRegion2 + 1);
	}

	const uint pixelX = min(Floor2UInt(filmX), filmSubRegion1);
	const uint pixelY = min(Floor2UInt(filmY), filmSubRegion3);
	const float uSubPixelX = filmX - pixelX;
	const float uSubPixelY = filmY - pixelY;

	sampleResult->pixelX = pixelX;
	sampleResult->pixelY = pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(&taskConfig->pixelFilter, pixelFilterDistribution,
			uSubPixelX, uSubPixelY, &distX, &distY);

	sampleResult->filmX = pixelX + .5f + distX;
	sampleResult->filmY = pixelY + .5f + distY;

	sampleResult->directShadowMask = 1.f;
	sampleResult->indirectShadowMask = 1.f;

	sampleResult->lastPathVertex = (taskConfig->pathTracer.maxPathDepth.depth == 1);
}

OPENCL_FORCE_INLINE void GenerateEyePath(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global GPUTaskDirectLight *taskDirectLight,
		__global GPUTaskState *taskState,
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		__global float *pixelFilterDistribution,
		__global Ray *ray,
		__global EyePathInfo *pathInfo
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		// cameraFilmWidth/cameraFilmHeight and filmWidth/filmHeight are usually
		// the same. They are different when doing tile rendering
		, const uint cameraFilmWidth, const uint cameraFilmHeight,
		const uint tileStartX, const uint tileStartY
#endif
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	EyePathInfo_Init(pathInfo);

	InitSampleResult(taskConfig,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1,
			filmSubRegion2, filmSubRegion3,
			pixelFilterDistribution
			SAMPLER_PARAM);

	// Generate the came ray
	const float timeSample = Sampler_GetSample(taskConfig, IDX_EYE_TIME SAMPLER_PARAM);

	const float dofSampleX = Sampler_GetSample(taskConfig, IDX_DOF_X SAMPLER_PARAM);
	const float dofSampleY = Sampler_GetSample(taskConfig, IDX_DOF_Y SAMPLER_PARAM);

#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
	Camera_GenerateRay(camera, cameraFilmWidth, cameraFilmHeight,
			ray,
			&pathInfo->volume,
			sampleResult->filmX + tileStartX, sampleResult->filmY + tileStartY,
			timeSample,
			dofSampleX, dofSampleY);
#else
	Camera_GenerateRay(camera, filmWidth, filmHeight,
			ray,
			&pathInfo->volume,
			sampleResult->filmX, sampleResult->filmY,
			timeSample,
			dofSampleX, dofSampleY);
#endif

	// Initialize the path state
	taskState->state = MK_RT_NEXT_VERTEX;
	VSTORE3F(WHITE, taskState->throughput.c);
	taskState->albedoToDo = true;
	taskState->photonGICacheEnabledOnLastHit = false;
	taskState->photonGICausticCacheUsed = false;
	taskState->photonGIShowIndirectPathMixUsed = false;
	// Initialize the trough a shadow transparency flag used by Scene_Intersect()
	taskState->throughShadowTransparency = false;

	// Initialize the pass-through event seed
	//
	// Note: using the IDX_PASSTHROUGH of path depth 0
	const float passThroughEvent = Sampler_GetSample(taskConfig, IDX_BSDF_OFFSET + IDX_PASSTHROUGH SAMPLER_PARAM);
	Seed seedPassThroughEvent;
	Rnd_InitFloat(passThroughEvent, &seedPassThroughEvent);
	taskState->seedPassThroughEvent = seedPassThroughEvent;
}

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE bool CheckDirectHitVisibilityFlags(__global const LightSource* restrict lightSource,
		__global PathDepthInfo *depthInfo,
		const BSDFEvent lastBSDFEvent) {
	if (depthInfo->depth == 0)
		return true;

	if ((lastBSDFEvent & DIFFUSE) && (lightSource->visibility & DIFFUSE))
		return true;
	if ((lastBSDFEvent & GLOSSY) && (lightSource->visibility & GLOSSY))
		return true;
	if ((lastBSDFEvent & SPECULAR) && (lightSource->visibility & SPECULAR))
		return true;

	return false;
}

OPENCL_FORCE_INLINE void DirectHitInfiniteLight(__constant const Film* restrict film,
		__global EyePathInfo *pathInfo, __global const Spectrum* restrict pathThroughput,
		const __global Ray *ray, __global const BSDF *bsdf, __global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	// If the material is shadow transparent, Direct Light sampling
	// will take care of transporting all emitted light
	if (bsdf && bsdf->hitPoint.throughShadowTransparency)
		return;

	const float3 throughput = VLOAD3F(pathThroughput->c);

	for (uint i = 0; i < envLightCount; ++i) {
		__global const LightSource* restrict light = &lights[envLightIndices[i]];

		// Check if the light source is visible according the settings
		if (!CheckDirectHitVisibilityFlags(light, &pathInfo->depth, pathInfo->lastBSDFEvent))
			continue;

		float directPdfW;
		const float3 envRadiance = EnvLight_GetRadiance(light, bsdf,
				-VLOAD3F(&ray->d.x), &directPdfW
				LIGHTS_PARAM);

		if (!Spectrum_IsBlack(envRadiance)) {
			float weight;
			if (!(pathInfo->lastBSDFEvent & SPECULAR)) {
				const float lightPickProb = LightStrategy_SampleLightPdf(lightsDistribution,
						dlscAllEntries,
						dlscDistributions, dlscBVHNodes,
						dlscRadius2, dlscNormalCosAngle,
						VLOAD3F(&ray->o.x), VLOAD3F(&pathInfo->lastShadeN.x),
						pathInfo->lastFromVolume,
						light->lightSceneIndex);

				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(pathInfo->lastBSDFPdfW, directPdfW * lightPickProb);
			} else
				weight = 1.f;
			
			SampleResult_AddEmission(film, sampleResult, light->lightID, throughput, weight * envRadiance);
		}
	}
}

OPENCL_FORCE_INLINE void DirectHitFiniteLight(__constant const Film* restrict film,
		__global EyePathInfo *pathInfo,
		__global const Spectrum* restrict pathThroughput, const __global Ray *ray,
		const float distance, __global const BSDF *bsdf,
		__global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	__global const LightSource* restrict light = &lights[bsdf->triangleLightSourceIndex];

	// Check if the light source is visible according the settings
	if (!CheckDirectHitVisibilityFlags(light, &pathInfo->depth, pathInfo->lastBSDFEvent) ||
			// If the material is shadow transparent, Direct Light sampling
			// will take care of transporting all emitted light
			bsdf->hitPoint.throughShadowTransparency)
		return;
	
	float directPdfA;
	const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf, &directPdfA
			LIGHTS_PARAM);

	if (!Spectrum_IsBlack(emittedRadiance)) {
		// Add emitted radiance
		float weight = 1.f;
		if (!(pathInfo->lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = LightStrategy_SampleLightPdf(lightsDistribution,
					dlscAllEntries,
					dlscDistributions, dlscBVHNodes,
					dlscRadius2, dlscNormalCosAngle,
					VLOAD3F(&ray->o.x), VLOAD3F(&pathInfo->lastShadeN.x),
					pathInfo->lastFromVolume,
					light->lightSceneIndex);

#if !defined(RENDER_ENGINE_RTPATHOCL)
			// This is a specific check to avoid fireflies with DLSC
			if ((lightPickProb == 0.f) && light->isDirectLightSamplingEnabled && dlscAllEntries)
				return;
#endif
			
			const float directPdfW = PdfAtoW(directPdfA, distance,
					fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

			// MIS between BSDF sampling and direct light sampling
			//
			// Note: mats[bsdf->materialIndex].avgPassThroughTransparency = lightSource->GetAvgPassThroughTransparency()
			weight = PowerHeuristic(pathInfo->lastBSDFPdfW * Light_GetAvgPassThroughTransparency(light LIGHTS_PARAM), directPdfW * lightPickProb);
		}

		SampleResult_AddEmission(film, sampleResult, BSDF_GetLightID(bsdf
				MATERIALS_PARAM), VLOAD3F(pathThroughput->c), weight * emittedRadiance);
	}
}

OPENCL_FORCE_INLINE float RussianRouletteProb(const float importanceCap, const float3 color) {
	return clamp(Spectrum_Filter(color), importanceCap, 1.f);
}

OPENCL_FORCE_INLINE bool DirectLight_Illuminate(
		__global const BSDF *bsdf,
		__global Ray *shadowRay,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		__global HitPoint *tmpHitPoint,
		const float time, const float u0, const float u1, const float u2,
		const float lightPassThroughEvent,
		__global DirectLightIlluminateInfo *info
		LIGHTS_PARAM_DECL) {
	// Select the light strategy to use
	__global const float* restrict lightDist = BSDF_IsShadowCatcherOnlyInfiniteLights(bsdf MATERIALS_PARAM) ?
		infiniteLightSourcesDistribution : lightsDistribution;

	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = LightStrategy_SampleLights(lightDist,
			dlscAllEntries,
			dlscDistributions, dlscBVHNodes,
			dlscRadius2, dlscNormalCosAngle,
			VLOAD3F(&bsdf->hitPoint.p.x), BSDF_GetLandingGeometryN(bsdf), 
			bsdf->isVolume,
			u0, &lightPickPdf);
	if ((lightIndex == NULL_INDEX) || (lightPickPdf <= 0.f))
		return false;

	__global const LightSource* restrict light = &lights[lightIndex];

	info->lightIndex = lightIndex;
	info->lightID = light->lightID;
	info->pickPdf = lightPickPdf;

	// Illuminate the point
	float3 lightRayDir;
	float distance, directPdfW;
	const float3 lightRadiance = Light_Illuminate(
			&lights[lightIndex],
			bsdf,
			time, u1, u2,
			lightPassThroughEvent,
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			tmpHitPoint,		
			shadowRay, &directPdfW
			LIGHTS_PARAM);
	
	if (Spectrum_IsBlack(lightRadiance))
		return false;
	else {
		info->directPdfW = directPdfW;
		VSTORE3F(lightRadiance, info->lightRadiance.c);
		VSTORE3F(lightRadiance, info->lightIrradiance.c);
		return true;
	}
}

OPENCL_FORCE_INLINE bool DirectLight_BSDFSampling(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global DirectLightIlluminateInfo *info,
		const float time,
		const bool lastPathVertex,
		__global EyePathInfo *pathInfo,
		__global PathDepthInfo *tmpDepthInfo,
		__global const BSDF *bsdf,
		const float3 shadowRayDir
		LIGHTS_PARAM_DECL) {
	// Sample the BSDF
	BSDFEvent event;
	float bsdfPdfW;
	const float3 bsdfEval = BSDF_Evaluate(bsdf,
			shadowRayDir, &event, &bsdfPdfW
			MATERIALS_PARAM);

	if (Spectrum_IsBlack(bsdfEval) ||
			(taskConfig->pathTracer.hybridBackForward.enabled &&
			EyePathInfo_IsCausticPathWithEvent(pathInfo, event,
				BSDF_GetGlossiness(bsdf MATERIALS_PARAM),
				taskConfig->pathTracer.hybridBackForward.glossinessThreshold))
			)
		return false;

	// Create a new DepthInfo for the path to the light source
	//
	// Note: I was using a local variable before to save, use and than restore
	// the depthInfo variable but it was triggering a AMD OpenCL compiler bug.
	*tmpDepthInfo = pathInfo->depth;
	PathDepthInfo_IncDepths(tmpDepthInfo, event);

	const float directLightSamplingPdfW = info->directPdfW * info->pickPdf;
	const float factor = 1.f / directLightSamplingPdfW;

	// Russian Roulette
	bsdfPdfW *= (PathDepthInfo_GetRRDepth(tmpDepthInfo) >= taskConfig->pathTracer.rrDepth) ?
		RussianRouletteProb(taskConfig->pathTracer.rrImportanceCap, bsdfEval) :
		1.f;

	// Account for material transparency
	__global const LightSource* restrict light = &lights[info->lightIndex];
	bsdfPdfW *= Light_GetAvgPassThroughTransparency(light
			LIGHTS_PARAM);
	
	// MIS between direct light sampling and BSDF sampling
	//
	// Note: I have to avoiding MIS on the last path vertex

	const bool misEnabled = !lastPathVertex &&
			Light_IsEnvOrIntersectable(light) &&
			CheckDirectHitVisibilityFlags(light, tmpDepthInfo, event);

	const float weight = misEnabled ? PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

	const float3 lightRadiance = VLOAD3F(info->lightRadiance.c);
	VSTORE3F(bsdfEval * (weight * factor) * lightRadiance, info->lightRadiance.c);
	VSTORE3F(factor * lightRadiance, info->lightIrradiance.c);

	return true;
}

//------------------------------------------------------------------------------
// Kernel parameters
//------------------------------------------------------------------------------

#define KERNEL_ARGS_VOLUMES \
		, __global PathVolumeInfo *directLightVolInfos

#define KERNEL_ARGS_INFINITELIGHTS \
		, const float worldCenterX \
		, const float worldCenterY \
		, const float worldCenterZ \
		, const float worldRadius

#define KERNEL_ARGS_NORMALS_BUFFER \
		, __global const Normal* restrict vertNormals
#define KERNEL_ARGS_TRINORMALS_BUFFER \
		, __global const Normal* restrict triNormals
#define KERNEL_ARGS_UVS_BUFFER \
		, __global const UV* restrict vertUVs
#define KERNEL_ARGS_COLS_BUFFER \
		, __global const Spectrum* restrict vertCols
#define KERNEL_ARGS_ALPHAS_BUFFER \
		, __global const float* restrict vertAlphas
#define KERNEL_ARGS_VERTEXAOVS_BUFFER \
		, __global const float* restrict vertexAOVs
#define KERNEL_ARGS_TRIAOVS_BUFFER \
		, __global const float* restrict triAOVs

#define KERNEL_ARGS_ENVLIGHTS \
		, __global const uint* restrict envLightIndices \
		, const uint envLightCount

#define KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict envLightDistribution

#define KERNEL_ARGS_IMAGEMAPS_PAGES \
		, __global const ImageMap* restrict imageMapDescs \
		, __global const float* restrict imageMapBuff0 \
		, __global const float* restrict imageMapBuff1 \
		, __global const float* restrict imageMapBuff2 \
		, __global const float* restrict imageMapBuff3 \
		, __global const float* restrict imageMapBuff4 \
		, __global const float* restrict imageMapBuff5 \
		, __global const float* restrict imageMapBuff6 \
		, __global const float* restrict imageMapBuff7

#define KERNEL_ARGS_FAST_PIXEL_FILTER \
		, __global float *pixelFilterDistribution

#define KERNEL_ARGS_PHOTONGI \
		, __global const RadiancePhoton* restrict pgicRadiancePhotons \
		, uint pgicLightGroupCounts \
		, __global const Spectrum* restrict pgicRadiancePhotonsValues \
		, __global const IndexBVHArrayNode* restrict pgicRadiancePhotonsBVHNodes \
		, __global const Photon* restrict pgicCausticPhotons \
		, __global const IndexBVHArrayNode* restrict pgicCausticPhotonsBVHNodes

#define KERNEL_ARGS \
		__constant const GPUTaskConfiguration* restrict taskConfig \
		, __global GPUTask *tasks \
		, __global GPUTaskDirectLight *tasksDirectLight \
		, __global GPUTaskState *tasksState \
		, __global GPUTaskStats *taskStats \
		KERNEL_ARGS_FAST_PIXEL_FILTER \
		, __global void *samplerSharedDataBuff \
		, __global void *samplesBuff \
		, __global float *samplesDataBuff \
		, __global SampleResult *sampleResultsBuff \
		, __global EyePathInfo *eyePathInfos \
		KERNEL_ARGS_VOLUMES \
		, __global Ray *rays \
		, __global RayHit *rayHits \
		/* Film parameters */ \
		KERNEL_ARGS_FILM \
		/* Scene parameters */ \
		KERNEL_ARGS_INFINITELIGHTS \
		, __global const Material* restrict mats \
		, __global const MaterialEvalOp* restrict matEvalOps \
		, __global float *matEvalStacks \
		, const uint maxMaterialEvalStackSize \
		, __global const Texture* restrict texs \
		, __global const TextureEvalOp* restrict texEvalOps \
		, __global float *texEvalStacks \
		, const uint maxTextureEvalStackSize \
		, __global const SceneObject* restrict sceneObjs \
		, __global const ExtMesh* restrict meshDescs \
		, __global const Point* restrict vertices \
		KERNEL_ARGS_NORMALS_BUFFER \
		KERNEL_ARGS_TRINORMALS_BUFFER \
		KERNEL_ARGS_UVS_BUFFER \
		KERNEL_ARGS_COLS_BUFFER \
		KERNEL_ARGS_ALPHAS_BUFFER \
		KERNEL_ARGS_VERTEXAOVS_BUFFER \
		KERNEL_ARGS_TRIAOVS_BUFFER \
		, __global const Triangle* restrict triangles \
		, __global const InterpolatedTransform* restrict interpolatedTransforms \
		, __global const Camera* restrict camera \
		/* Lights */ \
		, __global const LightSource* restrict lights \
		KERNEL_ARGS_ENVLIGHTS \
		, __global const uint* restrict lightIndexOffsetByMeshIndex \
		, __global const uint* restrict lightIndexByTriIndex \
		KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict lightsDistribution \
		, __global const float* restrict infiniteLightSourcesDistribution \
		, __global const DLSCacheEntry* restrict dlscAllEntries \
		, __global const float* restrict dlscDistributions \
		, __global const IndexBVHArrayNode* restrict dlscBVHNodes \
		, const float dlscRadius2 \
		, const float dlscNormalCosAngle \
		, __global const ELVCacheEntry* restrict elvcAllEntries \
		, __global const float* restrict elvcDistributions \
		, __global const uint* restrict elvcTileDistributionOffsets \
		, __global const IndexBVHArrayNode* restrict elvcBVHNodes \
		, const float elvcRadius2 \
		, const float elvcNormalCosAngle \
		, const uint elvcTilesXCount \
		, const uint elvcTilesYCount \
		/* Images */ \
		KERNEL_ARGS_IMAGEMAPS_PAGES \
		KERNEL_ARGS_PHOTONGI


//------------------------------------------------------------------------------
// To initialize image maps page pointer table
//------------------------------------------------------------------------------

#define INIT_IMAGEMAPS_PAGES \
	__global const float* restrict imageMapBuff[8]; \
	imageMapBuff[0] = imageMapBuff0; \
	imageMapBuff[1] = imageMapBuff1; \
	imageMapBuff[2] = imageMapBuff2; \
	imageMapBuff[3] = imageMapBuff3; \
	imageMapBuff[4] = imageMapBuff4; \
	imageMapBuff[5] = imageMapBuff5; \
	imageMapBuff[6] = imageMapBuff6; \
	imageMapBuff[7] = imageMapBuff7;

//------------------------------------------------------------------------------
// Init Kernels
//------------------------------------------------------------------------------

__kernel void InitSeed(__global GPUTask *tasks,
		const uint seedBase) {
	const size_t gid = get_global_id(0);

	// Initialize random number generator

	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Save the seed
	__global GPUTask *task = &tasks[gid];
	task->seed = seed;
}

__kernel void Init(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global GPUTask *tasks,
		__global GPUTaskDirectLight *tasksDirectLight,
		__global GPUTaskState *tasksState,
		__global GPUTaskStats *taskStats,
		__global void *samplerSharedDataBuff,
		__global void *samplesBuff,
		__global float *samplesDataBuff,
		__global SampleResult *sampleResultsBuff,
		__global EyePathInfo *eyePathInfos,
		__global float *pixelFilterDistribution,
		__global Ray *rays,
		__global Camera *camera
		KERNEL_ARGS_FILM
		) {
	const size_t gid = get_global_id(0);

	__global GPUTaskState *taskState = &tasksState[gid];

#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
	__global TilePathSamplerSharedData *samplerSharedData = (__global TilePathSamplerSharedData *)samplerSharedDataBuff;

	if (gid >= filmWidth * filmHeight * Sqr(samplerSharedData->aaSamples)) {
		taskState->state = MK_DONE;
		// Mark the ray like like one to NOT trace
		rays[gid].flags = RAY_FLAGS_MASKED;

		return;
	}
#endif

	// Initialize the task
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	// Initialize the sample and path
	const bool validSample = Sampler_Init(taskConfig,
			filmNoise,
			filmUserImportance,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3
			SAMPLER_PARAM);

	if (validSample) {
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		__global TilePathSamplerSharedData *samplerSharedData = (__global TilePathSamplerSharedData *)samplerSharedDataBuff;
		const uint cameraFilmWidth = samplerSharedData->cameraFilmWidth;
		const uint cameraFilmHeight = samplerSharedData->cameraFilmHeight;
		const uint tileStartX = samplerSharedData->tileStartX;
		const uint tileStartY =  samplerSharedData->tileStartY;
#endif

		// Generate the eye path
		GenerateEyePath(taskConfig,
				taskDirectLight, taskState,
				camera,
				filmWidth, filmHeight,
				filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3,
				pixelFilterDistribution,
				&rays[gid],
				&eyePathInfos[gid]
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
				, cameraFilmWidth, cameraFilmHeight,
				tileStartX, tileStartY
#endif
				SAMPLER_PARAM);
	} else {
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		taskState->state = MK_DONE;
#else
		taskState->state = MK_GENERATE_CAMERA_RAY;
#endif
		// Mark the ray like like one to NOT trace
		rays[gid].flags = RAY_FLAGS_MASKED;
	}

	// Save the seed
	task->seed = seedValue;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->sampleCount = 0;
}
