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
//  PARAM_IMAGE_FILTER_TYPE (0 = No filter, 1 = Box, 2 = Gaussian, 3 = Mitchell)
//  PARAM_IMAGE_FILTER_WIDTH_X
//  PARAM_IMAGE_FILTER_WIDTH_Y
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
#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

#if (PARAM_SAMPLER_TYPE == 2)
#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

	Camera_GenerateRay(camera, filmWidth, filmHeight, ray,
			sample->result.filmX, sample->result.filmY
#if defined(PARAM_CAMERA_HAS_DOF)
			, dofSampleX, dofSampleY
#endif
			);

	// Initialize the path state
	task->pathStateBase.state = RT_NEXT_VERTEX;
	task->pathStateBase.depth = 1;
	VSTORE3F(WHITE, &task->pathStateBase.throughput.r);
	task->directLightState.pathBSDFEvent = NONE;
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
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Init(
		uint seedBase,
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Sample *samples,
		__global float *samplesData,
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
}

//------------------------------------------------------------------------------
// AdvancePaths Kernel
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SUNLIGHT)
void DirectHitInfiniteLight(
		const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent,
		const BSDFEvent pathBSDFEvent,
		__global float *lightsDistribution,
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
		__global float *infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
		__global const Spectrum *pathThroughput,
		const float3 eyeDir, const float lastPdfW,
		__global SampleResult *sampleResult
		IMAGEMAPS_PARAM_DECL) {
	const float3 throughput = VLOAD3F(&pathThroughput->r);

#if defined(PARAM_HAS_INFINITELIGHT)
	{
		float directPdfW;
		const float3 infiniteLightRadiance = InfiniteLight_GetRadiance(infiniteLight,
				infiniteLightDistribution, eyeDir, &directPdfW
				IMAGEMAPS_PARAM);
		if (!Spectrum_IsBlack(infiniteLightRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, infiniteLight->lightSceneIndex);
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
			const float3 lightRadiance = weight * throughput * infiniteLightRadiance;

			const uint lightID = min(infiniteLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
			AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
		}
	}
#endif
#if defined(PARAM_HAS_SKYLIGHT)
	{
		float directPdfW;
		const float3 skyRadiance = SkyLight_GetRadiance(skyLight, eyeDir, &directPdfW);
		if (!Spectrum_IsBlack(skyRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, skyLight->lightSceneIndex);
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
			const float3 lightRadiance = weight * throughput * skyRadiance;

			const uint lightID = min(skyLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
			AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
		}
	}
#endif
#if defined(PARAM_HAS_SUNLIGHT)
	{
		float directPdfW;
		const float3 sunRadiance = SunLight_GetRadiance(sunLight, eyeDir, &directPdfW);
		if (!Spectrum_IsBlack(sunRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, sunLight->lightSceneIndex);
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
			const float3 lightRadiance = weight * throughput * sunRadiance;

			const uint lightID = min(sunLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
			AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
		}
	}
#endif
}
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
void DirectHitFiniteLight(
		const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent,
		const BSDFEvent pathBSDFEvent,
		__global float *lightsDistribution,
		__global TriangleLight *triLightDefs,
		__global const Spectrum *pathThroughput, const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global SampleResult *sampleResult
		MATERIALS_PARAM_DECL) {
	float directPdfA;
	const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf,
			triLightDefs, &directPdfA
			MATERIALS_PARAM);

	if (!Spectrum_IsBlack(emittedRadiance)) {
		// Add emitted radiance
		float weight = 1.f;
		if (!(lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution,
					triLightDefs[bsdf->triangleLightSourceIndex].lightSceneIndex);
			const float directPdfW = PdfAtoW(directPdfA, distance,
				fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		}
		const float3 lightRadiance = weight * VLOAD3F(&pathThroughput->r) * emittedRadiance;

		const uint lightID =  min(BSDF_GetLightID(bsdf
				MATERIALS_PARAM), PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
		AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
	}
}
#endif

float RussianRouletteProb(const float3 color) {
	return clamp(Spectrum_Filter(color), PARAM_RR_CAP, 1.f);
}

bool DirectLightSampling(
		const uint lightIndex,
		const float lightPickPdf,
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
		__global float *infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global TriangleLight *triLightDefs,
		__global HitPoint *tmpHitPoint,
#endif
		__global float *lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float u3,
		__global float *shadowPassThrought,
#endif
		const float u0, const float u1, const float u2,
		const uint depth,
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		MATERIALS_PARAM_DECL) {
	float3 lightRayDir;
	float distance, directPdfW;
	float3 lightRadiance;
	uint lightID;

#if defined(PARAM_HAS_INFINITELIGHT)
	const uint infiniteLightIndex = PARAM_TRIANGLE_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
		+ 1
#endif
	;

	if (lightIndex == infiniteLightIndex) {
		lightRadiance = InfiniteLight_Illuminate(
			infiniteLight,
			infiniteLightDistribution,
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			u0, u1,
			VLOAD3F(&bsdf->hitPoint.p.x),
			&lightRayDir, &distance, &directPdfW
			IMAGEMAPS_PARAM);
		lightID = infiniteLight->lightID;
	}
#endif

#if defined(PARAM_HAS_SKYLIGHT)
	const uint skyLightIndex = PARAM_TRIANGLE_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
		+ 1
#endif
	;

	if (lightIndex == skyLightIndex) {
		lightRadiance = SkyLight_Illuminate(
			skyLight,
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			u0, u1,
			VLOAD3F(&bsdf->hitPoint.p.x),
			&lightRayDir, &distance, &directPdfW);
		lightID = skyLight->lightID;
	}
#endif

#if defined(PARAM_HAS_SUNLIGHT)
	const uint sunLightIndex = PARAM_TRIANGLE_LIGHT_COUNT;
	if (lightIndex == sunLightIndex) {
		lightRadiance = SunLight_Illuminate(
			sunLight,
			u0, u1,
			&lightRayDir, &distance, &directPdfW);
		lightID = sunLight->lightID;
	}
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	if (lightIndex < PARAM_TRIANGLE_LIGHT_COUNT) {
		lightRadiance = TriangleLight_Illuminate(
			&triLightDefs[lightIndex], tmpHitPoint,
			VLOAD3F(&bsdf->hitPoint.p.x),
			u0, u1, u2,
			&lightRayDir, &distance, &directPdfW
			MATERIALS_PARAM);
		lightID = mats[triLightDefs[lightIndex].materialIndex].lightID;
	}
#endif

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
			const float factor = cosThetaToLight / directLightSamplingPdfW;

			// Russian Roulette
			bsdfPdfW *= (depth >= PARAM_RR_DEPTH) ? RussianRouletteProb(bsdfEval) : 1.f;

			// MIS between direct light sampling and BSDF sampling
			const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

			VSTORE3F((weight * factor) * VLOAD3F(&pathThroughput->r) * bsdfEval * lightRadiance, &radiance->r);
			*ID = min(lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
#if defined(PARAM_HAS_PASSTHROUGH)
			*shadowPassThrought = u3;
#endif

			// Setup the shadow ray
			const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
			const float epsilon = fmax(MachineEpsilon_E_Float3(hitPoint), MachineEpsilon_E(distance));

			Ray_Init4(shadowRay, hitPoint, lightRayDir,
				epsilon,
				distance - epsilon);

			return true;
		}
	}

	return false;
}

bool DirectLightSampling_ONE(
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
		__global float *infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global TriangleLight *triLightDefs,
		__global HitPoint *tmpHitPoint,
#endif
		__global float *lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float u4,
		__global float *shadowPassThrought,
#endif
		const float u0, const float u1, const float u2, const float u3,
		const uint depth,
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		MATERIALS_PARAM_DECL) {
	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightsDistribution, u0, &lightPickPdf);

	return DirectLightSampling(
		lightIndex,
		lightPickPdf,
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		worldCenterX,
		worldCenterY,
		worldCenterZ,
		worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		infiniteLight,
		infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		triLightDefs,
		tmpHitPoint,
#endif
		lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
		u4,
		shadowPassThrought,
#endif
		u1, u2, u3,
		depth,
		pathThroughput, bsdf,
		shadowRay, radiance, ID
		MATERIALS_PARAM);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Sample *samples,
		__global float *samplesData,
		__global Ray *rays,
		__global RayHit *rayHits,
		// Film parameters
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		, __global float *filmRadianceGroup0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		, __global float *filmRadianceGroup1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		, __global float *filmRadianceGroup2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		, __global float *filmRadianceGroup3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		, __global float *filmRadianceGroup4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		, __global float *filmRadianceGroup5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		, __global float *filmRadianceGroup6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		, __global float *filmRadianceGroup7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		, __global float *filmAlpha
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		, __global float *filmDepth
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		, __global float *filmPosition
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		, __global float *filmGeometryNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		, __global float *filmShadingNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		, __global uint *filmMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
		, __global float *filmDirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
		, __global float *filmDirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
		, __global float *filmEmission
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
		, __global float *filmIndirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
		, __global float *filmIndirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
		, __global float *filmIndirectSpecular
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
		, __global float *filmMaterialIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
		, __global float *filmDirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		, __global float *filmIndirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		, __global float *filmUV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
		, __global float *filmRayCount
#endif
		,
		// Scene parameters
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		__global Material *mats,
		__global Texture *texs,
		__global uint *meshMats,
		__global Mesh *meshDescs,
		__global Point *vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global Vector *vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global UV *vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
		__global Spectrum *vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
		__global float *vertAlphas,
#endif
		__global Triangle *triangles,
		__global Camera *camera,
		__global float *lightsDistribution
#if defined(PARAM_HAS_INFINITELIGHT)
		, __global InfiniteLight *infiniteLight
		, __global float *infiniteLightDistribution
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		, __global SunLight *sunLight
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		, __global SkyLight *skyLight
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		, __global TriangleLight *triLightDefs
		, __global uint *meshTriLightDefsOffset
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_0)
		, __global ImageMap *imageMapDescs, __global float *imageMapBuff0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
		, __global float *imageMapBuff1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
		, __global float *imageMapBuff2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
		, __global float *imageMapBuff3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
		, __global float *imageMapBuff4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
		, __global float *imageMapBuff5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
		, __global float *imageMapBuff6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
		, __global float *imageMapBuff7
#endif
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
	const uint depth = task->pathStateBase.depth;
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
	const bool rayMiss = (rayHit->meshIndex == NULL_INDEX);
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
		if (!rayMiss) {
			//------------------------------------------------------------------
			// Something was hit
			//------------------------------------------------------------------

			BSDF_Init(bsdf,
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
					triangles, ray, rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
					, task->pathStateBase.bsdf.hitPoint.passThroughEvent
#endif
#if defined(PARAM_HAS_BUMPMAPS) || defined(PARAM_HAS_NORMALMAPS)
					MATERIALS_PARAM
#endif
					);

#if defined(PARAM_HAS_PASSTHROUGH)
			const float3 passThroughTrans = BSDF_GetPassThroughTransparency(bsdf
					MATERIALS_PARAM);
			if (!Spectrum_IsBlack(passThroughTrans)) {
				const float3 pathThroughput = VLOAD3F(&task->pathStateBase.throughput.r) * passThroughTrans;
				VSTORE3F(pathThroughput, &task->pathStateBase.throughput.r);

				// It is a pass through point, continue to trace the ray
				ray->mint = rayHit->t + MachineEpsilon_E(rayHit->t);

				// Keep the same path state
			} else
#endif
			{
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
					DirectHitFiniteLight((depth == 1), task->directLightState.lastBSDFEvent,
							task->directLightState.pathBSDFEvent, lightsDistribution,
							triLightDefs, &task->pathStateBase.throughput,
							rayHit->t, bsdf, task->directLightState.lastPdfW,
							&sample->result
							MATERIALS_PARAM);
				}
#endif

				// Direct light sampling
				pathState = GENERATE_DL_RAY;
			}
		} else {
			//------------------------------------------------------------------
			// Nothing was hit, add environmental lights radiance
			//------------------------------------------------------------------

#if defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SUNLIGHT)
			DirectHitInfiniteLight(
					(depth == 1),
					task->directLightState.lastBSDFEvent,
					task->directLightState.pathBSDFEvent,
					lightsDistribution,
#if defined(PARAM_HAS_INFINITELIGHT)
					infiniteLight,
					infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
					sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
					skyLight,
#endif
					&task->pathStateBase.throughput,
					-VLOAD3F(&ray->d.x), task->directLightState.lastPdfW,
					&sample->result
					IMAGEMAPS_PARAM);
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
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
				sample->result.directShadowMask = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
				sample->result.indirectShadowMask = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
				sample->result.uv.u = INFINITY;
				sample->result.uv.v = INFINITY;
#endif
			}

			pathState = SPLAT_SAMPLE;
		}
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_DL
	// To: GENERATE_NEXT_VERTEX_RAY
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_SUNLIGHT) || defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	if (pathState == RT_DL) {
		pathState = GENERATE_NEXT_VERTEX_RAY;

		if (rayMiss) {
			// Nothing was hit, the light source is visible
			const float3 lightRadiance = VLOAD3F(&task->directLightState.lightRadiance.r);

			const uint lightID = task->directLightState.lightID;
			VADD3F(&sample->result.radiancePerPixelNormalized[lightID].r, lightRadiance);

			if (depth == 1) {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
				sample->result.directShadowMask = 0.f;
#endif
				if (BSDF_GetEventTypes(bsdf
						MATERIALS_PARAM) & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
					VADD3F(&sample->result.directDiffuse.r, lightRadiance);
#endif
				} else {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
					VADD3F(&sample->result.directGlossy.r, lightRadiance);
#endif
				}
			} else {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
				sample->result.indirectShadowMask = 0.f;
#endif

				const BSDFEvent pathBSDFEvent = task->directLightState.pathBSDFEvent;
				if (pathBSDFEvent & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
					VADD3F(&sample->result.indirectDiffuse.r, lightRadiance);
#endif
				} else if (pathBSDFEvent & GLOSSY) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
					VADD3F(&sample->result.indirectGlossy.r, lightRadiance);
#endif
				} else if (pathBSDFEvent & SPECULAR) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
					VADD3F(&sample->result.indirectSpecular.r, lightRadiance);
#endif
				}
			}
		}
#if defined(PARAM_HAS_PASSTHROUGH)
		else {
			BSDF_Init(&task->passThroughState.passThroughBsdf,
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
					triangles, ray, rayHit,
					task->passThroughState.passThroughEvent
#if defined(PARAM_HAS_BUMPMAPS) || defined(PARAM_HAS_NORMALMAPS)
					MATERIALS_PARAM
#endif
					);

			const float3 passthroughTrans = BSDF_GetPassThroughTransparency(&task->passThroughState.passThroughBsdf
					MATERIALS_PARAM);
			if (!Spectrum_IsBlack(passthroughTrans)) {
				const float3 lightRadiance = VLOAD3F(&task->directLightState.lightRadiance.r) * passthroughTrans;
				VSTORE3F(lightRadiance, &task->directLightState.lightRadiance.r);

				// It is a pass through point, continue to trace the ray
				ray->mint = rayHit->t + MachineEpsilon_E(rayHit->t);
				pathState = RT_DL;
			}
		}
#endif
	}
#endif

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: GENERATE_DL_RAY
	// To: GENERATE_NEXT_VERTEX_RAY or RT_DL or SPLAT_SAMPLE
	//--------------------------------------------------------------------------

	if (pathState == GENERATE_DL_RAY) {
		if (depth > PARAM_MAX_PATH_DEPTH) {
			pathState = SPLAT_SAMPLE;
		} else {
			// No shadow ray to trace, move to the next vertex ray
			pathState = GENERATE_NEXT_VERTEX_RAY;

			if (BSDF_IsDelta(bsdf
				MATERIALS_PARAM)) {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
				if (depth == 1)
					sample->result.directShadowMask = 0.f;
#endif
			} else {
				if (DirectLightSampling_ONE(
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
						worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
						infiniteLight,
						infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
						sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
						skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
						triLightDefs,
						&task->directLightState.tmpHitPoint,
#endif
						lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
						Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_A),
						&task->passThroughState.passThroughEvent,
#endif
						Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_X),
						Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
						Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
						Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_W),
						depth, &task->pathStateBase.throughput, bsdf,
						ray, &task->directLightState.lightRadiance, &task->directLightState.lightID
						MATERIALS_PARAM)) {
					// I have to trace the shadow ray
					pathState = RT_DL;
				}
			}
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

		const bool continuePath = !Spectrum_IsBlack(bsdfSample) && rrContinuePath;
		if (continuePath) {
			if (rrEnabled)
				lastPdfW *= rrProb; // Russian Roulette

			float3 throughput = VLOAD3F(&task->pathStateBase.throughput.r);
			throughput *= bsdfSample * (cosSampledDir / lastPdfW);
			VSTORE3F(throughput, &task->pathStateBase.throughput.r);

			Ray_Init2(ray, VLOAD3F(&bsdf->hitPoint.p.x), sampledDir);

			task->pathStateBase.depth = depth + 1;
			if (depth == 1)
				task->directLightState.pathBSDFEvent = event;
			task->directLightState.lastBSDFEvent = event;
			task->directLightState.lastPdfW = lastPdfW;
#if defined(PARAM_HAS_PASSTHROUGH)
			// This is a bit tricky. I store the passThroughEvent in the BSDF
			// before of the initialization because it can be use during the
			// tracing of next path vertex ray.

			// This sampleDataPathVertexBase is used inside Sampler_GetSamplePathVertex() macro
			__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
				sample, sampleDataPathBase, depth + 1);
			task->pathStateBase.bsdf.hitPoint.passThroughEvent = Sampler_GetSamplePathVertex(depth + 1, IDX_PASSTHROUGH);
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
