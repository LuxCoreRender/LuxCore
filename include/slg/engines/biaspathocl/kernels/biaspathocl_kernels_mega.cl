#line 2 "biaspatchocl_kernels_mega.cl"

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
// RenderSample (Mega-Kernel)
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample(
		const uint tileStartX,
		const uint tileStartY,
		KERNEL_ARGS
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

	INIT_IMAGEMAPS_PAGES

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

	taskStat->raysCount = tracedRaysCount;

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}
