#line 2 "biaspatchocl_kernels_micro.cl"

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
//  PARAM_TASK_COUNT
//  PARAM_TILE_SIZE
//  PARAM_FIRST_VERTEX_DL_COUNT
//  PARAM_RADIANCE_CLAMP_MAXVALUE
//  PARAM_PDF_CLAMP_VALUE
//  PARAM_AA_SAMPLES
//  PARAM_DIRECT_LIGHT_SAMPLES
//  PARAM_DIFFUSE_SAMPLES
//  PARAM_GLOSSY_SAMPLES
//  PARAM_SPECULAR_SAMPLES
//  PARAM_DEPTH_MAX
//  PARAM_DEPTH_DIFFUSE_MAX
//  PARAM_DEPTH_GLOSSY_MAX
//  PARAM_DEPTH_SPECULAR_MAX
//  PARAM_IMAGE_FILTER_WIDTH
//  PARAM_LOW_LIGHT_THREASHOLD
//  PARAM_NEAR_START_LIGHT

//------------------------------------------------------------------------------
// RenderSample (Micro-Kernels)
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// RenderSample_MK_GENERATE_CAMERA_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_GENERATE_CAMERA_RAY(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	__global GPUTask *task = &tasks[gid];

	const uint sampleIndex = gid % (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelIndex = gid / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelX = samplePixelIndex % PARAM_TILE_SIZE;
	const uint samplePixelY = samplePixelIndex / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES) ||
			(tileStartX + samplePixelX >= engineFilmWidth) ||
			(tileStartY + samplePixelY >= engineFilmHeight)) {
		task->pathState = MK_DONE;
		return;
	}

	//--------------------------------------------------------------------------
	// Initialize the eye ray
	//--------------------------------------------------------------------------

	// Read the seed
	Seed seed;
	seed.s1 = task->seed.s1;
	seed.s2 = task->seed.s2;
	seed.s3 = task->seed.s3;

#if defined(PARAM_HAS_VOLUMES)
	// Initialize the volume information
	PathVolumeInfo_Init(&task->volInfoPathVertex1);
#endif
	
	__global SampleResult *sampleResult = &taskResults[gid];
	SampleResult_Init(sampleResult);
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sampleResult->directShadowMask = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = 1.f;
#endif

	Ray ray;
	GenerateCameraRay(&seed, task, sampleResult,
			camera, pixelFilterDistribution,
			samplePixelX, samplePixelY, sampleIndex,
			tileStartX, tileStartY,
			engineFilmWidth, engineFilmHeight, &ray);

	task->currentTime = ray.time;
	VSTORE3F(WHITE, task->throughputPathVertex1.c);
	task->tmpRay = ray;

	task->pathState = MK_TRACE_EYE_RAY;

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// RenderSample_MK_TRACE_EYE_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_TRACE_EYE_RAY(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	if (task->pathState != MK_TRACE_EYE_RAY)
		return;

	__global GPUTaskStats *taskStat = &taskStats[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	// Read the seed
	Seed seed;
	seed.s1 = task->seed.s1;
	seed.s2 = task->seed.s2;
	seed.s3 = task->seed.s3;

	float3 throughputPathVertex1 = VLOAD3F(task->throughputPathVertex1.c);
	
	__global SampleResult *sampleResult = &taskResults[gid];

	//--------------------------------------------------------------------------
	// Trace the ray
	//--------------------------------------------------------------------------

	Ray ray = task->tmpRay;
	RayHit rayHit;

	taskStat->raysCount += BIASPATHOCL_Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
		&task->volInfoPathVertex1,
		&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
		Rnd_FloatValue(&seed),
#endif
		&ray, &rayHit,
		&task->bsdfPathVertex1,
		&throughputPathVertex1,
		sampleResult,
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
		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM);

	VSTORE3F(throughputPathVertex1, task->throughputPathVertex1.c);
	task->tmpRayHit = rayHit;

	task->pathState = MK_ILLUMINATE_EYE_MISS;

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// RenderSample_MK_ILLUMINATE_EYE_MISS
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_ILLUMINATE_EYE_MISS(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	if (task->pathState != MK_ILLUMINATE_EYE_MISS)
		return;

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	__global SampleResult *sampleResult = &taskResults[gid];

	//--------------------------------------------------------------------------
	// Render the eye ray miss
	//--------------------------------------------------------------------------

	RayHit rayHit = task->tmpRayHit;
	if (rayHit.meshIndex == NULL_INDEX) {
		// Nothing was hit

#if defined(PARAM_HAS_ENVLIGHTS)
		const Ray ray = task->tmpRay;

		// Add environmental lights radiance
		const float3 rayDir = (float3)(ray.d.x, ray.d.y, ray.d.z);
		// SPECULAR is required to avoid MIS
		DirectHitInfiniteLight(
				SPECULAR,
				VLOAD3F(task->throughputPathVertex1.c),
				-rayDir, 1.f,
				sampleResult
				LIGHTS_PARAM);
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		sampleResult->alpha = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		sampleResult->depth = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		sampleResult->position.x = INFINITY;
		sampleResult->position.y = INFINITY;
		sampleResult->position.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		sampleResult->geometryNormal.x = INFINITY;
		sampleResult->geometryNormal.y = INFINITY;
		sampleResult->geometryNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		sampleResult->shadingNormal.x = INFINITY;
		sampleResult->shadingNormal.y = INFINITY;
		sampleResult->shadingNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		sampleResult->materialID = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		sampleResult->uv.u = INFINITY;
		sampleResult->uv.v = INFINITY;
#endif

		task->pathState = MK_DONE;
	}
}
