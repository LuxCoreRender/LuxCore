#line 2 "pathoclbase_funcs.cl"

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

// List of symbols defined at compile time:
//  PARAM_RAY_EPSILON_MIN
//  PARAM_RAY_EPSILON_MAX
//  PARAM_HAS_IMAGEMAPS
//  PARAM_HAS_PASSTHROUGH
//  PARAM_USE_PIXEL_ATOMICS
//  PARAM_HAS_BUMPMAPS
//  PARAM_ACCEL_BVH or PARAM_ACCEL_MBVH or PARAM_ACCEL_QBVH or PARAM_ACCEL_MQBVH
//  PARAM_LIGHT_WORLD_RADIUS_SCALE
//  PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR
//  PARAM_HAS_VOLUMEs (and SCENE_DEFAULT_VOLUME_INDEX)

// To enable single material support
//  PARAM_ENABLE_MAT_MATTE
//  PARAM_ENABLE_MAT_MIRROR
//  PARAM_ENABLE_MAT_GLASS
//  PARAM_ENABLE_MAT_ARCHGLASS
//  PARAM_ENABLE_MAT_MIX
//  PARAM_ENABLE_MAT_NULL
//  PARAM_ENABLE_MAT_MATTETRANSLUCENT
//  PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT
//  PARAM_ENABLE_MAT_GLOSSY2
//  PARAM_ENABLE_MAT_METAL2
//  PARAM_ENABLE_MAT_ROUGHGLASS
//  PARAM_ENABLE_MAT_CLOTH
//  PARAM_ENABLE_MAT_CARPAINT
//  PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT
//  PARAM_ENABLE_MAT_GLOSSYCOATING
//  PARAM_ENABLE_MAT_CLEAR_VOL
/// etc.

// To enable single texture support
//  PARAM_ENABLE_TEX_CONST_FLOAT
//  PARAM_ENABLE_TEX_CONST_FLOAT3
//  PARAM_ENABLE_TEX_CONST_FLOAT4
//  PARAM_ENABLE_TEX_IMAGEMAP
//  PARAM_ENABLE_TEX_SCALE
//  etc.

// Film related parameters:
//  PARAM_FILM_RADIANCE_GROUP_COUNT
//  PARAM_FILM_CHANNELS_HAS_ALPHA
//  PARAM_FILM_CHANNELS_HAS_DEPTH
//  PARAM_FILM_CHANNELS_HAS_POSITION
//  PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL
//  PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID
//  PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE
//  PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY
//  PARAM_FILM_CHANNELS_HAS_EMISSION
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK (and PARAM_FILM_MASK_MATERIAL_ID)
//  PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID (and PARAM_FILM_BY_MATERIAL_ID)
//  PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK
//  PARAM_FILM_CHANNELS_HAS_UV
//  PARAM_FILM_CHANNELS_HAS_RAYCOUNT
//  PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID (and PARAM_FILM_BY_MATERIAL_ID)
//  PARAM_FILM_CHANNELS_HAS_IRRADIANCE
//  PARAM_FILM_CHANNELS_HAS_OBJECT_ID
//  PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK (and PARAM_FILM_MASK_OBJECT_ID)
//  PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID (and PARAM_FILM_BY_OBJECT_ID)
//  PARAM_FILM_CHANNELS_HAS_SAMPLECOUNT
//  PARAM_FILM_CHANNELS_HAS_CONVERGENCE
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR
//
//  PARAM_FILM_DENOISER

// (optional)
//  PARAM_HAS_INFINITELIGHT
//  PARAM_HAS_SUNLIGHT
//  PARAM_HAS_SKYLIGHT2
//  PARAM_HAS_POINTLIGHT
//  PARAM_HAS_MAPPOINTLIGHT
//  PARAM_HAS_SPOTLIGHT
//  PARAM_HAS_PROJECTIONLIGHT
//  PARAM_HAS_CONSTANTINFINITELIGHT
//  PARAM_HAS_SHARPDISTANTLIGHT
//  PARAM_HAS_DISTANTLIGHT
//  PARAM_HAS_LASERLIGHT
//  PARAM_HAS_TRIANGLELIGHT
//  PARAM_HAS_ENVLIGHTS (if it has any env. light)

// List of symbols defined at compile time:
//  PARAM_MAX_PATH_DEPTH
//  PARAM_MAX_PATH_DEPTH_DIFFUSE
//  PARAM_MAX_PATH_DEPTH_GLOSSY
//  PARAM_MAX_PATH_DEPTH_SPECULAR
//  PARAM_RR_DEPTH
//  PARAM_RR_CAP

// (optional)
//  PARAM_CAMERA_TYPE (0 = Perspective, 1 = Orthographic, 2 = Stereo)
//  PARAM_CAMERA_ENABLE_CLIPPING_PLANE
//  PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL

// (optional)
//  PARAM_IMAGE_FILTER_TYPE (0 = No filter, 1 = Box, 2 = Gaussian, 3 = Mitchell, 4 = Blackman-Harris)
//  PARAM_IMAGE_FILTER_WIDTH_X
//  PARAM_IMAGE_FILTER_WIDTH_Y
//  PARAM_IMAGE_FILTER_PIXEL_WIDTH_X
//  PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y
// (Box filter)
// (Gaussian filter)
//  PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA
// (Mitchell filter)
//  PARAM_IMAGE_FILTER_MITCHELL_B
//  PARAM_IMAGE_FILTER_MITCHELL_C
// (MitchellSS filter)
//  PARAM_IMAGE_FILTER_MITCHELL_B
//  PARAM_IMAGE_FILTER_MITCHELL_C
//  PARAM_IMAGE_FILTER_MITCHELL_A0
//  PARAM_IMAGE_FILTER_MITCHELL_A1

// (optional)
//  PARAM_SAMPLER_TYPE (0 = Inlined Random, 1 = Metropolis, 2 = Sobol, 3 = TilePathSampler)
// (Metropolis)
//  PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE
//  PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT
//  PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE
// (Sobol)
//  PARAM_SAMPLER_SOBOL_STARTOFFSET

/*void MangleMemory(__global unsigned char *ptr, const size_t size) {
	Seed seed;
	Rnd_Init(7 + get_global_id(0), &seed);

	for (uint i = 0; i < size; ++i)
		*ptr++ = (unsigned char)(Rnd_UintValue(&seed) & 0xff);
}*/

OPENCL_FORCE_NOT_INLINE bool Scene_Intersect(
		const bool cameraRay,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfo,
		__global HitPoint *tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThrough,
#endif
		__global Ray *ray,
		__global RayHit *rayHit,
		__global BSDF *bsdf,
		float3 *connectionThroughput,  const float3 pathThroughput,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
		__global const uint* restrict lightIndexOffsetByMeshIndex,
		__global const uint* restrict lightIndexByTriIndex,
		__global const Point* restrict vertices,
		__global const Vector* restrict vertNormals,
		__global const UV* restrict vertUVs,
		__global const Spectrum* restrict vertCols,
		__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles
		MATERIALS_PARAM_DECL
		) {
	*connectionThroughput = WHITE;
	const bool hit = (rayHit->meshIndex != NULL_INDEX);

	const uint sceneObjectIndex = rayHit->meshIndex;
	if (cameraRay && hit && sceneObjs[rayHit->meshIndex].cameraInvisible) {
		ray->mint = rayHit->t + MachineEpsilon_E(rayHit->t);

		// A safety check
		if (ray->mint >= ray->maxt)
			return false;
		else
			return true;
	}

#if defined(PARAM_HAS_VOLUMES)
	uint rayVolumeIndex = volInfo->currentVolumeIndex;
#endif
	if (hit) {
		// Initialize the BSDF of the hit point
		BSDF_Init(bsdf,
				meshDescs,
				sceneObjs,
				lightIndexOffsetByMeshIndex,
				lightIndexByTriIndex,
				vertices,
				vertNormals,
				vertUVs,
				vertCols,
				vertAlphas,
				triangles, ray, rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
				, passThrough
#endif
#if defined(PARAM_HAS_VOLUMES)
				, volInfo
#endif
				MATERIALS_PARAM
				);

#if defined(PARAM_HAS_VOLUMES)
		rayVolumeIndex = bsdf->hitPoint.intoObject ? bsdf->hitPoint.exteriorVolumeIndex : bsdf->hitPoint.interiorVolumeIndex;
#endif
	}
#if defined(PARAM_HAS_VOLUMES)
	else if (rayVolumeIndex == NULL_INDEX) {
		// No volume information, I use the default volume
		rayVolumeIndex = SCENE_DEFAULT_VOLUME_INDEX;
	}
#endif

#if defined(PARAM_HAS_VOLUMES)
	// Check if there is volume scatter event
	if (rayVolumeIndex != NULL_INDEX) {
		// This applies volume transmittance too
		// Note: by using passThrough here, I introduce subtle correlation
		// between scattering events and pass-through events
		float3 connectionEmission = BLACK;

		const float t = Volume_Scatter(&mats[rayVolumeIndex], ray,
				hit ? rayHit->t : ray->maxt,
				passThrough, volInfo->scatteredStart,
				connectionThroughput, &connectionEmission,
				tmpHitPoint
				TEXTURES_PARAM);

		// Add the volume emitted light to the appropriate light group
		if (!Spectrum_IsBlack(connectionEmission) && sampleResult)
			SampleResult_AddEmission(sampleResult, BSDF_GetLightID(bsdf
				MATERIALS_PARAM),
					pathThroughput, connectionEmission);

		if (t > 0.f) {
			// There was a volume scatter event

			// I have to set RayHit fields even if there wasn't a real
			// ray hit
			rayHit->t = t;
			// This is a trick in order to have RayHit::Miss() return
			// false. I assume 0xfffffffeu will trigger a memory fault if
			// used (and the bug will be noticed)
			rayHit->meshIndex = 0xfffffffeu;

			BSDF_InitVolume(bsdf, mats, ray, rayVolumeIndex, t, passThrough);
			volInfo->scatteredStart = true;

			return false;
		}
	}
#endif

	if (hit) {
		// Check if the volume priority system tells me to continue to trace the ray
		bool continueToTrace =
#if defined(PARAM_HAS_VOLUMES)
			PathVolumeInfo_ContinueToTrace(volInfo, bsdf
				MATERIALS_PARAM);
#else
		false;
#endif

#if defined(PARAM_HAS_PASSTHROUGH)
		// Check if it is a pass through point
		if (!continueToTrace) {
			const float3 passThroughTrans = BSDF_GetPassThroughTransparency(bsdf
				MATERIALS_PARAM);
			if (!Spectrum_IsBlack(passThroughTrans)) {
				*connectionThroughput *= passThroughTrans;

				// It is a pass through point, continue to trace the ray
				continueToTrace = true;
			}
		}
#endif

		if (continueToTrace) {
#if defined(PARAM_HAS_VOLUMES)
			// Update volume information
			const BSDFEvent eventTypes = BSDF_GetEventTypes(bsdf
						MATERIALS_PARAM);
			PathVolumeInfo_Update(volInfo, eventTypes, bsdf
					MATERIALS_PARAM);
#endif

			// It is a transparent material, continue to trace the ray
			ray->mint = rayHit->t + MachineEpsilon_E(rayHit->t);

			// A safety check
			if (ray->mint >= ray->maxt)
				return false;
			else
				return true;
		} else
			return false;
	} else {
		// Nothing was hit, stop tracing the ray
		return false;
	}
}

//------------------------------------------------------------------------------
// PathDepthInfo
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void PathDepthInfo_Init(__global PathDepthInfo *depthInfo) {
	depthInfo->depth = 0;
	depthInfo->diffuseDepth = 0;
	depthInfo->glossyDepth = 0;
	depthInfo->specularDepth = 0;
}

OPENCL_FORCE_INLINE void PathDepthInfo_IncDepths(__global PathDepthInfo *depthInfo, const BSDFEvent event) {
	++(depthInfo->depth);
	if (event & DIFFUSE)
		++(depthInfo->diffuseDepth);
	if (event & GLOSSY)
		++(depthInfo->glossyDepth);
	if (event & SPECULAR)
		++(depthInfo->specularDepth);
}

OPENCL_FORCE_INLINE bool PathDepthInfo_IsLastPathVertex(__global PathDepthInfo *depthInfo, const BSDFEvent event) {
	return (depthInfo->depth + 1 >= PARAM_MAX_PATH_DEPTH) ||
			((event & DIFFUSE) && (depthInfo->diffuseDepth + 1 >= PARAM_MAX_PATH_DEPTH_DIFFUSE)) ||
			((event & GLOSSY) && (depthInfo->glossyDepth + 1 >= PARAM_MAX_PATH_DEPTH_GLOSSY)) ||
			((event & SPECULAR) && (depthInfo->specularDepth + 1 >= PARAM_MAX_PATH_DEPTH_SPECULAR));
}

OPENCL_FORCE_INLINE bool PathDepthInfo_CheckComponentDepths(const BSDFEvent component) {
	return ((PARAM_MAX_PATH_DEPTH_DIFFUSE > 0) && (component & DIFFUSE)) ||
			((PARAM_MAX_PATH_DEPTH_GLOSSY > 0) && (component & GLOSSY)) ||
			((PARAM_MAX_PATH_DEPTH_SPECULAR > 0) && (component & SPECULAR));
}

OPENCL_FORCE_INLINE uint PathDepthInfo_GetRRDepth(__global PathDepthInfo *depthInfo) {
	return depthInfo->diffuseDepth + depthInfo->glossyDepth;
}

//------------------------------------------------------------------------------
// Init functions
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void InitSampleResult(
		__global Sample *sample,
		__global float *sampleDataPathBase,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		__global float *pixelFilterDistribution,
		Seed *seed) {
	SampleResult_Init(&sample->result);

	float filmX = Sampler_GetSamplePath(seed, sample, sampleDataPathBase, IDX_SCREEN_X);
	float filmY = Sampler_GetSamplePath(seed, sample, sampleDataPathBase, IDX_SCREEN_Y);

#if (PARAM_SAMPLER_TYPE == 1)
	// Metropolis return IDX_SCREEN_X and IDX_SCREEN_Y between [0.0, 1.0] instead
	// that in film pixels like RANDOM and SOBOL samplers
	filmX = filmSubRegion0 + filmX * (filmSubRegion1 - filmSubRegion0 + 1);
	filmY = filmSubRegion2 + filmY * (filmSubRegion3 - filmSubRegion2 + 1);
#endif

	const uint pixelX = min(Floor2UInt(filmX), filmSubRegion1);
	const uint pixelY = min(Floor2UInt(filmY), filmSubRegion3);
	const float uSubPixelX = filmX - pixelX;
	const float uSubPixelY = filmY - pixelY;

	sample->result.pixelX = pixelX;
	sample->result.pixelY = pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(pixelFilterDistribution, uSubPixelX, uSubPixelY, &distX, &distY);

	sample->result.filmX = pixelX + .5f + distX;
	sample->result.filmY = pixelY + .5f + distY;
}

OPENCL_FORCE_NOT_INLINE void GenerateEyePath(
		__global GPUTaskDirectLight *taskDirectLight,
		__global GPUTaskState *taskState,
		__global Sample *sample,
		__global float *sampleDataPathBase,
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		__global float *pixelFilterDistribution,
		__global Ray *ray,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfo,
#endif
		Seed *seed
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		// cameraFilmWidth/cameraFilmHeight and filmWidth/filmHeight are usually
		// the same. They are different when doing tile rendering
		, const uint cameraFilmWidth, const uint cameraFilmHeight,
		const uint tileStartX, const uint tileStartY
#endif
		) {
	InitSampleResult(sample, sampleDataPathBase,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1,
			filmSubRegion2, filmSubRegion3,
			pixelFilterDistribution, seed);

	// Generate the came ray
	const float time = Sampler_GetSamplePath(seed, sample, sampleDataPathBase, IDX_EYE_TIME);

	const float dofSampleX = Sampler_GetSamplePath(seed, sample, sampleDataPathBase, IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(seed, sample, sampleDataPathBase, IDX_DOF_Y);

#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
	Camera_GenerateRay(camera, cameraFilmWidth, cameraFilmHeight,
			ray,
#if defined(PARAM_HAS_VOLUMES)
			volInfo,
#endif
			sample->result.filmX + tileStartX, sample->result.filmY + tileStartY,
			time,
			dofSampleX, dofSampleY);
#else
	Camera_GenerateRay(camera, filmWidth, filmHeight,
			ray,
#if defined(PARAM_HAS_VOLUMES)
			volInfo,
#endif
			sample->result.filmX, sample->result.filmY,
			time,
			dofSampleX, dofSampleY);
#endif

	// Initialize the path state
	taskState->state = MK_RT_NEXT_VERTEX;
	PathDepthInfo_Init(&taskState->depthInfo);
	VSTORE3F(WHITE, taskState->throughput.c);
	taskDirectLight->lastBSDFEvent = SPECULAR; // SPECULAR is required to avoid MIS
	taskDirectLight->lastPdfW = 1.f;

#if defined(PARAM_HAS_PASSTHROUGH)
	// Initialize the pass-through event seed
	const float passThroughEvent = Sampler_GetSamplePath(seed, sample, sampleDataPathBase, IDX_EYE_PASSTHROUGH);
	Seed seedPassThroughEvent;
	Rnd_InitFloat(passThroughEvent, &seedPassThroughEvent);
	taskState->seedPassThroughEvent = seedPassThroughEvent;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sample->result.directShadowMask = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sample->result.indirectShadowMask = 1.f;
#endif

	sample->result.lastPathVertex = (PARAM_MAX_PATH_DEPTH == 1);
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

#if defined(PARAM_HAS_ENVLIGHTS)
OPENCL_FORCE_NOT_INLINE void DirectHitInfiniteLight(
		__global PathDepthInfo *depthInfo,
		const BSDFEvent lastBSDFEvent,
		__global const Spectrum* restrict pathThroughput,
		const __global Ray *ray, const float3 rayNormal,
#if defined(PARAM_HAS_VOLUMES)
		const bool rayFromVolume,
#endif
		const float lastPdfW, __global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	const float3 throughput = VLOAD3F(pathThroughput->c);

	for (uint i = 0; i < envLightCount; ++i) {
		__global const LightSource* restrict light = &lights[envLightIndices[i]];

		// Check if the light source is visible according the settings
		if (!CheckDirectHitVisibilityFlags(light, depthInfo, lastBSDFEvent))
			continue;

		float directPdfW;
		const float3 lightRadiance = EnvLight_GetRadiance(light, -VLOAD3F(&ray->d.x), &directPdfW
				LIGHTS_PARAM);

		if (!Spectrum_IsBlack(lightRadiance)) {
			const float lightPickProb = LightStrategy_SampleLightPdf(lightsDistribution,
					dlscAllEntries, dlscDistributionIndexToLightIndex,
					dlscDistributions, dlscBVHNodes,
					dlscRadius2, dlscNormalCosAngle,
					VLOAD3F(&ray->o.x), rayNormal,
#if defined(PARAM_HAS_VOLUMES)
					rayFromVolume,
#endif
					light->lightSceneIndex);

			// MIS between BSDF sampling and direct light sampling
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));

			SampleResult_AddEmission(sampleResult, light->lightID, throughput, weight * lightRadiance);
		}
	}
}
#endif

OPENCL_FORCE_NOT_INLINE void DirectHitFiniteLight(
		__global PathDepthInfo *depthInfo,
		const BSDFEvent lastBSDFEvent,
		__global const Spectrum* restrict pathThroughput,
		const __global Ray *ray, const float3 rayNormal,
#if defined(PARAM_HAS_VOLUMES)
		const bool rayFromVolume,
#endif
		const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	__global const LightSource* restrict light = &lights[bsdf->triangleLightSourceIndex];

	// Check if the light source is visible according the settings
	if (!CheckDirectHitVisibilityFlags(light, depthInfo, lastBSDFEvent))
		return;
	
	float directPdfA;
	const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf, &directPdfA
			LIGHTS_PARAM);

	if (!Spectrum_IsBlack(emittedRadiance)) {
		// Add emitted radiance
		float weight = 1.f;
		if (!(lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = LightStrategy_SampleLightPdf(lightsDistribution,
					dlscAllEntries, dlscDistributionIndexToLightIndex,
					dlscDistributions, dlscBVHNodes,
					dlscRadius2, dlscNormalCosAngle,
					VLOAD3F(&ray->o.x), rayNormal,
#if defined(PARAM_HAS_VOLUMES)
					rayFromVolume,
#endif
					light->lightSceneIndex);
			const float directPdfW = PdfAtoW(directPdfA, distance,
					fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		}

		SampleResult_AddEmission(sampleResult, BSDF_GetLightID(bsdf
				MATERIALS_PARAM), VLOAD3F(pathThroughput->c), weight * emittedRadiance);
	}
}

OPENCL_FORCE_INLINE float RussianRouletteProb(const float3 color) {
	return clamp(Spectrum_Filter(color), PARAM_RR_CAP, 1.f);
}

OPENCL_FORCE_NOT_INLINE bool DirectLight_Illuminate(
		__global BSDF *bsdf,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		__global HitPoint *tmpHitPoint,
		const float u0, const float u1, const float u2,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float lightPassThroughEvent,
#endif
		__global DirectLightIlluminateInfo *info
		LIGHTS_PARAM_DECL) {
	// Select the light strategy to use
	__global const float* restrict lightDist = BSDF_IsShadowCatcherOnlyInfiniteLights(bsdf MATERIALS_PARAM) ?
		infiniteLightSourcesDistribution : lightsDistribution;

	// Pick a light source to sample
	const float3 point = VLOAD3F(&bsdf->hitPoint.p.x);

	float3 normal = VLOAD3F(&bsdf->hitPoint.shadeN.x);
	const float3 rayDir = -VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const bool intoObject = (dot(rayDir, normal) < 0.f);
	normal = intoObject ? normal : -normal;

	float lightPickPdf;
	const uint lightIndex = LightStrategy_SampleLights(lightDist,
			dlscAllEntries, dlscDistributionIndexToLightIndex,
			dlscDistributions, dlscBVHNodes,
			dlscRadius2, dlscNormalCosAngle,
			point, normal, 
#if defined(PARAM_HAS_VOLUMES)
			bsdf->isVolume,
#endif
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
			point,
			u1, u2,
#if defined(PARAM_HAS_PASSTHROUGH)
			lightPassThroughEvent,
#endif
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			tmpHitPoint,		
			&lightRayDir, &distance, &directPdfW
			LIGHTS_PARAM);
	
	if (Spectrum_IsBlack(lightRadiance))
		return false;
	else {
		VSTORE3F(lightRayDir, &info->dir.x);
		info->distance = distance;
		info->directPdfW = directPdfW;
		VSTORE3F(lightRadiance, info->lightRadiance.c);
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
		VSTORE3F(lightRadiance, info->lightIrradiance.c);
#endif
		return true;
	}
}

OPENCL_FORCE_NOT_INLINE bool DirectLight_BSDFSampling(
		__global DirectLightIlluminateInfo *info,
		const float time,
		const bool lastPathVertex,
		__global PathDepthInfo *depthInfo,
		__global BSDF *bsdf,
		__global Ray *shadowRay
		LIGHTS_PARAM_DECL) {
	const float3 lightRayDir = VLOAD3F(&info->dir.x);
	
	// Sample the BSDF
	BSDFEvent event;
	float bsdfPdfW;
	const float3 bsdfEval = BSDF_Evaluate(bsdf,
			lightRayDir, &event, &bsdfPdfW
			MATERIALS_PARAM);

	if (Spectrum_IsBlack(bsdfEval))
		return false;
	
	// Create a copy of depthInfo for later restore
	PathDepthInfo depthInfoCopy = *depthInfo;

	// Create a new DepthInfo for the path to the light source
	PathDepthInfo_IncDepths(depthInfo, event);

	const float directLightSamplingPdfW = info->directPdfW * info->pickPdf;
	const float factor = 1.f / directLightSamplingPdfW;

	// Russian Roulette
	bsdfPdfW *= (PathDepthInfo_GetRRDepth(depthInfo) >= PARAM_RR_DEPTH) ? RussianRouletteProb(bsdfEval) : 1.f;
	
	// MIS between direct light sampling and BSDF sampling
	//
	// Note: I have to avoiding MIS on the last path vertex
	__global const LightSource* restrict light = &lights[info->lightIndex];

	const bool misEnabled = !lastPathVertex &&
			Light_IsEnvOrIntersectable(light) &&
			CheckDirectHitVisibilityFlags(light, depthInfo, event);

	const float weight = misEnabled ? PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

	const float3 lightRadiance = VLOAD3F(info->lightRadiance.c);
	VSTORE3F(bsdfEval * (weight * factor) * lightRadiance, info->lightRadiance.c);
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	VSTORE3F(factor * lightRadiance, info->lightIrradiance.c);
#endif

	// Setup the shadow ray
	const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
	const float distance = info->distance;
	Ray_Init4(shadowRay, hitPoint, lightRayDir, 0.f, distance, time);

	// Restore the original depthInfo
	*depthInfo = depthInfoCopy;

	return true;
}

//------------------------------------------------------------------------------
// Kernel parameters
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
#define KERNEL_ARGS_VOLUMES \
		, __global PathVolumeInfo *pathVolInfos \
		, __global PathVolumeInfo *directLightVolInfos
#else
#define KERNEL_ARGS_VOLUMES
#endif

#define KERNEL_ARGS_INFINITELIGHTS \
		, const float worldCenterX \
		, const float worldCenterY \
		, const float worldCenterZ \
		, const float worldRadius

#define KERNEL_ARGS_NORMALS_BUFFER \
		, __global const Vector* restrict vertNormals
#define KERNEL_ARGS_UVS_BUFFER \
		, __global const UV* restrict vertUVs
#define KERNEL_ARGS_COLS_BUFFER \
		, __global const Spectrum* restrict vertCols
#define KERNEL_ARGS_ALPHAS_BUFFER \
		, __global const float* restrict vertAlphas

#define KERNEL_ARGS_ENVLIGHTS \
		, __global const uint* restrict envLightIndices \
		, const uint envLightCount

#define KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict envLightDistribution

#if defined(PARAM_IMAGEMAPS_PAGE_0)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0 \
		, __global const ImageMap* restrict imageMapDescs, __global const float* restrict imageMapBuff0
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1 \
		, __global const float* restrict imageMapBuff1
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2 \
		, __global const float* restrict imageMapBuff2
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3 \
		, __global const float* restrict imageMapBuff3
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4 \
		, __global const float* restrict imageMapBuff4
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5 \
		, __global const float* restrict imageMapBuff5
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6 \
		, __global const float* restrict imageMapBuff6
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_7 \
		, __global const float* restrict imageMapBuff7
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_7
#endif
#define KERNEL_ARGS_IMAGEMAPS_PAGES \
		KERNEL_ARGS_IMAGEMAPS_PAGE_0 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_1 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_2 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_3 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_4 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_5 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_6 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_7

#define KERNEL_ARGS_FAST_PIXEL_FILTER \
		, __global float *pixelFilterDistribution

#define KERNEL_ARGS \
		__global GPUTask *tasks \
		, __global GPUTaskDirectLight *tasksDirectLight \
		, __global GPUTaskState *tasksState \
		, __global GPUTaskStats *taskStats \
		KERNEL_ARGS_FAST_PIXEL_FILTER \
		, __global SamplerSharedData *samplerSharedData \
		, __global Sample *samples \
		, __global float *samplesData \
		KERNEL_ARGS_VOLUMES \
		, __global Ray *rays \
		, __global RayHit *rayHits \
		/* Film parameters */ \
		KERNEL_ARGS_FILM \
		/* Scene parameters */ \
		KERNEL_ARGS_INFINITELIGHTS \
		, __global const Material* restrict mats \
		, __global const Texture* restrict texs \
		, __global const SceneObject* restrict sceneObjs \
		, __global const Mesh* restrict meshDescs \
		, __global const Point* restrict vertices \
		KERNEL_ARGS_NORMALS_BUFFER \
		KERNEL_ARGS_UVS_BUFFER \
		KERNEL_ARGS_COLS_BUFFER \
		KERNEL_ARGS_ALPHAS_BUFFER \
		, __global const Triangle* restrict triangles \
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
		, __global const uint* restrict dlscDistributionIndexToLightIndex \
		, __global const float* restrict dlscDistributions \
		, __global const DLSCBVHArrayNode* restrict dlscBVHNodes \
		, const float dlscRadius2 \
		, const float dlscNormalCosAngle \
		/* Images */ \
		KERNEL_ARGS_IMAGEMAPS_PAGES


//------------------------------------------------------------------------------
// To initialize image maps page pointer table
//------------------------------------------------------------------------------

#if defined(PARAM_IMAGEMAPS_PAGE_0)
#define INIT_IMAGEMAPS_PAGE_0 imageMapBuff[0] = imageMapBuff0;
#else
#define INIT_IMAGEMAPS_PAGE_0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
#define INIT_IMAGEMAPS_PAGE_1 imageMapBuff[1] = imageMapBuff1;
#else
#define INIT_IMAGEMAPS_PAGE_1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
#define INIT_IMAGEMAPS_PAGE_2 imageMapBuff[2] = imageMapBuff2;
#else
#define INIT_IMAGEMAPS_PAGE_2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
#define INIT_IMAGEMAPS_PAGE_3 imageMapBuff[3] = imageMapBuff3;
#else
#define INIT_IMAGEMAPS_PAGE_3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define INIT_IMAGEMAPS_PAGE_4 imageMapBuff[4] = imageMapBuff4;
#else
#define INIT_IMAGEMAPS_PAGE_4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
#define INIT_IMAGEMAPS_PAGE_5 imageMapBuff[5] = imageMapBuff5;
#else
#define INIT_IMAGEMAPS_PAGE_5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
#define INIT_IMAGEMAPS_PAGE_6 imageMapBuff[6] = imageMapBuff6;
#else
#define INIT_IMAGEMAPS_PAGE_6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
#define INIT_IMAGEMAPS_PAGE_7 imageMapBuff[7] = imageMapBuff7;
#else
#define INIT_IMAGEMAPS_PAGE_7
#endif

#if defined(PARAM_HAS_IMAGEMAPS)
#define INIT_IMAGEMAPS_PAGES \
	__global const float* restrict imageMapBuff[PARAM_IMAGEMAPS_COUNT]; \
	INIT_IMAGEMAPS_PAGE_0 \
	INIT_IMAGEMAPS_PAGE_1 \
	INIT_IMAGEMAPS_PAGE_2 \
	INIT_IMAGEMAPS_PAGE_3 \
	INIT_IMAGEMAPS_PAGE_4 \
	INIT_IMAGEMAPS_PAGE_5 \
	INIT_IMAGEMAPS_PAGE_6 \
	INIT_IMAGEMAPS_PAGE_7
#else
#define INIT_IMAGEMAPS_PAGES
#endif

//------------------------------------------------------------------------------
// Init Kernels
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitSeed(__global GPUTask *tasks,
		const uint seedBase) {
	const size_t gid = get_global_id(0);

	// Initialize random number generator

	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Save the seed
	__global GPUTask *task = &tasks[gid];
	task->seed = seed;
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Init(
		__global GPUTask *tasks,
		__global GPUTaskDirectLight *tasksDirectLight,
		__global GPUTaskState *tasksState,
		__global GPUTaskStats *taskStats,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *samples,
		__global float *samplesData,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *pathVolInfos,
#endif
		__global float *pixelFilterDistribution,
		__global Ray *rays,
		__global Camera *camera
		KERNEL_ARGS_FILM
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		// cameraFilmWidth/cameraFilmHeight and filmWidth/filmHeight are usually
		// the same. They are different when doing tile rendering
		, const uint cameraFilmWidth, const uint cameraFilmHeight
		, const uint tileStartX, const uint tileStartY
		, const uint tileWidth, const uint tileHeight
		, const uint tilePass, const uint aaSamples
#endif
		) {
	const size_t gid = get_global_id(0);

	__global GPUTaskState *taskState = &tasksState[gid];

#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
	if (gid >= filmWidth * filmHeight * aaSamples * aaSamples) {
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
	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	const bool validSample = Sampler_Init(seed, samplerSharedData, sample, sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
			filmConvergence,
#endif
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
			, cameraFilmWidth, cameraFilmHeight
			, tileStartX, tileStartY
			, tileWidth, tileHeight
			, tilePass, aaSamples
#endif
			);

	if (validSample) {
		__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);

#if defined(PARAM_HAS_VOLUMES)
		PathVolumeInfo_Init(&pathVolInfos[gid]);
#endif

		// Generate the eye path
		GenerateEyePath(taskDirectLight, taskState, sample, sampleDataPathBase, camera,
				filmWidth, filmHeight,
				filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3,
				pixelFilterDistribution,
				&rays[gid],
#if defined(PARAM_HAS_VOLUMES)
				&pathVolInfos[gid],
#endif
				seed
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
				, cameraFilmWidth, cameraFilmHeight,
				tileStartX, tileStartY
#endif
				);
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
