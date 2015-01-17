#line 2 "biaspatchocl_kernels.cl"

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
// InitSeed Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitSeed(
		uint seedBase,
		__global GPUTask *tasks) {
	//if (get_global_id(0) == 0)
	//	printf("sizeof(BSDF) = %dbytes\n", sizeof(BSDF));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(HitPoint) = %dbytes\n", sizeof(HitPoint));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(GPUTask) = %dbytes\n", sizeof(GPUTask));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(SampleResult) = %dbytes\n", sizeof(SampleResult));

	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	// For testing/debugging
	//MangleMemory((__global unsigned char *)task, sizeof(GPUTask));

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// InitStats Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitStat(
		__global GPUTaskStats *taskStats) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->raysCount = 0;
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample(
		const uint tileStartX,
		const uint tileStartY,
		const int tileSampleIndex,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global GPUTask *tasks,
		__global GPUTaskDirectLight *tasksDirectLight,
		__global GPUTaskPathVertexN *tasksPathVertexN,
		__global GPUTaskStats *taskStats,
		__global SampleResult *taskResults,
		__global float *pixelFilterDistribution,
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
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
		, __global float *filmByMaterialID
#endif
		,
		// Scene parameters
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
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
		// Lights
		__global LightSource *lights,
#if defined(PARAM_HAS_ENVLIGHTS)
		__global uint *envLightIndices,
		const uint envLightCount,
#endif
		__global uint *meshTriLightDefsOffset,
#if defined(PARAM_HAS_INFINITELIGHT)
		__global float *infiniteLightDistribution,
#endif
		__global float *lightsDistribution
		// Images
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
		// Ray intersection accelerator parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	const size_t gid = get_global_id(0);

	const uint sampleIndex = gid % (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelIndex = gid / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelX = samplePixelIndex % PARAM_TILE_SIZE;
	const uint samplePixelY = samplePixelIndex / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES) ||
			(tileStartX + samplePixelX >= engineFilmWidth) ||
			(tileStartY + samplePixelY >= engineFilmHeight))
		return;

	__global GPUTask *task = &tasks[gid];
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global GPUTaskPathVertexN *taskPathVertexN = &tasksPathVertexN[gid];
	__global GPUTaskStats *taskStat = &taskStats[gid];

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

	uint tracedRaysCount = taskStat->raysCount;
	float3 throughputPathVertex1 = WHITE;
	
	__global SampleResult *sampleResult = &taskResults[gid];
	SampleResult_Init(sampleResult);
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sampleResult->directShadowMask = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = 1.f;
#endif

	Ray ray;
	RayHit rayHit;
	GenerateCameraRay(&seed, task, sampleResult,
			camera, pixelFilterDistribution,
			samplePixelX, samplePixelY, sampleIndex,
			tileStartX, tileStartY,
			engineFilmWidth, engineFilmHeight, &ray);
	const float time = ray.time;

	//--------------------------------------------------------------------------
	// Trace the eye ray
	//--------------------------------------------------------------------------

	tracedRaysCount += BIASPATHOCL_Scene_Intersect(
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

	//--------------------------------------------------------------------------
	// Render the eye ray hit point
	//--------------------------------------------------------------------------

	if (rayHit.meshIndex != NULL_INDEX) {
		//----------------------------------------------------------------------
		// Something was hit
		//----------------------------------------------------------------------

		// Save the path first vertex information
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		sampleResult->alpha = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		sampleResult->depth = rayHit.t;
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
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		sampleResult->uv = task->bsdfPathVertex1.hitPoint.uv;
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		// Check if it is a light source (note: I can hit only triangle area light sources)
		if (BSDF_IsLightSource(&task->bsdfPathVertex1) && (rayHit.t > PARAM_NEAR_START_LIGHT)) {
			// SPECULAR is required to avoid MIS
			DirectHitFiniteLight(SPECULAR,
					throughputPathVertex1,
					rayHit.t, &task->bsdfPathVertex1, 1.f,
					sampleResult
					LIGHTS_PARAM);
		}
#endif

		//----------------------------------------------------------------------
		// First path vertex direct light sampling
		//----------------------------------------------------------------------

		const BSDFEvent materialEventTypes = BSDF_GetEventTypes(&task->bsdfPathVertex1
			MATERIALS_PARAM);
		sampleResult->lastPathVertex = 
				(PARAM_DEPTH_MAX <= 1) ||
				(!((PARAM_DEPTH_DIFFUSE_MAX > 0) && (materialEventTypes & DIFFUSE)) &&
				!((PARAM_DEPTH_GLOSSY_MAX > 0) && (materialEventTypes & GLOSSY)) &&
				!((PARAM_DEPTH_SPECULAR_MAX > 0) && (materialEventTypes & SPECULAR)));

		// Only if it is not a SPECULAR BSDF
		if (!BSDF_IsDelta(&task->bsdfPathVertex1
				MATERIALS_PARAM)) {
#if defined(PARAM_HAS_VOLUMES)
			// I need to work on a copy of volume information of the first path vertex
			taskDirectLight->directLightVolInfo = task->volInfoPathVertex1;
#endif

			tracedRaysCount +=
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
				time,
				throughputPathVertex1,
				&task->bsdfPathVertex1, &taskDirectLight->directLightBSDF,
				sampleResult,
				// BSDF_Init parameters
				meshDescs,
				meshMats,
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
		}
		
		//----------------------------------------------------------------------
		// Sample the components of the BSDF of the first path vertex
		//----------------------------------------------------------------------

#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		sampleResult->indirectShadowMask = 0.f;
#endif

		if (!sampleResult->lastPathVertex) {
			sampleResult->firstPathVertex = false;

			BSDFEvent vertex1SampleComponent = DIFFUSE;
			do {
				if (PathDepthInfo_CheckComponentDepths(vertex1SampleComponent) && (materialEventTypes & vertex1SampleComponent)) {
					const int matSamplesCount = mats[task->bsdfPathVertex1.materialIndex].samples;
					const uint globalMatSamplesCount = ((vertex1SampleComponent == DIFFUSE) ? PARAM_DIFFUSE_SAMPLES :
						((vertex1SampleComponent == GLOSSY) ? PARAM_GLOSSY_SAMPLES :
							PARAM_SPECULAR_SAMPLES));
					const uint sampleCount = (matSamplesCount < 0) ? globalMatSamplesCount : (uint)matSamplesCount;

					tracedRaysCount += SampleComponent(
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
							time,
							vertex1SampleComponent,
							sampleCount, throughputPathVertex1,
							&task->bsdfPathVertex1, &taskPathVertexN->bsdfPathVertexN,
							&taskDirectLight->directLightBSDF,
							sampleResult,
							// BSDF_Init parameters
							meshDescs,
							meshMats,
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
				}

				// Move to the next BSDF component
				vertex1SampleComponent = (vertex1SampleComponent == DIFFUSE) ? GLOSSY :
					((vertex1SampleComponent == GLOSSY) ? SPECULAR : NONE);
			} while (vertex1SampleComponent != NONE);
		}
	} else {
		//----------------------------------------------------------------------
		// Nothing was hit
		//----------------------------------------------------------------------

#if defined(PARAM_HAS_ENVLIGHTS)
		// Add environmental lights radiance
		const float3 rayDir = (float3)(ray.d.x, ray.d.y, ray.d.z);
		// SPECULAR is required to avoid MIS
		DirectHitInfiniteLight(
				SPECULAR,
				throughputPathVertex1,
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
	}

	//--------------------------------------------------------------------------
	// Save the result
	//--------------------------------------------------------------------------

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	sampleResult->rayCount = tracedRaysCount;
#endif

	if (PARAM_RADIANCE_CLAMP_MAXVALUE > 0.f) {
		// Radiance clamping
		SR_RadianceClamp(sampleResult);
	}

	taskStat->raysCount = tracedRaysCount;

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// MergePixelSamples
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergePixelSamples(
		const uint tileStartX,
		const uint tileStartY,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global SampleResult *taskResults,
		__global GPUTaskStats *taskStats,
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
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
		, __global float *filmByMaterialID
#endif
		) {
	const size_t gid = get_global_id(0);

	uint sampleX, sampleY;
	sampleX = gid % PARAM_TILE_SIZE;
	sampleY = gid / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE) ||
			(tileStartX + sampleX >= engineFilmWidth) ||
			(tileStartY + sampleY >= engineFilmHeight))
		return;

	const uint index = gid * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES;
	__global SampleResult *sampleResult = &taskResults[index];

	//--------------------------------------------------------------------------
	// Initialize Film radiance group pointer table
	//--------------------------------------------------------------------------

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
	filmRadianceGroup[4] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroup[5] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroup[6] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	filmRadianceGroup[7] = filmRadianceGroup7;
#endif

	//--------------------------------------------------------------------------
	// Merge all samples and accumulate statistics
	//--------------------------------------------------------------------------

#if (PARAM_AA_SAMPLES == 1)
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);
#else
	__global GPUTaskStats *taskStat = &taskStats[index];

	SampleResult result = sampleResult[0];
	uint totalRaysCount = 0;
	for (uint i = 1; i < PARAM_AA_SAMPLES * PARAM_AA_SAMPLES; ++i) {
		SR_Accumulate(&sampleResult[i], &result);
		totalRaysCount += taskStat[i].raysCount;
	}
	SR_Normalize(&result, 1.f / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES));

	// I have to save result in __global space in order to be able
	// to use Film_AddSample(). OpenCL can be so stupid some time...
	sampleResult[0] = result;
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);

	taskStat[0].raysCount = totalRaysCount;
#endif
}
