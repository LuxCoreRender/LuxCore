#line 2 "patchocl_kernels.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

// (optional)
//  PARAM_SAMPLER_TYPE (0 = Inlined Random, 1 = Metropolis, 2 = Sobol)
// (Metropolis)
//  PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE
//  PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT
//  PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE
// (Sobol)
//  PARAM_SAMPLER_SOBOL_STARTOFFSET
//  PARAM_SAMPLER_SOBOL_MAXDEPTH


//------------------------------------------------------------------------------
// Init Kernel
//------------------------------------------------------------------------------

void GenerateCameraPath(
		__global GPUTask *task,
		__global Sample *sample,
		__global float *sampleData,
		__global Camera *camera,
		const uint filmWidth,
		const uint filmHeight,
		__global Ray *ray,
		Seed *seed) {
#if (PARAM_SAMPLER_TYPE == 0)
	const float time = Rnd_FloatValue(seed);

#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Rnd_FloatValue(seed);
	const float dofSampleY = Rnd_FloatValue(seed);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Rnd_FloatValue(seed);
#endif
#endif

#if (PARAM_SAMPLER_TYPE == 1)
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	
	const float time = Sampler_GetSamplePath(IDX_EYE_TIME);

#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

#if (PARAM_SAMPLER_TYPE == 2)
	const float time = Sampler_GetSamplePath(IDX_EYE_TIME);

#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

	Camera_GenerateRay(camera, filmWidth, filmHeight, ray,
			sample->result.filmX, sample->result.filmY, time
#if defined(PARAM_CAMERA_HAS_DOF)
			, dofSampleX, dofSampleY
#endif
			);

	// Initialize the path state
	task->pathStateBase.state = RT_NEXT_VERTEX; // Or MK_RT_NEXT_VERTEX (they have the same value)
	task->pathStateBase.depth = 1;
	VSTORE3F(WHITE, task->pathStateBase.throughput.c);
	task->directLightState.lastBSDFEvent = SPECULAR; // SPECULAR is required to avoid MIS
	task->directLightState.lastPdfW = 1.f;
#if defined(PARAM_HAS_PASSTHROUGH)
	// This is a bit tricky. I store the passThroughEvent in the BSDF
	// before of the initialization because it can be used during the
	// tracing of next path vertex ray.

	task->pathStateBase.bsdf.hitPoint.passThroughEvent = eyePassthrough;
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
		__global GPUTaskStats *taskStats,
		__global Sample *samples,
		__global float *samplesData,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *pathVolInfos,
#endif
		__global Ray *rays,
		__global Camera *camera,
		const uint filmWidth,
		const uint filmHeight
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Initialize the sample and path
	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	Sampler_Init(&seed, sample, sampleData, filmWidth, filmHeight);
	GenerateCameraPath(task, sample, sampleData, camera, filmWidth, filmHeight, &rays[gid], &seed);

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
		__global const Spectrum *pathThroughput,
		const float3 eyeDir, const float lastPdfW,
		__global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	const float3 throughput = VLOAD3F(pathThroughput->c);

	for (uint i = 0; i < envLightCount; ++i) {
		__global LightSource *light = &lights[envLightIndices[i]];

		float directPdfW;
		const float3 lightRadiance = EnvLight_GetRadiance(light, -eyeDir, &directPdfW
				LIGHTS_PARAM);

		if (!Spectrum_IsBlack(lightRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW));
			const float3 radiance = weight * throughput * lightRadiance;

			SampleResult_AddEmission(sampleResult, light->lightID, radiance);
		}
	}
}
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
void DirectHitFiniteLight(
		const BSDFEvent lastBSDFEvent,
		__global const Spectrum *pathThroughput, const float distance, __global BSDF *bsdf,
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
		const float3 lightRadiance = weight * VLOAD3F(pathThroughput->c) * emittedRadiance;

		SampleResult_AddEmission(sampleResult, BSDF_GetLightID(bsdf
				MATERIALS_PARAM), lightRadiance);
	}
}
#endif

float RussianRouletteProb(const float3 color) {
	return clamp(Spectrum_Filter(color), PARAM_RR_CAP, 1.f);
}

bool DirectLightSampling(
		__global LightSource *light,
		const float lightPickPdf,
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
		const float time, const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float lightPassThroughEvent,
#endif
		const bool lastPathVertex, const uint depth,
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		LIGHTS_PARAM_DECL) {
	float3 lightRayDir;
	float distance, directPdfW;
	const float3 lightRadiance = Light_Illuminate(
			light,
			VLOAD3F(&bsdf->hitPoint.p.x),
			u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
			lightPassThroughEvent,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			tmpHitPoint,
#endif		
			&lightRayDir, &distance, &directPdfW
			LIGHTS_PARAM);

	// Setup the shadow ray
	if (!Spectrum_IsBlack(lightRadiance)) {
		BSDFEvent event;
		float bsdfPdfW;
		const float3 bsdfEval = BSDF_Evaluate(bsdf,
				lightRayDir, &event, &bsdfPdfW
				MATERIALS_PARAM);

		if (!Spectrum_IsBlack(bsdfEval)) {
			const float cosThetaToLight = fabs(dot(lightRayDir, VLOAD3F(&bsdf->hitPoint.shadeN.x)));
			const float directLightSamplingPdfW = directPdfW * lightPickPdf;
			const float factor = 1.f / directLightSamplingPdfW;

			// Russian Roulette
			bsdfPdfW *= (depth >= PARAM_RR_DEPTH) ? RussianRouletteProb(bsdfEval) : 1.f;

			// MIS between direct light sampling and BSDF sampling
			//
			// Note: I have to avoiding MIS on the last path vertex
			const float weight = (!lastPathVertex && Light_IsEnvOrIntersectable(light)) ?
				PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

			VSTORE3F((weight * factor) * VLOAD3F(pathThroughput->c) * bsdfEval * lightRadiance, radiance->c);
			*ID = light->lightID;

			// Setup the shadow ray
			const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
			const float epsilon = fmax(MachineEpsilon_E_Float3(hitPoint), MachineEpsilon_E(distance));

			Ray_Init4(shadowRay, hitPoint, lightRayDir,
				epsilon,
				distance - epsilon, time);

			return true;
		}
	}

	return false;
}

bool DirectLightSampling_ONE(
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
		const float time, const float u0, const float u1, const float u2,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float lightPassThroughEvent,
#endif
		const bool lastPathVertex, const uint depth,
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		LIGHTS_PARAM_DECL) {
	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightsDistribution, u0, &lightPickPdf);

	return DirectLightSampling(
		&lights[lightIndex],
		lightPickPdf,
#if defined(PARAM_HAS_INFINITELIGHTS)
		worldCenterX,
		worldCenterY,
		worldCenterZ,
		worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		tmpHitPoint,
#endif
		time, u1, u2,
#if defined(PARAM_HAS_PASSTHROUGH)
		lightPassThroughEvent,
#endif
		lastPathVertex, depth,
		pathThroughput, bsdf,
		shadowRay, radiance, ID
		LIGHTS_PARAM);
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
#define KERNEL_ARGS_FILM_CHANNELS_ID_MASK \
		, __global float *filmMaterialIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_ID_MASK
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

#define KERNEL_ARGS_FILM \
		, const uint filmWidth, const uint filmHeight \
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
		KERNEL_ARGS_FILM_CHANNELS_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_UV \
		KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID

#if defined(PARAM_HAS_INFINITELIGHTS)
#define KERNEL_ARGS_INFINITELIGHTS \
		, const float worldCenterX \
		, const float worldCenterY \
		, const float worldCenterZ \
		, const float worldRadius
#else
#define KERNEL_ARGS_INFINITELIGHTS
#endif

#if defined(PARAM_HAS_NORMALS_BUFFER)
#define KERNEL_ARGS_NORMALS_BUFFER \
		, __global Vector *vertNormals
#else
#define KERNEL_ARGS_NORMALS_BUFFER
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
#define KERNEL_ARGS_UVS_BUFFER \
		, __global UV *vertUVs
#else
#define KERNEL_ARGS_UVS_BUFFER
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
#define KERNEL_ARGS_COLS_BUFFER \
		, __global Spectrum *vertCols
#else
#define KERNEL_ARGS_COLS_BUFFER
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
#define KERNEL_ARGS_ALPHAS_BUFFER \
		__global float *vertAlphas,
#else
#define KERNEL_ARGS_ALPHAS_BUFFER
#endif

#if defined(PARAM_HAS_ENVLIGHTS)
#define KERNEL_ARGS_ENVLIGHTS \
		, __global uint *envLightIndices \
		, const uint envLightCount
#else
#define KERNEL_ARGS_ENVLIGHTS
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
#define KERNEL_ARGS_INFINITELIGHT \
		, __global float *infiniteLightDistribution
#else
#define KERNEL_ARGS_INFINITELIGHT
#endif

#if defined(PARAM_IMAGEMAPS_PAGE_0)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0 \
		, __global ImageMap *imageMapDescs, __global float *imageMapBuff0
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1 \
		, __global float *imageMapBuff1
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2 \
		, __global float *imageMapBuff2
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3 \
		, __global float *imageMapBuff3
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4 \
		, __global float *imageMapBuff4
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5 \
		, __global float *imageMapBuff5
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6 \
		, __global float *imageMapBuff6
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_7 \
		, __global float *imageMapBuff7
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

#define KERNEL_ARGS \
		__global GPUTask *tasks \
		, __global GPUTaskStats *taskStats \
		, __global Sample *samples \
		, __global float *samplesData \
		KERNEL_ARGS_VOLUMES \
		, __global Ray *rays \
		, __global RayHit *rayHits \
		/* Film parameters */ \
		KERNEL_ARGS_FILM \
		/* Scene parameters */ \
		KERNEL_ARGS_INFINITELIGHTS \
		, __global Material *mats \
		, __global Texture *texs \
		, __global uint *meshMats \
		, __global Mesh *meshDescs \
		, __global Point *vertices \
		KERNEL_ARGS_NORMALS_BUFFER \
		KERNEL_ARGS_UVS_BUFFER \
		KERNEL_ARGS_COLS_BUFFER \
		KERNEL_ARGS_ALPHAS_BUFFER \
		, __global Triangle *triangles \
		, __global Camera *camera \
		/* Lights */ \
		, __global LightSource *lights \
		KERNEL_ARGS_ENVLIGHTS \
		, __global uint *meshTriLightDefsOffset \
		KERNEL_ARGS_INFINITELIGHT \
		, __global float *lightsDistribution \
		/* Images */ \
		KERNEL_ARGS_IMAGEMAPS_PAGES

//------------------------------------------------------------------------------
// AdvancePaths (Mega-Kernel)
//------------------------------------------------------------------------------
 
__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	//--------------------------------------------------------------------------
	// Advance the finite state machine step
	//--------------------------------------------------------------------------

	__global GPUTask *task = &tasks[gid];

	// Read the path state
	PathState pathState = task->pathStateBase.state;
	uint depth = task->pathStateBase.depth;

	__global BSDF *bsdf = &task->pathStateBase.bsdf;

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
#if (PARAM_SAMPLER_TYPE != 0)
	// Used by Sampler_GetSamplePathVertex() macro
	__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, depth);
#endif

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	sample->result.rayCount += 1;
#endif

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_NEXT_VERTEX
	// To: SPLAT_SAMPLE or GENERATE_DL_RAY
	//--------------------------------------------------------------------------

	if (pathState == RT_NEXT_VERTEX) {
		const bool continueToTrace = Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
			&pathVolInfos[gid],
			&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
			task->pathStateBase.bsdf.hitPoint.passThroughEvent,
#endif
			ray, rayHit, bsdf,
			&task->pathStateBase.throughput,
			&sample->result,
			// BSDF_Init parameters
			meshDescs,
			meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			meshTriLightDefsOffset,
#endif
			vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
			vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
			vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
			vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
			vertAlphas,
#endif
			triangles
			MATERIALS_PARAM
			);
		const bool rayMiss = (rayHit->meshIndex == NULL_INDEX);

		// If continueToTrace, there is nothing to do, just keep the same state
		if (!continueToTrace) {
			if (!rayMiss) {
				//--------------------------------------------------------------
				// Something was hit
				//--------------------------------------------------------------

				if (depth == 1) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
					sample->result.alpha = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
					sample->result.depth = rayHit->t;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
					sample->result.position = bsdf->hitPoint.p;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
					sample->result.geometryNormal = bsdf->hitPoint.geometryN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
					sample->result.shadingNormal = bsdf->hitPoint.shadeN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
					sample->result.materialID = BSDF_GetMaterialID(bsdf
							MATERIALS_PARAM);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
					sample->result.uv = bsdf->hitPoint.uv;
#endif
				}

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
				// Check if it is a light source (note: I can hit only triangle area light sources)
				if (BSDF_IsLightSource(bsdf)) {
					DirectHitFiniteLight(
							task->directLightState.lastBSDFEvent,
							&task->pathStateBase.throughput,
							rayHit->t, bsdf, task->directLightState.lastPdfW,
							&sample->result
							LIGHTS_PARAM);
				}
#endif

				// Check if this is the last path vertex (but not also the first)
				//
				// I handle as a special case when the path vertex is both the first
				// and the last: I do direct light sampling without MIS.
				if (sample->result.lastPathVertex && !sample->result.firstPathVertex)
					pathState = SPLAT_SAMPLE;
				else {
					// Direct light sampling
					pathState = GENERATE_DL_RAY;
				}
			} else {
				//--------------------------------------------------------------
				// Nothing was hit, add environmental lights radiance
				//--------------------------------------------------------------

#if defined(PARAM_HAS_ENVLIGHTS)
				DirectHitInfiniteLight(
						task->directLightState.lastBSDFEvent,
						&task->pathStateBase.throughput,
						VLOAD3F(&ray->d.x), task->directLightState.lastPdfW,
						&sample->result
						LIGHTS_PARAM);
#endif

				if (depth == 1) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
					sample->result.alpha = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
					sample->result.depth = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
					sample->result.position.x = INFINITY;
					sample->result.position.y = INFINITY;
					sample->result.position.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
					sample->result.geometryNormal.x = INFINITY;
					sample->result.geometryNormal.y = INFINITY;
					sample->result.geometryNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
					sample->result.shadingNormal.x = INFINITY;
					sample->result.shadingNormal.y = INFINITY;
					sample->result.shadingNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
					sample->result.materialID = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
					sample->result.uv.u = INFINITY;
					sample->result.uv.v = INFINITY;
#endif
				}

				pathState = SPLAT_SAMPLE;
			}
		}
#if defined(PARAM_HAS_PASSTHROUGH)
		else {
			// I generate a new random variable starting from the previous one. I'm
			// not really sure about the kind of correlation introduced by this
			// trick.
			task->pathStateBase.bsdf.hitPoint.passThroughEvent = fabs(task->pathStateBase.bsdf.hitPoint.passThroughEvent - .5f) * 2.f;
		}
#endif
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_DL
	// To: GENERATE_NEXT_VERTEX_RAY
	//--------------------------------------------------------------------------

	if (pathState == RT_DL) {
		const bool continueToTrace = 
#if defined(PARAM_HAS_PASSTHROUGH) || defined(PARAM_HAS_VOLUMES)
			Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
				&directLightVolInfos[gid],
				&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
				task->directLightRayPassThroughEvent,
#endif
				ray, rayHit, &task->tmpBsdf,
				&task->directLightState.lightRadiance,
				NULL,
				// BSDF_Init parameters
				meshDescs,
				meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
				meshTriLightDefsOffset,
#endif
				vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
				vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
				vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
				vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
				vertAlphas,
#endif
				triangles
				MATERIALS_PARAM
				);
#else
		false;
#endif
		const bool rayMiss = (rayHit->meshIndex == NULL_INDEX);

		// If continueToTrace, there is nothing to do, just keep the same state
		if (!continueToTrace) {
			if (rayMiss) {
				// Nothing was hit, the light source is visible

				SampleResult_AddDirectLight(&sample->result, task->directLightState.lightID,
						BSDF_GetEventTypes(bsdf
							MATERIALS_PARAM),
						VLOAD3F(task->directLightState.lightRadiance.c),
						1.f);
			}

			// Check if this is the last path vertex
			if (sample->result.lastPathVertex)
				pathState = SPLAT_SAMPLE;
			else
				pathState = GENERATE_NEXT_VERTEX_RAY;
		}
#if defined(PARAM_HAS_PASSTHROUGH)
		else {
			// I generate a new random variable starting from the previous one. I'm
			// not really sure about the kind of correlation introduced by this
			// trick.
			task->directLightRayPassThroughEvent = fabs(task->directLightRayPassThroughEvent - .5f) * 2.f;
		}
#endif
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: GENERATE_DL_RAY
	// To: GENERATE_NEXT_VERTEX_RAY or RT_DL or SPLAT_SAMPLE
	//--------------------------------------------------------------------------

	if (pathState == GENERATE_DL_RAY) {
		if (!BSDF_IsDelta(bsdf
				MATERIALS_PARAM) &&
			DirectLightSampling_ONE(
#if defined(PARAM_HAS_INFINITELIGHTS)
				worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
				&task->tmpHitPoint,
#endif
				ray->time,
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_X),
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
#if defined(PARAM_HAS_PASSTHROUGH)
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_W),
#endif
				sample->result.lastPathVertex, depth, &task->pathStateBase.throughput, bsdf,
				ray, &task->directLightState.lightRadiance, &task->directLightState.lightID
				LIGHTS_PARAM)) {
#if defined(PARAM_HAS_PASSTHROUGH)
			// Initialize the pass-through event for the shadow ray
			task->directLightRayPassThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_A);
#endif
#if defined(PARAM_HAS_VOLUMES)
			// Make a copy of current PathVolumeInfo for tracing the
			// shadow ray
			directLightVolInfos[gid] = pathVolInfos[gid];
#endif
			// I have to trace the shadow ray
			pathState = RT_DL;
		} else {
			// No shadow ray to trace, move to the next vertex ray
			// however, I have to Check if this is the last path vertex
			if (sample->result.lastPathVertex)
				pathState = SPLAT_SAMPLE;
			else
				pathState = GENERATE_NEXT_VERTEX_RAY;
		}
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: GENERATE_NEXT_VERTEX_RAY
	// To: SPLAT_SAMPLE or RT_NEXT_VERTEX
	//--------------------------------------------------------------------------

	if (pathState == GENERATE_NEXT_VERTEX_RAY) {
		// Sample the BSDF
		__global BSDF *bsdf = &task->pathStateBase.bsdf;
		float3 sampledDir;
		float lastPdfW;
		float cosSampledDir;
		BSDFEvent event;

		const float3 bsdfSample = BSDF_Sample(bsdf,
				Sampler_GetSamplePathVertex(depth, IDX_BSDF_X),
				Sampler_GetSamplePathVertex(depth, IDX_BSDF_Y),
				&sampledDir, &lastPdfW, &cosSampledDir, &event, ALL
				MATERIALS_PARAM);

		// Russian Roulette
		const float rrProb = RussianRouletteProb(bsdfSample);
		const bool rrEnabled = (depth >= PARAM_RR_DEPTH) && !(event & SPECULAR);
		const bool rrContinuePath = !rrEnabled || (Sampler_GetSamplePathVertex(depth, IDX_RR) < rrProb);

		// Max. path depth
		const bool maxPathDepth = (depth >= PARAM_MAX_PATH_DEPTH);

		const bool continuePath = !Spectrum_IsBlack(bsdfSample) && rrContinuePath && !maxPathDepth;
		if (continuePath) {
			float3 throughput = VLOAD3F(task->pathStateBase.throughput.c);
			throughput *= bsdfSample;
			if (rrEnabled)
				throughput /= rrProb; // Russian Roulette

			VSTORE3F(throughput, task->pathStateBase.throughput.c);

#if defined(PARAM_HAS_VOLUMES)
			// Update volume information
			PathVolumeInfo_Update(&pathVolInfos[gid], event, bsdf
					MATERIALS_PARAM);
#endif

			Ray_Init2(ray, VLOAD3F(&bsdf->hitPoint.p.x), sampledDir, ray->time);

			++depth;
			sample->result.firstPathVertex = false;
			sample->result.lastPathVertex = (depth == PARAM_MAX_PATH_DEPTH);

			if (sample->result.firstPathVertex)
				sample->result.firstPathVertexEvent = event;

			task->pathStateBase.depth = depth;
			task->directLightState.lastBSDFEvent = event;
			task->directLightState.lastPdfW = lastPdfW;
#if defined(PARAM_HAS_PASSTHROUGH)
			// This is a bit tricky. I store the passThroughEvent in the BSDF
			// before of the initialization because it can be use during the
			// tracing of next path vertex ray.

			// This sampleDataPathVertexBase is used inside Sampler_GetSamplePathVertex() macro
			__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
				sample, sampleDataPathBase, depth);
			task->pathStateBase.bsdf.hitPoint.passThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_PASSTHROUGH);
#endif

			pathState = RT_NEXT_VERTEX;
		} else
			pathState = SPLAT_SAMPLE;
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: SPLAT_SAMPLE
	// To: RT_NEXT_VERTEX
	//--------------------------------------------------------------------------

	if (pathState == SPLAT_SAMPLE) {	
		// Initialize Film radiance group pointer table
		__global float *filmRadianceGroup[PARAM_FILM_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		filmRadianceGroup[0] = filmRadianceGroup0;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		filmRadianceGroup[1] = filmRadianceGroup1;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		filmRadianceGroup[2] = filmRadianceGroup2;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		filmRadianceGroup[3] = filmRadianceGroup3;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		filmRadianceGroup[3] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		filmRadianceGroup[3] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		filmRadianceGroup[3] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		filmRadianceGroup[3] = filmRadianceGroup7;
#endif

		Sampler_NextSample(seed, sample, sampleData
				FILM_PARAM);
		taskStats[gid].sampleCount += 1;

		GenerateCameraPath(task, sample, sampleData, camera, filmWidth, filmHeight, ray, seed);
		// task->pathStateBase.state is set to RT_NEXT_VERTEX inside Sampler_NextSample() => GenerateCameraPath()
		
		// Re-initialize the volume information
#if defined(PARAM_HAS_VOLUMES)
		PathVolumeInfo_Init(&pathVolInfos[gid]);
#endif
	} else {
		// Save the state
		task->pathStateBase.state = pathState;
	}

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;
}

//------------------------------------------------------------------------------
// AdvancePaths (Micro-Kernels)
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_RT_NEXT_VERTEX
// To: MK_HIT_NOTHING or MK_HIT_OBJECT or MK_RT_NEXT_VERTEX
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_RT_NEXT_VERTEX(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	// This has to be done by the first kernel to run after RT kernel
	samples[gid].result.rayCount += 1;
#endif

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_RT_NEXT_VERTEX)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global BSDF *bsdf = &task->pathStateBase.bsdf;

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
#if (PARAM_SAMPLER_TYPE != 0)
	// Used by Sampler_GetSamplePathVertex() macro
	__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, task->pathStateBase.depth);
#endif

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	const bool continueToTrace = Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
			&pathVolInfos[gid],
			&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
			task->pathStateBase.bsdf.hitPoint.passThroughEvent,
#endif
			ray, rayHit, bsdf,
			&task->pathStateBase.throughput,
			&sample->result,
			// BSDF_Init parameters
			meshDescs,
			meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			meshTriLightDefsOffset,
#endif
			vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
			vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
			vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
			vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
			vertAlphas,
#endif
			triangles
			MATERIALS_PARAM
			);

	// If continueToTrace, there is nothing to do, just keep the same state
	if (!continueToTrace) {
		const bool rayMiss = (rayHit->meshIndex == NULL_INDEX);
		task->pathStateBase.state = rayMiss ? MK_HIT_NOTHING : MK_HIT_OBJECT;
	}
#if defined(PARAM_HAS_PASSTHROUGH)
	else {
		// I generate a new random variable starting from the previous one. I'm
		// not really sure about the kind of correlation introduced by this
		// trick.
		task->pathStateBase.bsdf.hitPoint.passThroughEvent = fabs(task->pathStateBase.bsdf.hitPoint.passThroughEvent - .5f) * 2.f;
	}
#endif
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_HIT_NOTHING
// To: MK_SPLAT_SAMPLE
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_HIT_NOTHING(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_HIT_NOTHING)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

	__global Sample *sample = &samples[gid];

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------

	__global Ray *ray = &rays[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Nothing was hit, add environmental lights radiance

#if defined(PARAM_HAS_ENVLIGHTS)
	DirectHitInfiniteLight(
			task->directLightState.lastBSDFEvent,
			&task->pathStateBase.throughput,
			VLOAD3F(&ray->d.x), task->directLightState.lastPdfW,
			&sample->result
			LIGHTS_PARAM);
#endif

	if (task->pathStateBase.depth == 1) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		sample->result.alpha = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		sample->result.depth = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		sample->result.position.x = INFINITY;
		sample->result.position.y = INFINITY;
		sample->result.position.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		sample->result.geometryNormal.x = INFINITY;
		sample->result.geometryNormal.y = INFINITY;
		sample->result.geometryNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		sample->result.shadingNormal.x = INFINITY;
		sample->result.shadingNormal.y = INFINITY;
		sample->result.shadingNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		sample->result.materialID = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		sample->result.uv.u = INFINITY;
		sample->result.uv.v = INFINITY;
#endif
	}

	task->pathStateBase.state = MK_SPLAT_SAMPLE;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_HIT_OBJECT
// To: MK_GENERATE_DL_RAY or MK_SPLAT_SAMPLE
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_HIT_OBJECT(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_HIT_OBJECT)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global BSDF *bsdf = &task->pathStateBase.bsdf;

	__global Sample *sample = &samples[gid];

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------

	__global RayHit *rayHit = &rayHits[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Something was hit

	if (task->pathStateBase.depth == 1) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		sample->result.alpha = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		sample->result.depth = rayHit->t;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		sample->result.position = bsdf->hitPoint.p;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		sample->result.geometryNormal = bsdf->hitPoint.geometryN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		sample->result.shadingNormal = bsdf->hitPoint.shadeN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		sample->result.materialID = BSDF_GetMaterialID(bsdf
				MATERIALS_PARAM);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		sample->result.uv = bsdf->hitPoint.uv;
#endif
	}

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	// Check if it is a light source (note: I can hit only triangle area light sources)
	if (BSDF_IsLightSource(bsdf)) {
		DirectHitFiniteLight(
				task->directLightState.lastBSDFEvent,
				&task->pathStateBase.throughput,
				rayHit->t, bsdf, task->directLightState.lastPdfW,
				&sample->result
				LIGHTS_PARAM);
	}
#endif

	// Check if this is the last path vertex (but not also the first)
	//
	// I handle as a special case when the path vertex is both the first
	// and the last: I do direct light sampling without MIS.
	task->pathStateBase.state = (sample->result.lastPathVertex && !sample->result.firstPathVertex) ?
		MK_SPLAT_SAMPLE : MK_GENERATE_DL_RAY;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_RT_DL
// To: MK_GENERATE_NEXT_VERTEX_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_RT_DL(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_RT_DL)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global BSDF *bsdf = &task->pathStateBase.bsdf;

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
#if (PARAM_SAMPLER_TYPE != 0)
	// Used by Sampler_GetSamplePathVertex() macro
	__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, task->pathStateBase.depth);
#endif

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	const bool continueToTrace = 
#if defined(PARAM_HAS_PASSTHROUGH) || defined(PARAM_HAS_VOLUMES)
		Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
			&directLightVolInfos[gid],
			&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
			task->directLightRayPassThroughEvent,
#endif
			ray, rayHit, &task->tmpBsdf,
			&task->directLightState.lightRadiance,
			NULL,
			// BSDF_Init parameters
			meshDescs,
			meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			meshTriLightDefsOffset,
#endif
			vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
			vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
			vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
			vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
			vertAlphas,
#endif
			triangles
			MATERIALS_PARAM
			);
#else
	false;
#endif
	const bool rayMiss = (rayHit->meshIndex == NULL_INDEX);

	// If continueToTrace, there is nothing to do, just keep the same state
	if (!continueToTrace) {
		if (rayMiss) {
			// Nothing was hit, the light source is visible

			SampleResult_AddDirectLight(&sample->result, task->directLightState.lightID,
					BSDF_GetEventTypes(bsdf
						MATERIALS_PARAM),
					VLOAD3F(task->directLightState.lightRadiance.c),
					1.f);
		}

		// Check if this is the last path vertex
		if (sample->result.lastPathVertex)
			pathState = MK_SPLAT_SAMPLE;
		else
			pathState = MK_GENERATE_NEXT_VERTEX_RAY;

		// Save the state
		task->pathStateBase.state = pathState;
	}
#if defined(PARAM_HAS_PASSTHROUGH)
	else {
		// I generate a new random variable starting from the previous one. I'm
		// not really sure about the kind of correlation introduced by this
		// trick.
		task->directLightRayPassThroughEvent = fabs(task->directLightRayPassThroughEvent - .5f) * 2.f;
	}
#endif

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_GENERATE_DL_RAY
// To: MK_GENERATE_NEXT_VERTEX_RAY or MK_RT_DL or MK_SPLAT_SAMPLE
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_GENERATE_DL_RAY(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_GENERATE_DL_RAY)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	uint depth = task->pathStateBase.depth;

	__global BSDF *bsdf = &task->pathStateBase.bsdf;

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
#if (PARAM_SAMPLER_TYPE != 0)
	// Used by Sampler_GetSamplePathVertex() macro
	__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, depth);
#endif

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------

	__global Ray *ray = &rays[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	if (!BSDF_IsDelta(bsdf
			MATERIALS_PARAM) &&
		DirectLightSampling_ONE(
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			&task->tmpHitPoint,
#endif
			ray->time,
			Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_X),
			Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
			Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
#if defined(PARAM_HAS_PASSTHROUGH)
			Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_W),
#endif
			sample->result.lastPathVertex, depth, &task->pathStateBase.throughput, bsdf,
			ray, &task->directLightState.lightRadiance, &task->directLightState.lightID
			LIGHTS_PARAM)) {
#if defined(PARAM_HAS_PASSTHROUGH)
		// Initialize the pass-through event for the shadow ray
		task->directLightRayPassThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_A);
#endif
#if defined(PARAM_HAS_VOLUMES)
		// Make a copy of current PathVolumeInfo for tracing the
		// shadow ray
		directLightVolInfos[gid] = pathVolInfos[gid];
#endif
		// I have to trace the shadow ray
		pathState = MK_RT_DL;
	} else {
		// No shadow ray to trace, move to the next vertex ray
		// however, I have to Check if this is the last path vertex
		if (sample->result.lastPathVertex)
			pathState = MK_SPLAT_SAMPLE;
		else
			pathState = MK_GENERATE_NEXT_VERTEX_RAY;
	}

	// Save the state
	task->pathStateBase.state = pathState;

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_GENERATE_NEXT_VERTEX_RAY
// To: MK_SPLAT_SAMPLE or MK_RT_NEXT_VERTEX
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_GENERATE_NEXT_VERTEX_RAY)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	uint depth = task->pathStateBase.depth;

	__global BSDF *bsdf = &task->pathStateBase.bsdf;

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
#if (PARAM_SAMPLER_TYPE != 0)
	// Used by Sampler_GetSamplePathVertex() macro
	__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, depth);
#endif

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------

	__global Ray *ray = &rays[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Sample the BSDF
	float3 sampledDir;
	float lastPdfW;
	float cosSampledDir;
	BSDFEvent event;

	const float3 bsdfSample = BSDF_Sample(bsdf,
			Sampler_GetSamplePathVertex(depth, IDX_BSDF_X),
			Sampler_GetSamplePathVertex(depth, IDX_BSDF_Y),
			&sampledDir, &lastPdfW, &cosSampledDir, &event, ALL
			MATERIALS_PARAM);

	// Russian Roulette
	const float rrProb = RussianRouletteProb(bsdfSample);
	const bool rrEnabled = (depth >= PARAM_RR_DEPTH) && !(event & SPECULAR);
	const bool rrContinuePath = !rrEnabled || (Sampler_GetSamplePathVertex(depth, IDX_RR) < rrProb);

	// Max. path depth
	const bool maxPathDepth = (depth >= PARAM_MAX_PATH_DEPTH);

	const bool continuePath = !Spectrum_IsBlack(bsdfSample) && rrContinuePath && !maxPathDepth;
	if (continuePath) {
		float3 throughput = VLOAD3F(task->pathStateBase.throughput.c);
		throughput *= bsdfSample;
		if (rrEnabled)
			throughput /= rrProb; // Russian Roulette

		VSTORE3F(throughput, task->pathStateBase.throughput.c);

#if defined(PARAM_HAS_VOLUMES)
		// Update volume information
		PathVolumeInfo_Update(&pathVolInfos[gid], event, bsdf
				MATERIALS_PARAM);
#endif

		Ray_Init2(ray, VLOAD3F(&bsdf->hitPoint.p.x), sampledDir, ray->time);

		++depth;
		sample->result.firstPathVertex = false;
		sample->result.lastPathVertex = (depth == PARAM_MAX_PATH_DEPTH);

		if (sample->result.firstPathVertex)
			sample->result.firstPathVertexEvent = event;

		task->pathStateBase.depth = depth;
		task->directLightState.lastBSDFEvent = event;
		task->directLightState.lastPdfW = lastPdfW;
#if defined(PARAM_HAS_PASSTHROUGH)
		// This is a bit tricky. I store the passThroughEvent in the BSDF
		// before of the initialization because it can be use during the
		// tracing of next path vertex ray.

		// This sampleDataPathVertexBase is used inside Sampler_GetSamplePathVertex() macro
		__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, depth);
		task->pathStateBase.bsdf.hitPoint.passThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_PASSTHROUGH);
#endif

		pathState = MK_RT_NEXT_VERTEX;
	} else
		pathState = MK_SPLAT_SAMPLE;

	// Save the state
	task->pathStateBase.state = pathState;

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_SPLAT_SAMPLE
// To: MK_GENERATE_CAMERA_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_SPLAT_SAMPLE(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_SPLAT_SAMPLE)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Initialize Film radiance group pointer table
	__global float *filmRadianceGroup[PARAM_FILM_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	filmRadianceGroup[0] = filmRadianceGroup0;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	filmRadianceGroup[1] = filmRadianceGroup1;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	filmRadianceGroup[2] = filmRadianceGroup2;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	filmRadianceGroup[3] = filmRadianceGroup3;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	filmRadianceGroup[3] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroup[3] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroup[3] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	filmRadianceGroup[3] = filmRadianceGroup7;
#endif

	Sampler_NextSample(seed, sample, sampleData
			FILM_PARAM);
	taskStats[gid].sampleCount += 1;

	// Save the state
	task->pathStateBase.state = MK_GENERATE_CAMERA_RAY;

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_GENERATE_CAMERA_RAY
// To: MK_RT_NEXT_VERTEX
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_GENERATE_CAMERA_RAY(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	PathState pathState = task->pathStateBase.state;
	if (pathState != MK_GENERATE_CAMERA_RAY)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	__global Ray *ray = &rays[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	GenerateCameraPath(task, sample, sampleData, camera, filmWidth, filmHeight, ray, seed);
	// task->pathStateBase.state is set to RT_NEXT_VERTEX inside GenerateCameraPath()

	// Re-initialize the volume information
#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo_Init(&pathVolInfos[gid]);
#endif

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;
}
