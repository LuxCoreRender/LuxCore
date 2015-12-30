#line 2 "biaspatchocl_kernels_micro.cl"

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
//  PARAM_FIRST_VERTEX_DL_COUNT
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
//
// Used for RTBIASPATHOCL:
//  PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION
//  PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP
//  PARAM_RTBIASPATHOCL_PREVIEW_DL_ONLY
//  PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION

//------------------------------------------------------------------------------
// RenderSample (Micro-Kernels)
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// RenderSample_MK_GENERATE_CAMERA_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_GENERATE_CAMERA_RAY(
		const uint pass,
		const uint tileStartX, const uint tileStartY,
		const uint tileWidth, const uint tileHeight,
		const uint tileTotalWidth, const uint tileTotalHeight,
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	__global GPUTask *task = &tasks[gid];

#if defined(RENDER_ENGINE_RTBIASPATHOCL)
	// RTBIASPATHOCL renders first passes at a lower resolution
	// (PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION x PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION)
	// than render one sample every
	// (PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION x PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION)

	const uint previewResolutionReduction = RT_PreviewResolutionReduction(pass);

	const uint sampleIndex = 0;
	uint samplePixelX, samplePixelY;
	if (previewResolutionReduction > PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION) {
		const uint samplePixelIndex = gid * previewResolutionReduction;
		samplePixelX = samplePixelIndex % tileTotalWidth;
		samplePixelY = samplePixelIndex / tileTotalWidth * previewResolutionReduction;
	} else
		RT_ResolutionReduction(pass, gid, tileTotalWidth, tileTotalHeight, &samplePixelX, &samplePixelY);
#else
	// Normal BIASPATHOCL
	const uint sampleIndex = gid % (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelIndex = gid / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelX = samplePixelIndex % tileTotalWidth;
	const uint samplePixelY = samplePixelIndex / tileTotalWidth;
#endif

	if ((gid >= tileTotalWidth * tileTotalHeight * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES) ||
			(samplePixelX >= tileWidth) ||
			(samplePixelY >= tileHeight) ||
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

	float3 connectionThroughput;
	taskStats[gid].raysCount += BIASPATHOCL_Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
		&task->volInfoPathVertex1,
		&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
		Rnd_FloatValue(&seed),
#endif
		&ray, &rayHit,
		&task->bsdfPathVertex1,
		&connectionThroughput, throughputPathVertex1,
		sampleResult,
		// BSDF_Init parameters
		meshDescs,
		sceneObjs,
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
	throughputPathVertex1 *= connectionThroughput;

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

	const RayHit rayHit = task->tmpRayHit;
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
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
		sampleResult->objectID = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		sampleResult->uv.u = INFINITY;
		sampleResult->uv.v = INFINITY;
#endif

		task->pathState = MK_DONE;
	} else
		task->pathState = MK_ILLUMINATE_EYE_HIT;
}

//------------------------------------------------------------------------------
// RenderSample_MK_ILLUMINATE_EYE_HIT
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_ILLUMINATE_EYE_HIT(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	if (task->pathState != MK_ILLUMINATE_EYE_HIT)
		return;

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	__global SampleResult *sampleResult = &taskResults[gid];

	//----------------------------------------------------------------------
	// Something was hit
	//----------------------------------------------------------------------

	const float rayHitT = task->tmpRayHit.t;

	// Save the path first vertex information
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	sampleResult->alpha = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	sampleResult->depth = rayHitT;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
	sampleResult->position = task->bsdfPathVertex1.hitPoint.p;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
	sampleResult->geometryNormal = task->bsdfPathVertex1.hitPoint.geometryN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
	sampleResult->shadingNormal = task->bsdfPathVertex1.hitPoint.shadeN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
	sampleResult->materialID = BSDF_GetMaterialID(&task->bsdfPathVertex1
				MATERIALS_PARAM);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
	sampleResult->objectID = BSDF_GetObjectID(&task->bsdfPathVertex1, sceneObjs);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
	sampleResult->uv = task->bsdfPathVertex1.hitPoint.uv;
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	// Check if it is a light source (note: I can hit only triangle area light sources)
	if (BSDF_IsLightSource(&task->bsdfPathVertex1) && (rayHitT > PARAM_NEAR_START_LIGHT)) {
		// SPECULAR is required to avoid MIS
		DirectHitFiniteLight(SPECULAR,
				VLOAD3F(task->throughputPathVertex1.c),
				rayHitT, &task->bsdfPathVertex1, 1.f,
				sampleResult
				LIGHTS_PARAM);
	}
#endif

	task->pathState = MK_DL_VERTEX_1;
}

//------------------------------------------------------------------------------
// RenderSample_MK_DL_VERTEX_1
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_DL_VERTEX_1(
		const uint pass,
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	if (task->pathState != MK_DL_VERTEX_1)
		return;

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	__global SampleResult *sampleResult = &taskResults[gid];

	//----------------------------------------------------------------------
	// First path vertex direct light sampling
	//----------------------------------------------------------------------

	const BSDFEvent materialEventTypes = BSDF_GetEventTypes(&task->bsdfPathVertex1
		MATERIALS_PARAM);
#if defined(RENDER_ENGINE_RTBIASPATHOCL) && defined(PARAM_RTBIASPATHOCL_PREVIEW_DL_ONLY)
	// RTBIASPATHOCL can render first passes with direct light only

	const uint previewResolutionReduction = RT_PreviewResolutionReduction(pass);

	if (previewResolutionReduction > PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION)
		sampleResult->lastPathVertex = true;
	else
#endif
		sampleResult->lastPathVertex = 
				(PARAM_DEPTH_MAX <= 1) ||
				((PARAM_DEPTH_DIFFUSE_MAX <= 1) && (materialEventTypes & DIFFUSE)) ||
				((PARAM_DEPTH_GLOSSY_MAX <= 1) && (materialEventTypes & GLOSSY)) ||
				((PARAM_DEPTH_SPECULAR_MAX <= 1) && (materialEventTypes & SPECULAR));
	task->materialEventTypesPathVertex1 = materialEventTypes;

	// Only if it is not a SPECULAR BSDF
	if (!BSDF_IsDelta(&task->bsdfPathVertex1
			MATERIALS_PARAM)) {
		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;

		__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];

#if defined(PARAM_HAS_VOLUMES)
		// I need to work on a copy of volume information of the first path vertex
		taskDirectLight->directLightVolInfo = task->volInfoPathVertex1;
#endif

		taskStats[gid].raysCount +=
			DirectLightSampling_ALL(
			&seed,
#if defined(PARAM_HAS_VOLUMES)
			&taskDirectLight->directLightVolInfo,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
			&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
			task->currentTime,
			VLOAD3F(task->throughputPathVertex1.c),
			&task->bsdfPathVertex1, &taskDirectLight->directLightBSDF,
			sampleResult,
			// BSDF_Init parameters
			meshDescs,
			sceneObjs,
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
			// Accelerator_Intersect parameters
			ACCELERATOR_INTERSECT_PARAM
			// Light related parameters
			LIGHTS_PARAM);

		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;
	}

	sampleResult->firstPathVertex = false;
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = 0.f;
#endif

#if defined(RENDER_ENGINE_RTBIASPATHOCL) && defined(PARAM_RTBIASPATHOCL_PREVIEW_DL_ONLY)
	// RTBIASPATHOCL renders first passes at a lower resolution and (optionally)
	// with direct light only
	if (previewResolutionReduction > PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION)
		task->pathState = MK_DONE;
	else
#endif
		task->pathState = MK_BSDF_SAMPLE;
}

//------------------------------------------------------------------------------
// RenderSample_MK_BSDF_SAMPLE
//------------------------------------------------------------------------------

void RenderSample_MK_BSDF_SAMPLE(
		const BSDFEvent vertex1SampleComponent,
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	__global SampleResult *sampleResult = &taskResults[gid];
	if ((task->pathState != MK_BSDF_SAMPLE) ||
			(sampleResult->lastPathVertex) ||
			!(task->materialEventTypesPathVertex1 & vertex1SampleComponent) ||
			!PathDepthInfo_CheckComponentDepths(vertex1SampleComponent))
		return;

	// Read the seed
	Seed seed;
	seed.s1 = task->seed.s1;
	seed.s2 = task->seed.s2;
	seed.s3 = task->seed.s3;

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global GPUTaskPathVertexN *taskPathVertexN = &tasksPathVertexN[gid];

	//----------------------------------------------------------------------
	// Sample the components of the BSDF of the first path vertex
	//----------------------------------------------------------------------

	const int matSamplesCount = mats[task->bsdfPathVertex1.materialIndex].samples;
	const uint globalMatSamplesCount = ((vertex1SampleComponent == DIFFUSE) ? PARAM_DIFFUSE_SAMPLES :
		((vertex1SampleComponent == GLOSSY) ? PARAM_GLOSSY_SAMPLES :
			PARAM_SPECULAR_SAMPLES));
	const uint sampleCount = (matSamplesCount < 0) ? globalMatSamplesCount : (uint)matSamplesCount;

	taskStats[gid].raysCount += SampleComponent(
			&seed,
#if defined(PARAM_HAS_VOLUMES)
			&task->volInfoPathVertex1,
			&taskPathVertexN->volInfoPathVertexN,
			&taskDirectLight->directLightVolInfo,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
			&task->tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
			task->currentTime,
			vertex1SampleComponent,
			sampleCount, VLOAD3F(task->throughputPathVertex1.c),
			&task->bsdfPathVertex1, &taskPathVertexN->bsdfPathVertexN,
			&taskDirectLight->directLightBSDF,
			sampleResult,
			// BSDF_Init parameters
			meshDescs,
			sceneObjs,
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
			// Accelerator_Intersect parameters
			ACCELERATOR_INTERSECT_PARAM
			// Light related parameters
			LIGHTS_PARAM);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_BSDF_SAMPLE_DIFFUSE(
		KERNEL_ARGS
		) {
	RenderSample_MK_BSDF_SAMPLE(
			DIFFUSE,
			engineFilmWidth,engineFilmHeight,
			tasks,
			tasksDirectLight,
			tasksPathVertexN,
			taskStats,
			taskResults,
			pixelFilterDistribution,
			// Film parameters
			filmWidth, filmHeight,
			0, filmWidth - 1,
			0, filmHeight - 1
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
			, filmRadianceGroup0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
			, filmRadianceGroup1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
			, filmRadianceGroup2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
			, filmRadianceGroup3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
			, filmRadianceGroup4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
			, filmRadianceGroup5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
			, filmRadianceGroup6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
			, filmRadianceGroup7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
			, filmAlpha
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
			, filmDepth
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
			, filmPosition
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
			, filmGeometryNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
			, filmShadingNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
			, filmMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
			, filmDirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
			, filmDirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
			, filmEmission
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
			, filmIndirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
			, filmIndirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
			, filmIndirectSpecular
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
			, filmMaterialIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
			, filmDirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
			, filmIndirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
			, filmUV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
			, filmRayCount
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
			, filmByMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
			, filmIrradiance
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
			, filmObjectID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
			, filmObjectIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
			, filmByObjectID
#endif
			,
			// Scene parameters
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX,
			worldCenterY,
			worldCenterZ,
			worldRadius,
#endif
			mats,
			texs,
			sceneObjs,
			meshDescs,
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
			triangles,
			camera,
			// Lights
			lights,
#if defined(PARAM_HAS_ENVLIGHTS)
			envLightIndices,
			envLightCount,
#endif
			meshTriLightDefsOffset,
#if defined(PARAM_HAS_INFINITELIGHT)
			infiniteLightDistribution,
#endif
			lightsDistribution
			// Images
#if defined(PARAM_IMAGEMAPS_PAGE_0)
			, 	imageMapDescs, 	imageMapBuff0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
			imageMapBuff1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
			, imageMapBuff2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
			, imageMapBuff3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
			, imageMapBuff4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
			, imageMapBuff5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
			, imageMapBuff6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
			, imageMapBuff7
#endif
			// Ray intersection accelerator parameters
			ACCELERATOR_INTERSECT_PARAM
			);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_BSDF_SAMPLE_GLOSSY(
		KERNEL_ARGS
		) {
	RenderSample_MK_BSDF_SAMPLE(
			GLOSSY,
			engineFilmWidth,engineFilmHeight,
			tasks,
			tasksDirectLight,
			tasksPathVertexN,
			taskStats,
			taskResults,
			pixelFilterDistribution,
			// Film parameters
			filmWidth, filmHeight,
			0, filmWidth - 1,
			0, filmHeight - 1
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
			, filmRadianceGroup0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
			, filmRadianceGroup1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
			, filmRadianceGroup2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
			, filmRadianceGroup3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
			, filmRadianceGroup4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
			, filmRadianceGroup5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
			, filmRadianceGroup6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
			, filmRadianceGroup7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
			, filmAlpha
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
			, filmDepth
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
			, filmPosition
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
			, filmGeometryNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
			, filmShadingNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
			, filmMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
			, filmDirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
			, filmDirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
			, filmEmission
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
			, filmIndirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
			, filmIndirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
			, filmIndirectSpecular
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
			, filmMaterialIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
			, filmDirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
			, filmIndirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
			, filmUV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
			, filmRayCount
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
			, filmByMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
			, filmIrradiance
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
			, filmObjectID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
			, filmObjectIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
			, filmByObjectID
#endif
			,
			// Scene parameters
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX,
			worldCenterY,
			worldCenterZ,
			worldRadius,
#endif
			mats,
			texs,
			sceneObjs,
			meshDescs,
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
			triangles,
			camera,
			// Lights
			lights,
#if defined(PARAM_HAS_ENVLIGHTS)
			envLightIndices,
			envLightCount,
#endif
			meshTriLightDefsOffset,
#if defined(PARAM_HAS_INFINITELIGHT)
			infiniteLightDistribution,
#endif
			lightsDistribution
			// Images
#if defined(PARAM_IMAGEMAPS_PAGE_0)
			, 	imageMapDescs, 	imageMapBuff0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
			imageMapBuff1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
			, imageMapBuff2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
			, imageMapBuff3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
			, imageMapBuff4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
			, imageMapBuff5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
			, imageMapBuff6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
			, imageMapBuff7
#endif
			// Ray intersection accelerator parameters
			ACCELERATOR_INTERSECT_PARAM
			);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample_MK_BSDF_SAMPLE_SPECULAR(
		KERNEL_ARGS
		) {
	RenderSample_MK_BSDF_SAMPLE(
			SPECULAR,
			engineFilmWidth,engineFilmHeight,
			tasks,
			tasksDirectLight,
			tasksPathVertexN,
			taskStats,
			taskResults,
			pixelFilterDistribution,
			// Film parameters
			filmWidth, filmHeight,
			0, filmWidth - 1,
			0, filmHeight - 1
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
			, filmRadianceGroup0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
			, filmRadianceGroup1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
			, filmRadianceGroup2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
			, filmRadianceGroup3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
			, filmRadianceGroup4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
			, filmRadianceGroup5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
			, filmRadianceGroup6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
			, filmRadianceGroup7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
			, filmAlpha
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
			, filmDepth
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
			, filmPosition
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
			, filmGeometryNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
			, filmShadingNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
			, filmMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
			, filmDirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
			, filmDirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
			, filmEmission
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
			, filmIndirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
			, filmIndirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
			, filmIndirectSpecular
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
			, filmMaterialIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
			, filmDirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
			, filmIndirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
			, filmUV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
			, filmRayCount
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
			, filmByMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
			, filmIrradiance
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
			, filmObjectID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
			, filmObjectIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
			, filmByObjectID
#endif
			,
			// Scene parameters
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX,
			worldCenterY,
			worldCenterZ,
			worldRadius,
#endif
			mats,
			texs,
			sceneObjs,
			meshDescs,
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
			triangles,
			camera,
			// Lights
			lights,
#if defined(PARAM_HAS_ENVLIGHTS)
			envLightIndices,
			envLightCount,
#endif
			meshTriLightDefsOffset,
#if defined(PARAM_HAS_INFINITELIGHT)
			infiniteLightDistribution,
#endif
			lightsDistribution
			// Images
#if defined(PARAM_IMAGEMAPS_PAGE_0)
			, 	imageMapDescs, 	imageMapBuff0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
			imageMapBuff1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
			, imageMapBuff2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
			, imageMapBuff3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
			, imageMapBuff4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
			, imageMapBuff5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
			, imageMapBuff6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
			, imageMapBuff7
#endif
			// Ray intersection accelerator parameters
			ACCELERATOR_INTERSECT_PARAM
			);
	
	// All done
	tasks[get_global_id(0)].pathState = MK_DONE;
}
