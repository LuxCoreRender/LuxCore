#line 2 "patchocl_kernels.cl"

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

// List of symbols defined at compile time:
//  PARAM_TASK_COUNT
//  PARAM_IMAGE_WIDTH
//  PARAM_IMAGE_HEIGHT
//  PARAM_RAY_EPSILON_MIN
//  PARAM_RAY_EPSILON_MAX
//  PARAM_MAX_PATH_DEPTH
//  PARAM_RR_DEPTH
//  PARAM_RR_CAP
//  PARAM_HAS_IMAGEMAPS
//  PARAM_HAS_PASSTHROUGH
//  PARAM_USE_PIXEL_ATOMICS
//  PARAM_HAS_BUMPMAPS
//  PARAM_HAS_NORMALMAPS
//  PARAM_ACCEL_BVH or PARAM_ACCEL_MBVH or PARAM_ACCEL_QBVH or PARAM_ACCEL_MQBVH
//  PARAM_DEVICE_INDEX
//  PARAM_DEVICE_COUNT
//  PARAM_LIGHT_WORLD_RADIUS_SCALE
//  PARAM_DL_LIGHT_COUNT

// To enable single material support
//  PARAM_ENABLE_MAT_MATTE
//  PARAM_ENABLE_MAT_MIRROR
//  PARAM_ENABLE_MAT_GLASS
//  PARAM_ENABLE_MAT_METAL
//  PARAM_ENABLE_MAT_ARCHGLASS
//  PARAM_ENABLE_MAT_MIX
//  PARAM_ENABLE_MAT_NULL
//  PARAM_ENABLE_MAT_MATTETRANSLUCENT

// To enable single texture support
//  PARAM_ENABLE_TEX_CONST_FLOAT
//  PARAM_ENABLE_TEX_CONST_FLOAT3
//  PARAM_ENABLE_TEX_CONST_FLOAT4
//  PARAM_ENABLE_TEX_IMAGEMAP
//  PARAM_ENABLE_TEX_SCALE

// (optional)
//  PARAM_CAMERA_HAS_DOF

// (optional)
//  PARAM_HAS_INFINITELIGHT

// (optional)
//  PARAM_HAS_SUNLIGHT

// (optional)
//  PARAM_HAS_SKYLIGHT

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

// (optional)
//  PARAM_ENABLE_ALPHA_CHANNEL

// (optional)
//  PARAM_HAS_NORMALS_BUFFER
//  PARAM_HAS_UVS_BUFFER
//  PARAM_HAS_COLS_BUFFER
//  PARAM_HAS_ALPHAS_BUFFER

//------------------------------------------------------------------------------
// Init Kernel
//------------------------------------------------------------------------------

void GenerateCameraPath(
		__global GPUTask *task,
		__global float *sampleData,
		__global Camera *camera,
		__global Ray *ray,
		Seed *seed) {
#if (PARAM_SAMPLER_TYPE == 0)

	const float scrSampleX = sampleData[IDX_SCREEN_X];
	const float scrSampleY = sampleData[IDX_SCREEN_Y];
#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Rnd_FloatValue(seed);
	const float dofSampleY = Rnd_FloatValue(seed);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Rnd_FloatValue(seed);
#endif
#endif

#if (PARAM_SAMPLER_TYPE == 1)
	__global Sample *sample = &task->sample;
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	const float scrSampleX = Sampler_GetSamplePath(IDX_SCREEN_X);
	const float scrSampleY = Sampler_GetSamplePath(IDX_SCREEN_Y);
#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

#if (PARAM_SAMPLER_TYPE == 2)
	__global Sample *sample = &task->sample;
	const float scrSampleX = sampleData[IDX_SCREEN_X];
	const float scrSampleY = sampleData[IDX_SCREEN_Y];
#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	const float eyePassthrough = Sampler_GetSamplePath(IDX_EYE_PASSTHROUGH);
#endif
#endif

	Camera_GenerateRay(camera, ray, scrSampleX, scrSampleY
#if defined(PARAM_CAMERA_HAS_DOF)
			, dofSampleX, dofSampleY
#endif
			);

	// Initialize the path state
	task->pathStateBase.state = RT_NEXT_VERTEX;
	task->pathStateBase.depth = 1;
	VSTORE3F(WHITE, &task->pathStateBase.throughput.r);
	task->directLightState.lastPdfW = 1.f;
	task->directLightState.lastSpecular = TRUE;
#if defined(PARAM_HAS_PASSTHROUGH)
	// This is a bit tricky. I store the passThroughEvent in the BSDF
	// before of the initialization because it can be use during the
	// tracing of next path vertex ray.

	task->pathStateBase.bsdf.hitPoint.passThroughEvent = eyePassthrough;
#endif
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Init(
		uint seedBase,
		__global GPUTask *tasks,
		__global float *samplesData,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global Camera *camera
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
	__global Sample *sample = &task->sample;
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	Sampler_Init(&seed, sample, sampleData);
	GenerateCameraPath(task, sampleData, camera, &rays[gid], &seed);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->sampleCount = 0;
}

//------------------------------------------------------------------------------
// InitFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitFrameBuffer(
		__global Pixel *frameBuffer
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
#endif
		) {
	const size_t gid = get_global_id(0);
	if (gid >= (PARAM_IMAGE_WIDTH + 2) * (PARAM_IMAGE_HEIGHT + 2))
		return;

	VSTORE4F(0.f, &frameBuffer[gid].c.r);

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	__global AlphaPixel *ap = &alphaFrameBuffer[gid];
	ap->alpha = 0.f;
#endif
}

//------------------------------------------------------------------------------
// AdvancePaths Kernel
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SUNLIGHT)
void DirectHitInfiniteLight(
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
		const bool lastSpecular, __global const float *pathTroughput,
		const float3 eyeDir, const float lastPdfW,
		__global float *sampleRadiance
		IMAGEMAPS_PARAM_DECL) {
	float3 radiance = VLOAD3F(sampleRadiance);
	const float3 pathThroughput = VLOAD3F(pathTroughput);
	float3 lightRadiance = BLACK;

#if defined(PARAM_HAS_INFINITELIGHT)
	{
		float directPdfW;
		const float3 radiance = InfiniteLight_GetRadiance(infiniteLight, eyeDir, &directPdfW
				IMAGEMAPS_PARAM);
		if (!Spectrum_IsBlack(radiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float weight = (lastSpecular ? 1.f : PowerHeuristic(lastPdfW, directPdfW));
			lightRadiance += weight * radiance;
		}
	}
#endif
#if defined(PARAM_HAS_SKYLIGHT)
	{
		float directPdfW;
		const float3 radiance = SkyLight_GetRadiance(skyLight, eyeDir, &directPdfW);
		if (!Spectrum_IsBlack(radiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float weight = (lastSpecular ? 1.f : PowerHeuristic(lastPdfW, directPdfW));
			lightRadiance += weight * radiance;
		}
	}
#endif
#if defined(PARAM_HAS_SUNLIGHT)
	{
		float directPdfW;
		const float3 radiance = SunLight_GetRadiance(sunLight, eyeDir, &directPdfW);
		if (!Spectrum_IsBlack(radiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float weight = (lastSpecular ? 1.f : PowerHeuristic(lastPdfW, directPdfW));
			lightRadiance += weight * radiance;
		}
	}
#endif

	radiance += pathThroughput * lightRadiance;
	VSTORE3F(radiance, sampleRadiance);
}
#endif

#if (PARAM_DL_LIGHT_COUNT > 0)
void DirectHitFiniteLight(
		__global TriangleLight *triLightDefs, const bool lastSpecular,
		__global const float *pathTroughput, const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global float *sampleRadiance
		MATERIALS_PARAM_DECL) {
	float directPdfA;
	const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf,
			triLightDefs, &directPdfA
			MATERIALS_PARAM);

	if (!Spectrum_IsBlack(emittedRadiance)) {
		// Add emitted radiance
		float weight = 1.f;
		if (!lastSpecular) {
			const float lightPickProb = Scene_PickLightPdf();
			const float directPdfW = PdfAtoW(directPdfA, distance,
				fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		}

		float3 radiance = VLOAD3F(sampleRadiance);
		const float3 pathThroughput = VLOAD3F(pathTroughput);
		const float3 le = pathThroughput * weight * emittedRadiance;
		radiance += le;
		VSTORE3F(radiance, sampleRadiance);
	}
}
#endif

float RussianRouletteProb(const float3 color) {
	return clamp(Spectrum_Filter(color), PARAM_RR_CAP, 1.f);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global float *samplesData,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
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
		__global Camera *camera
#if defined(PARAM_HAS_INFINITELIGHT)
		, __global InfiniteLight *infiniteLight
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		, __global SunLight *sunLight
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		, __global SkyLight *skyLight
#endif
#if (PARAM_DL_LIGHT_COUNT > 0)
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
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
#endif
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	__global GPUTask *task = &tasks[gid];

	// Read the path state
	PathState pathState = task->pathStateBase.state;
	const uint depth = task->pathStateBase.depth;
	__global BSDF *bsdf = &task->pathStateBase.bsdf;

	__global Sample *sample = &task->sample;
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

#if defined(PARAM_HAS_IMAGEMAPS)
	// Initialize image maps page pointer table
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

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	const bool rayMiss = (rayHit->meshIndex == NULL_INDEX);

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_NEXT_VERTEX
	// To: SPLAT_SAMPLE or GENERATE_DL_RAY
	//--------------------------------------------------------------------------

	if (pathState == RT_NEXT_VERTEX) {
		if (!rayMiss) {
			// Something was hit

			BSDF_Init(bsdf,
					meshDescs,
					meshMats,
#if (PARAM_DL_LIGHT_COUNT > 0)
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
			}
#endif
#if defined(PARAM_HAS_PASSTHROUGH) && (PARAM_DL_LIGHT_COUNT > 0)
			else
#endif
			{
#if (PARAM_DL_LIGHT_COUNT > 0)
				// Check if it is a light source (note: I can hit only triangle area light sources)
				if (BSDF_IsLightSource(bsdf)) {
					DirectHitFiniteLight(triLightDefs, task->directLightState.lastSpecular,
							&task->pathStateBase.throughput.r,
							rayHit->t, bsdf, task->directLightState.lastPdfW,
							&sample->radiance.r
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
#if defined(PARAM_HAS_INFINITELIGHT)
				infiniteLight,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
				sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
				skyLight,
#endif
				task->directLightState.lastSpecular, &task->pathStateBase.throughput.r,
				-VLOAD3F(&ray->d.x), task->directLightState.lastPdfW,
				&sample->radiance.r
				IMAGEMAPS_PARAM);
#endif

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			if (depth == 1)
				sample->alpha = 0.f;
#endif

			pathState = SPLAT_SAMPLE;
		}
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_DL
	// To: GENERATE_NEXT_VERTEX_RAY
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_SUNLIGHT) || defined(PARAM_HAS_SKYLIGHT) || (PARAM_HAS_INFINITELIGHT) || (PARAM_DL_LIGHT_COUNT > 0)
	if (pathState == RT_DL) {
		pathState = GENERATE_NEXT_VERTEX_RAY;

		if (rayMiss) {
			// Nothing was hit, the light source is visible
			float3 radiance = VLOAD3F(&sample->radiance.r);
			const float3 lightRadiance = VLOAD3F(&task->directLightState.lightRadiance.r);
			radiance += lightRadiance;
			VSTORE3F(radiance, &sample->radiance.r);
		}
#if defined(PARAM_HAS_PASSTHROUGH)
		else {
			BSDF_Init(&task->passThroughState.passThroughBsdf,
					meshDescs,
					meshMats,
#if (PARAM_DL_LIGHT_COUNT > 0)
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
	// To: GENERATE_NEXT_VERTEX_RAY or RT_DL
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_SUNLIGHT) || defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || (PARAM_DL_LIGHT_COUNT > 0)
	if (pathState == GENERATE_DL_RAY) {
		pathState = GENERATE_NEXT_VERTEX_RAY;

		if (!BSDF_IsDelta(bsdf
				MATERIALS_PARAM)) {
			float3 lightRayDir;
			float distance, directPdfW;
			float3 lightRadiance;

			// Pick a light source to sample
			const float lu0 = Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_X);
			const float lightPickPdf = Scene_PickLightPdf();

			const uint lightsSize = PARAM_DL_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
				+ 1
#endif
#if defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT)
				+ 1
#endif
			;

			const uint lightIndex = min((uint)floor((lightsSize - 1) * lu0), (uint)(lightsSize - 1));

#if defined(PARAM_HAS_INFINITELIGHT)
			const uint infiniteLightIndex = PARAM_DL_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
				+ 1
#endif
			;

			if (lightIndex == infiniteLightIndex) {
				lightRadiance = InfiniteLight_Illuminate(
					infiniteLight,
					worldCenterX, worldCenterY, worldCenterZ, worldRadius,
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
					VLOAD3F(&bsdf->hitPoint.p.x),
					&lightRayDir, &distance, &directPdfW
					IMAGEMAPS_PARAM);
			}
#endif

#if defined(PARAM_HAS_SKYLIGHT)
			const uint skyLightIndex = PARAM_DL_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
				+ 1
#endif
			;

			if (lightIndex == skyLightIndex) {
				lightRadiance = SkyLight_Illuminate(
					skyLight,
					worldCenterX, worldCenterY, worldCenterZ, worldRadius,
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
					VLOAD3F(&bsdf->hitPoint.p.x),
					&lightRayDir, &distance, &directPdfW);
			}
#endif

#if defined(PARAM_HAS_SUNLIGHT)
			const uint sunLightIndex = PARAM_DL_LIGHT_COUNT;
			if (lightIndex == sunLightIndex) {
				lightRadiance = SunLight_Illuminate(
					sunLight,
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
					&lightRayDir, &distance, &directPdfW);
			}
#endif

#if (PARAM_DL_LIGHT_COUNT > 0)
			if (lightIndex < PARAM_DL_LIGHT_COUNT) {
				lightRadiance = TriangleLight_Illuminate(
					&triLightDefs[lightIndex], &task->directLightState.tmpHitPoint,
					VLOAD3F(&bsdf->hitPoint.p.x),
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
					Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_W),
					&lightRayDir, &distance, &directPdfW
					MATERIALS_PARAM);
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
					const float3 pathThroughput = VLOAD3F(&task->pathStateBase.throughput.r);
					const float cosThetaToLight = fabs(dot(lightRayDir, VLOAD3F(&bsdf->hitPoint.shadeN.x)));
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;
					const float factor = cosThetaToLight / directLightSamplingPdfW;

					// Russian Roulette
					bsdfPdfW *= (depth >= PARAM_RR_DEPTH) ? RussianRouletteProb(bsdfEval) : 1.f;

					// MIS between direct light sampling and BSDF sampling
					const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

					VSTORE3F((weight * factor) * pathThroughput * bsdfEval * lightRadiance, &task->directLightState.lightRadiance.r);
#if defined(PARAM_HAS_PASSTHROUGH)
					task->passThroughState.passThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_A);
#endif

					// Setup the shadow ray
					const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
					const float epsilon = fmax(MachineEpsilon_E_Float3(hitPoint), MachineEpsilon_E(distance));
					Ray_Init4(ray, hitPoint, lightRayDir,
						epsilon,
						distance - epsilon);
					pathState = RT_DL;
				}
			}
		}
	}
#endif

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: GENERATE_NEXT_VERTEX_RAY
	// To: SPLAT_SAMPLE or RT_NEXT_VERTEX
	//--------------------------------------------------------------------------

	if (pathState == GENERATE_NEXT_VERTEX_RAY) {
		if (depth < PARAM_MAX_PATH_DEPTH) {
			// Sample the BSDF
			__global BSDF *bsdf = &task->pathStateBase.bsdf;
			float3 sampledDir;
			float lastPdfW;
			float cosSampledDir;
			BSDFEvent event;

			const float3 bsdfSample = BSDF_Sample(bsdf,
					Sampler_GetSamplePathVertex(depth, IDX_BSDF_X),
					Sampler_GetSamplePathVertex(depth, IDX_BSDF_Y),
					&sampledDir, &lastPdfW, &cosSampledDir, &event
					MATERIALS_PARAM);
			const bool lastSpecular = ((event & SPECULAR) != 0);

			// Russian Roulette
			const float rrProb = RussianRouletteProb(bsdfSample);
			const bool rrEnabled = (depth >= PARAM_RR_DEPTH) && !lastSpecular;
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
				task->directLightState.lastPdfW = lastPdfW;
				task->directLightState.lastSpecular = lastSpecular;
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
		Sampler_NextSample(sample, sampleData, seed, frameBuffer
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				, alphaFrameBuffer
#endif
				);
		taskStats[gid].sampleCount += 1;

		GenerateCameraPath(task, sampleData, camera, ray, seed);
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
