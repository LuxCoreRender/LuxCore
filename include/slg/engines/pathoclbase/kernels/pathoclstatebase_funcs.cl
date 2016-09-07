#line 2 "pathocl_funcs.cl"

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

// List of symbols defined at compile time:
//  PARAM_MAX_PATH_DEPTH
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
//  PARAM_USE_FAST_PIXEL_FILTER
//  PARAM_SAMPLER_TYPE (0 = Inlined Random, 1 = Metropolis, 2 = Sobol)
// (Metropolis)
//  PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE
//  PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT
//  PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE
// (Sobol)
//  PARAM_SAMPLER_SOBOL_STARTOFFSET
//  PARAM_SAMPLER_SOBOL_MAXDEPTH
//  PARAM_SAMPLER_SOBOL_RNG0
//  PARAM_SAMPLER_SOBOL_RNG1


//------------------------------------------------------------------------------
// Init Kernel
//------------------------------------------------------------------------------

void InitSampleResult(
		__global Sample *sample,
		__global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, __global float *pixelFilterDistribution
#endif
		, Seed *seed) {
	SampleResult_Init(&sample->result);

#if (PARAM_SAMPLER_TYPE == 1)
	// Used by Sampler_GetSamplePath() macro
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
#endif

	const float u0 = Sampler_GetSamplePath(IDX_SCREEN_X);
	const float u1 = Sampler_GetSamplePath(IDX_SCREEN_Y);
	float ux, uy;
	Film_GetSampleXY(u0, u1, &ux, &uy,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1,
			filmSubRegion2, filmSubRegion3);

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	const uint pixelX = Floor2UInt(ux);
	const uint pixelY = Floor2UInt(uy);
	sample->result.pixelX = pixelX;
	sample->result.pixelY = pixelY;

	float uSubPixelX = ux - pixelX;
	float uSubPixelY = uy - pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(pixelFilterDistribution, uSubPixelX, uSubPixelY, &distX, &distY);

	sample->result.filmX = pixelX + .5f + distX;
	sample->result.filmY = pixelY + .5f + distY;
#else
	sample->result.filmX = ux;
	sample->result.filmY = uy;
#endif
}

void GenerateEyePath(
		__global GPUTaskDirectLight *taskDirectLight,
		__global GPUTaskState *taskState,
		__global Sample *sample,
		__global float *sampleData,
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		__global float *pixelFilterDistribution,
#endif
		__global Ray *ray,
		Seed *seed) {
#if (PARAM_SAMPLER_TYPE == 0)
	const float time = Rnd_FloatValue(seed);

	const float dofSampleX = Rnd_FloatValue(seed);
	const float dofSampleY = Rnd_FloatValue(seed);

#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassThrough = Rnd_FloatValue(seed);
#endif
#endif

#if (PARAM_SAMPLER_TYPE == 1)
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	
	const float time = Sampler_GetSamplePath(IDX_EYE_TIME);

	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);

#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassThrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

#if (PARAM_SAMPLER_TYPE == 2)
	const float time = Sampler_GetSamplePath(IDX_EYE_TIME);

	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);

#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassThrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

	InitSampleResult(sample, sampleData,
		filmWidth, filmHeight,
		filmSubRegion0, filmSubRegion1,
		filmSubRegion2, filmSubRegion3
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, pixelFilterDistribution
#endif
		, seed);

	Camera_GenerateRay(camera, filmWidth, filmHeight,
			ray, sample->result.filmX, sample->result.filmY, time,
			dofSampleX, dofSampleY);

	// Initialize the path state
	taskState->state = RT_NEXT_VERTEX; // Or MK_RT_NEXT_VERTEX (they have the same value)
	taskState->pathVertexCount = 1;
	VSTORE3F(WHITE, taskState->throughput.c);
	taskDirectLight->lastBSDFEvent = SPECULAR; // SPECULAR is required to avoid MIS
	taskDirectLight->lastPdfW = 1.f;
#if defined(PARAM_HAS_PASSTHROUGH)
	// This is a bit tricky. I store the passThroughEvent in the BSDF
	// before of the initialization because it can be used during the
	// tracing of next path vertex ray.

	taskState->bsdf.hitPoint.passThroughEvent = eyePassThrough;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sample->result.directShadowMask = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sample->result.indirectShadowMask = 1.f;
#endif

	sample->result.lastPathVertex = (PARAM_MAX_PATH_DEPTH == 1);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Init(
		uint seedBase,
		__global GPUTask *tasks,
		__global GPUTaskDirectLight *tasksDirectLight,
		__global GPUTaskState *tasksState,
		__global GPUTaskStats *taskStats,
		__global Sample *samples,
		__global float *samplesData,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *pathVolInfos,
#endif
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		__global float *pixelFilterDistribution,
#endif
		__global Ray *rays,
		__global Camera *camera,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		) {
	const size_t gid = get_global_id(0);

	// Initialize the task
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global GPUTaskState *taskState = &tasksState[gid];

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Initialize the sample and path
	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	Sampler_Init(&seed, sample, sampleData);
	GenerateEyePath(taskDirectLight, taskState, sample, sampleData, camera,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3,
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
			pixelFilterDistribution,
#endif
			&rays[gid], &seed);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->sampleCount = 0;

#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo_Init(&pathVolInfos[gid]);
#endif
}

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_ENVLIGHTS)
void DirectHitInfiniteLight(
		const BSDFEvent lastBSDFEvent,
		__global const Spectrum* restrict pathThroughput,
		const float3 eyeDir, const float lastPdfW,
		__global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	const float3 throughput = VLOAD3F(pathThroughput->c);

	for (uint i = 0; i < envLightCount; ++i) {
		__global const LightSource* restrict light = &lights[envLightIndices[i]];

		float directPdfW;
		const float3 lightRadiance = EnvLight_GetRadiance(light, -eyeDir, &directPdfW
				LIGHTS_PARAM);

		if (!Spectrum_IsBlack(lightRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW));

			SampleResult_AddEmission(sampleResult, light->lightID, throughput, weight * lightRadiance);
		}
	}
}
#endif

void DirectHitFiniteLight(
		const BSDFEvent lastBSDFEvent,
		__global const Spectrum* restrict pathThroughput, const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	float directPdfA;
	const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf, &directPdfA
			LIGHTS_PARAM);

	if (!Spectrum_IsBlack(emittedRadiance)) {
		// Add emitted radiance
		float weight = 1.f;
		if (!(lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution,
					lights[bsdf->triangleLightSourceIndex].lightSceneIndex);
			const float directPdfW = PdfAtoW(directPdfA, distance,
				fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		}

		SampleResult_AddEmission(sampleResult, BSDF_GetLightID(bsdf
				MATERIALS_PARAM), VLOAD3F(pathThroughput->c), weight * emittedRadiance);
	}
}

float RussianRouletteProb(const float3 color) {
	return clamp(Spectrum_Filter(color), PARAM_RR_CAP, 1.f);
}

bool DirectLight_Illuminate(
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
		const float3 point,
		__global DirectLightIlluminateInfo *info
		LIGHTS_PARAM_DECL) {
	// Select the light strategy to use
	__global const float* restrict lightDist = BSDF_IsShadowCatcherOnlyInfiniteLights(bsdf MATERIALS_PARAM) ?
		infiniteLightSourcesDistribution : lightsDistribution;

	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightDist, u0, &lightPickPdf);
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

bool DirectLight_BSDFSampling(
		__global DirectLightIlluminateInfo *info,
		const float time,
		const bool lastPathVertex, const uint pathVertexCount,
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

	const float cosThetaToLight = fabs(dot(lightRayDir, VLOAD3F(&bsdf->hitPoint.shadeN.x)));
	const float directLightSamplingPdfW = info->directPdfW * info->pickPdf;
	const float factor = 1.f / directLightSamplingPdfW;

	// Russian Roulette
	// The +1 is there to account the current path vertex used for DL
	bsdfPdfW *= (pathVertexCount + 1 >= PARAM_RR_DEPTH) ? RussianRouletteProb(bsdfEval) : 1.f;

	// MIS between direct light sampling and BSDF sampling
	//
	// Note: I have to avoiding MIS on the last path vertex
	__global const LightSource* restrict light = &lights[info->lightIndex];
	const float weight = (!lastPathVertex && Light_IsEnvOrIntersectable(light)) ?
		PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

	const float3 lightRadiance = VLOAD3F(info->lightRadiance.c);
	VSTORE3F(bsdfEval * (weight * factor) * lightRadiance, info->lightRadiance.c);
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	VSTORE3F(factor * lightRadiance, info->lightIrradiance.c);
#endif

	// Setup the shadow ray
	const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
	const float distance = info->distance;
	Ray_Init4(shadowRay, hitPoint, lightRayDir, 0.f, distance, time);

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

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_0 \
		, __global float *filmRadianceGroup0
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_1 \
		, __global float *filmRadianceGroup1
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_2 \
		, __global float *filmRadianceGroup2
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_3 \
		, __global float *filmRadianceGroup3
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_4 \
		, __global float *filmRadianceGroup4
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_5 \
		, __global float *filmRadianceGroup5
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_6 \
		, __global float *filmRadianceGroup6
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_7 \
		, __global float *filmRadianceGroup7
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
#define KERNEL_ARGS_FILM_CHANNELS_ALPHA \
		, __global float *filmAlpha
#else
#define KERNEL_ARGS_FILM_CHANNELS_ALPHA
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
#define KERNEL_ARGS_FILM_CHANNELS_DEPTH \
		, __global float *filmDepth
#else
#define KERNEL_ARGS_FILM_CHANNELS_DEPTH
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
#define KERNEL_ARGS_FILM_CHANNELS_POSITION \
		, __global float *filmPosition
#else
#define KERNEL_ARGS_FILM_CHANNELS_POSITION
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
#define KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL \
		, __global float *filmGeometryNormal
#else
#define KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
#define KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL \
		, __global float *filmShadingNormal
#else
#define KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID \
		, __global uint *filmMaterialID
#else
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE \
		, __global float *filmDirectDiffuse
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY \
		, __global float *filmDirectGlossy
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
#define KERNEL_ARGS_FILM_CHANNELS_EMISSION \
		, __global float *filmEmission
#else
#define KERNEL_ARGS_FILM_CHANNELS_EMISSION
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE \
		, __global float *filmIndirectDiffuse
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY \
		, __global float *filmIndirectGlossy
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR \
		, __global float *filmIndirectSpecular
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK \
		, __global float *filmMaterialIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		, __global float *filmDirectShadowMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		, __global float *filmIndirectShadowMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
#define KERNEL_ARGS_FILM_CHANNELS_UV \
		, __global float *filmUV
#else
#define KERNEL_ARGS_FILM_CHANNELS_UV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
#define KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		, __global float *filmRayCount
#else
#define KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
#define KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID \
		, __global float *filmByMaterialID
#else
#define KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
#define KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE \
		, __global float *filmIrradiance
#else
#define KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID \
		, __global uint *filmObjectID
#else
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK \
		, __global float *filmObjectIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
#define KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID \
		, __global float *filmByObjectID
#else
#define KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID
#endif

#define KERNEL_ARGS_FILM \
		, const uint filmWidth, const uint filmHeight \
		, const uint filmSubRegion0, const uint filmSubRegion1 \
		, const uint filmSubRegion2, const uint filmSubRegion3 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_0 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_1 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_2 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_3 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_4 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_5 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_6 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_7 \
		KERNEL_ARGS_FILM_CHANNELS_ALPHA \
		KERNEL_ARGS_FILM_CHANNELS_DEPTH \
		KERNEL_ARGS_FILM_CHANNELS_POSITION \
		KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL \
		KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL \
		KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY \
		KERNEL_ARGS_FILM_CHANNELS_EMISSION \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR \
		KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_UV \
		KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID \
		KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE \
		KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID \
		KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID

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
		, __global const float* restrict infiniteLightDistribution

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

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
#define KERNEL_ARGS_FAST_PIXEL_FILTER \
		, __global float *pixelFilterDistribution
#else
#define KERNEL_ARGS_FAST_PIXEL_FILTER
#endif

#define KERNEL_ARGS \
		__global GPUTask *tasks \
		, __global GPUTaskDirectLight *tasksDirectLight \
		, __global GPUTaskState *tasksState \
		, __global GPUTaskStats *taskStats \
		KERNEL_ARGS_FAST_PIXEL_FILTER \
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
		, __global const uint* restrict meshTriLightDefsOffset \
		KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict lightsDistribution \
		, __global const float* restrict infiniteLightSourcesDistribution \
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
