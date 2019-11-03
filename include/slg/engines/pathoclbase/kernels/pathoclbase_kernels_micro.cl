#line 2 "pathoclbase_kernels_micro.cl"

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

//------------------------------------------------------------------------------
// AdvancePaths (Micro-Kernels)
//------------------------------------------------------------------------------

//#define DEBUG_PRINTF_KERNEL_NAME 1

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

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	// This has to be done by the first kernel to run after RT kernel
	samples[gid].result.rayCount += 1;
#endif

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_RT_NEXT_VERTEX(state = %d)\n", pathState);
#endif
	if (pathState != MK_RT_NEXT_VERTEX)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------
	
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	float3 connectionThroughput;

	Seed seedPassThroughEvent = taskState->seedPassThroughEvent;
	const float passThroughEvent = Rnd_FloatValue(&seedPassThroughEvent);
	taskState->seedPassThroughEvent = seedPassThroughEvent;

	int throughShadowTransparency = taskState->throughShadowTransparency;
	const bool continueToTrace = Scene_Intersect(
			EYE_RAY | ((pathInfo->depth.depth == 0) ? CAMERA_RAY : GENERIC_RAY),
			&throughShadowTransparency,
#if defined(PARAM_HAS_VOLUMES)
			&pathInfo->volume,
			&tasks[gid].tmpHitPoint,
#endif
			passThroughEvent,
			&rays[gid], &rayHits[gid], &taskState->bsdf,
			&connectionThroughput, VLOAD3F(taskState->throughput.c),
			&samples[gid].result,
			false
			MATERIALS_PARAM
			);
	taskState->throughShadowTransparency = throughShadowTransparency;
	VSTORE3F(connectionThroughput * VLOAD3F(taskState->throughput.c), taskState->throughput.c);

	// If continueToTrace, there is nothing to do, just keep the same state
	if (!continueToTrace) {
		if (rayHits[gid].meshIndex == NULL_INDEX)
			taskState->state = MK_HIT_NOTHING;
		else {
			__global Sample *sample = &samples[gid];
			const BSDFEvent eventTypes = BSDF_GetEventTypes(&taskState->bsdf
					MATERIALS_PARAM);
			sample->result.lastPathVertex = PathDepthInfo_IsLastPathVertex(&pathInfo->depth, eventTypes);

			taskState->state = MK_HIT_OBJECT;
		}
	}
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

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_HIT_NOTHING(state = %d)\n", pathState);
#endif
	if (pathState != MK_HIT_NOTHING)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global Sample *sample = &samples[gid];
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Nothing was hit, add environmental lights radiance

#if defined(PARAM_HAS_ENVLIGHTS)
	bool checkDirectLightHit = true;
	
#if defined(PARAM_FORCE_BLACK_BACKGROUND)
	checkDirectLightHit = checkDirectLightHit && (!pathInfo->isPassThroughPath);
#endif

#if defined(PARAM_HYBRID_BACKFORWARD)
	checkDirectLightHit = checkDirectLightHit &&
			// Avoid to render caustic path if hybridBackForwardEnable
			!pathInfo->isNearlyCaustic;
#endif

#if defined(PARAM_PGIC_ENABLED)
	checkDirectLightHit = checkDirectLightHit &&
			PhotonGICache_IsDirectLightHitVisible(pathInfo, taskState->photonGICausticCacheUsed);
#endif

	if (checkDirectLightHit) {
		DirectHitInfiniteLight(
				pathInfo,
				&taskState->throughput,
				&rays[gid],
				sample->result.firstPathVertex ? NULL : &taskState->bsdf,
				&samples[gid].result
				LIGHTS_PARAM);
	}
#endif

	if (pathInfo->depth.depth == 0) {
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
		sample->result.geometryNormal.x = 0.f;
		sample->result.geometryNormal.y = 0.f;
		sample->result.geometryNormal.z = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL) || defined(PARAM_FILM_CHANNELS_HAS_AVG_SHADING_NORMAL)
		sample->result.shadingNormal.x = 0.f;
		sample->result.shadingNormal.y = 0.f;
		sample->result.shadingNormal.z = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR)
		sample->result.materialID = 0;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
		sample->result.objectID = 0;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		sample->result.uv.u = INFINITY;
		sample->result.uv.v = INFINITY;
#endif
	}

	taskState->state = MK_SPLAT_SAMPLE;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_HIT_OBJECT
// To: MK_DL_ILLUMINATE or MK_SPLAT_SAMPLE
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_HIT_OBJECT(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_HIT_OBJECT(state = %d)\n", pathState);
#endif
	if (pathState != MK_HIT_OBJECT)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global BSDF *bsdf = &taskState->bsdf;
	__global Sample *sample = &samples[gid];
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Something was hit

	if (taskState->albedoToDo && BSDF_IsAlbedoEndPoint(bsdf MATERIALS_PARAM)) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALBEDO)
		const float3 albedo = VLOAD3F(taskState->throughput.c) * BSDF_Albedo(bsdf
				MATERIALS_PARAM);
		VSTORE3F(albedo, sample->result.albedo.c);
#endif
		taskState->albedoToDo = false;
	}

	if (pathInfo->depth.depth == 0) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		sample->result.alpha = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		sample->result.depth = rayHits[gid].t;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		sample->result.position = bsdf->hitPoint.p;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		sample->result.geometryNormal = bsdf->hitPoint.geometryN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL) || defined(PARAM_FILM_CHANNELS_HAS_AVG_SHADING_NORMAL)
		sample->result.shadingNormal = bsdf->hitPoint.shadeN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR)
		sample->result.materialID = BSDF_GetMaterialID(bsdf
				MATERIALS_PARAM);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
		sample->result.objectID = BSDF_GetObjectID(bsdf, sceneObjs);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		sample->result.uv = bsdf->hitPoint.uv;
#endif
	}

	//--------------------------------------------------------------------------
	// Check if it is a light source and I have to add light emission
	//--------------------------------------------------------------------------

	bool checkDirectLightHit = true;

#if defined(PARAM_HYBRID_BACKFORWARD)
	checkDirectLightHit = checkDirectLightHit &&
			// Avoid to render caustic path if hybridBackForwardEnable
			!pathInfo->isNearlyCaustic;
#endif

#if defined(PARAM_PGIC_ENABLED)
	checkDirectLightHit = checkDirectLightHit &&
			PhotonGICache_IsDirectLightHitVisible(pathInfo, taskState->photonGICausticCacheUsed);
#endif

	// Check if it is a light source (note: I can hit only triangle area light sources)
	if (BSDF_IsLightSource(bsdf) && checkDirectLightHit) {
		DirectHitFiniteLight(
				pathInfo,
				&taskState->throughput,
				&rays[gid],
				rayHits[gid].t,
				bsdf,
				&sample->result
				LIGHTS_PARAM);
	}

	//----------------------------------------------------------------------
	// Check if I can use the photon cache
	//----------------------------------------------------------------------

#if defined(PARAM_PGIC_ENABLED)
	const bool isPhotonGIEnabled = PhotonGICache_IsPhotonGIEnabled(bsdf, pgicGlossinessUsageThreshold
			MATERIALS_PARAM);

#if defined(PARAM_PGIC_DEBUG_SHOWINDIRECT)

	if (isPhotonGIEnabled) {
		const float3 radiance = PhotonGICache_GetIndirectRadiance(bsdf,
				pgicRadiancePhotons, pgicRadiancePhotonsBVHNodes,
				pgicIndirectLookUpRadius * pgicIndirectLookUpRadius, pgicIndirectLookUpNormalCosAngle);
		VADD3F(sample->result.radiancePerPixelNormalized[0].c, radiance);
	}
	taskState->state = MK_SPLAT_SAMPLE;
	return;

#elif defined(PARAM_PGIC_DEBUG_SHOWCAUSTIC)

	if (isPhotonGIEnabled) {
		const float3 radiance = PhotonGICache_ConnectWithCausticPaths(bsdf,
				pgicCausticPhotons, pgicCausticPhotonsBVHNodes,
				pgicCausticPhotonTracedCount, pgicCausticLookUpRadius * pgicCausticLookUpRadius,
				pgicCausticLookUpNormalCosAngle
				MATERIALS_PARAM);

		VADD3F(sample->result.radiancePerPixelNormalized[0].c, VLOAD3F(taskState->throughput.c) * radiance);
	}
	taskState->state = MK_SPLAT_SAMPLE;
	return;

#elif defined(PARAM_PGIC_DEBUG_SHOWINDIRECTPATHMIX)

	if (isPhotonGIEnabled) {
		Seed seedPassThroughEvent = taskState->seedPassThroughEvent;
		const float passThroughEvent = Rnd_FloatValue(&seedPassThroughEvent);

		if (taskState->photonGICacheEnabledOnLastHit &&
				(rayHits[gid].t > PhotonGICache_GetIndirectUsageThreshold(
					pathInfo->lastBSDFEvent,
					pathInfo->lastGlossiness,
					// I hope to not introduce strange sample correlations
					// by using passThrough here
					passThroughEvent,
					pgicGlossinessUsageThreshold,
					pgicIndirectUsageThresholdScale,
					pgicIndirectLookUpRadius))) {
			VSTORE3F((float3)(0.f, 0.f, 1.f), sample->result.radiancePerPixelNormalized[0].c);
			taskState->photonGIShowIndirectPathMixUsed = true;

			taskState->state = MK_SPLAT_SAMPLE;
			return;
		}
		
		taskState->photonGICacheEnabledOnLastHit = true;
	} else
		taskState->photonGICacheEnabledOnLastHit = false;

#else

	if (isPhotonGIEnabled) {
#if defined(PARAM_PGIC_CAUSTIC_ENABLED)
#if defined(PARAM_HYBRID_BACKFORWARD)
		if (pathInfo->depth.depth != 0) {
#endif
			const float3 causticRadiance = PhotonGICache_ConnectWithCausticPaths(bsdf,
					pgicCausticPhotons, pgicCausticPhotonsBVHNodes,
					pgicCausticPhotonTracedCount, pgicCausticLookUpRadius * pgicCausticLookUpRadius,
					pgicCausticLookUpNormalCosAngle
					MATERIALS_PARAM);

			if (!Spectrum_IsBlack(causticRadiance)) {
				VADD3F(sample->result.radiancePerPixelNormalized[0].c, VLOAD3F(taskState->throughput.c) * causticRadiance);			
				taskState->photonGICausticCacheUsed = true;
			}
#if defined(PARAM_HYBRID_BACKFORWARD)
		}
#endif
#endif

#if defined(PARAM_PGIC_INDIRECT_ENABLED)
		Seed seedPassThroughEvent = taskState->seedPassThroughEvent;
		const float passThroughEvent = Rnd_FloatValue(&seedPassThroughEvent);

		if (taskState->photonGICacheEnabledOnLastHit &&
				(rayHits[gid].t > PhotonGICache_GetIndirectUsageThreshold(
					pathInfo->lastBSDFEvent,
					pathInfo->lastGlossiness,
					// I hope to not introduce strange sample correlations
					// by using passThrough here
					passThroughEvent,
					pgicGlossinessUsageThreshold,
					pgicIndirectUsageThresholdScale,
					pgicIndirectLookUpRadius))) {
			const float3 radiance = PhotonGICache_GetIndirectRadiance(bsdf,
				pgicRadiancePhotons, pgicRadiancePhotonsBVHNodes,
				pgicIndirectLookUpRadius * pgicIndirectLookUpRadius, pgicIndirectLookUpNormalCosAngle);

			VADD3F(sample->result.radiancePerPixelNormalized[0].c, VLOAD3F(taskState->throughput.c) * radiance);

			// I can terminate the path, all done
			taskState->state = MK_SPLAT_SAMPLE;
			return;
		}
#endif

		taskState->photonGICacheEnabledOnLastHit = true;
	} else
		taskState->photonGICacheEnabledOnLastHit = false;

#endif

#endif

	//----------------------------------------------------------------------
	// Check if this is the last path vertex (but not also the first)
	//
	// I handle as a special case when the path vertex is both the first
	// and the last: I do direct light sampling without MIS.
	taskState->state = (sample->result.lastPathVertex && !sample->result.firstPathVertex) ?
		MK_SPLAT_SAMPLE : MK_DL_ILLUMINATE;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_RT_DL
// To: MK_SPLAT_SAMPLE or MK_GENERATE_NEXT_VERTEX_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_RT_DL(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_RT_DL(state = %d)\n", pathState);
#endif
	if (pathState != MK_RT_DL)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	float3 connectionThroughput = WHITE;

	Seed seedPassThroughEvent = taskDirectLight->seedPassThroughEvent;
	const float passThroughEvent = Rnd_FloatValue(&seedPassThroughEvent);
	taskDirectLight->seedPassThroughEvent = seedPassThroughEvent;

	int throughShadowTransparency = taskDirectLight->throughShadowTransparency;
	const bool continueToTrace =
		Scene_Intersect(
			EYE_RAY | SHADOW_RAY,
			&throughShadowTransparency,
#if defined(PARAM_HAS_VOLUMES)
			&directLightVolInfos[gid],
			&task->tmpHitPoint,
#endif
			passThroughEvent,
			&rays[gid], &rayHits[gid], &task->tmpBsdf,
			&connectionThroughput, WHITE,
			NULL,
			true
			MATERIALS_PARAM
			);
	taskDirectLight->throughShadowTransparency = throughShadowTransparency;
	VSTORE3F(connectionThroughput * VLOAD3F(taskDirectLight->illumInfo.lightRadiance.c), taskDirectLight->illumInfo.lightRadiance.c);
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	VSTORE3F(connectionThroughput * VLOAD3F(taskDirectLight->illumInfo.lightIrradiance.c), taskDirectLight->illumInfo.lightIrradiance.c);
#endif

	const bool rayMiss = (rayHits[gid].meshIndex == NULL_INDEX);

	// If continueToTrace, there is nothing to do, just keep the same state
	if (!continueToTrace) {
		__global Sample *sample = &samples[gid];

		if (rayMiss) {
			// Nothing was hit, the light source is visible

			__global BSDF *bsdf = &taskState->bsdf;

			if (!BSDF_IsShadowCatcher(bsdf MATERIALS_PARAM)) {
				const float3 lightRadiance = VLOAD3F(taskDirectLight->illumInfo.lightRadiance.c);
				SampleResult_AddDirectLight(&sample->result, taskDirectLight->illumInfo.lightID,
						BSDF_GetEventTypes(bsdf
							MATERIALS_PARAM),
						VLOAD3F(taskState->throughput.c), lightRadiance,
						1.f);

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
				// The first path vertex is not handled by AddDirectLight(). This is valid
				// for irradiance AOV only if it is not a SPECULAR material.
				//
				// Note: irradiance samples the light sources only here (i.e. no
				// direct hit, no MIS, it would be useless)
				if ((sample->result.firstPathVertex) && !(BSDF_GetEventTypes(bsdf
							MATERIALS_PARAM) & SPECULAR)) {
					const float3 irradiance = (M_1_PI_F * fabs(dot(
								VLOAD3F(&bsdf->hitPoint.shadeN.x),
								VLOAD3F(&rays[gid].d.x)))) *
							VLOAD3F(taskDirectLight->illumInfo.lightIrradiance.c);
					VSTORE3F(irradiance, sample->result.irradiance.c);
				}
#endif
			}

			taskDirectLight->directLightResult = ILLUMINATED;
		} else
			taskDirectLight->directLightResult = SHADOWED;

		// Check if this is the last path vertex
		if (sample->result.lastPathVertex)
			pathState = MK_SPLAT_SAMPLE;
		else
			pathState = MK_GENERATE_NEXT_VERTEX_RAY;

		// Save the state
		taskState->state = pathState;
	}
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_DL_ILLUMINATE
// To: MK_DL_SAMPLE_BSDF or MK_GENERATE_NEXT_VERTEX_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_DL_ILLUMINATE(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_DL_ILLUMINATE(state = %d)\n", pathState);
#endif
	if (pathState != MK_DL_ILLUMINATE)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	const uint depth = pathInfo->depth.depth;

	__global BSDF *bsdf = &taskState->bsdf;

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, depth);

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// It will set eventually to true if the light is visible
	taskDirectLight->directLightResult = NOT_VISIBLE;

	if (!BSDF_IsDelta(bsdf
			MATERIALS_PARAM) &&
			DirectLight_Illuminate(
				bsdf,
				&rays[gid],
				worldCenterX, worldCenterY, worldCenterZ, worldRadius,
				&task->tmpHitPoint,
				rays[gid].time,
				Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, depth, IDX_DIRECTLIGHT_X),
				Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, depth, IDX_DIRECTLIGHT_Y),
				Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, depth, IDX_DIRECTLIGHT_Z),
				Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, depth, IDX_DIRECTLIGHT_W),
				&taskDirectLight->illumInfo
				LIGHTS_PARAM)) {
		// I have now to evaluate the BSDF
		taskState->state = MK_DL_SAMPLE_BSDF;
	} else {
		// No shadow ray to trace, move to the next vertex ray
		// however, I have to Check if this is the last path vertex
		taskState->state = (sample->result.lastPathVertex) ? MK_SPLAT_SAMPLE : MK_GENERATE_NEXT_VERTEX_RAY;
	}

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed = seedValue;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_DL_SAMPLE_BSDF
// To: MK_GENERATE_NEXT_VERTEX_RAY or MK_RT_DL or MK_SPLAT_SAMPLE
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_DL_SAMPLE_BSDF(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_DL_SAMPLE_BSDF(state = %d)\n", pathState);
#endif
	if (pathState != MK_DL_SAMPLE_BSDF)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global GPUTask *task = &tasks[gid];
	__global Sample *sample = &samples[gid];
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	if (DirectLight_BSDFSampling(
			&tasksDirectLight[gid].illumInfo,
			rays[gid].time, sample->result.lastPathVertex,
			pathInfo,
			&task->tmpPathDepthInfo,
			&taskState->bsdf,
			VLOAD3F(&rays[gid].d.x)
			LIGHTS_PARAM)) {
		const uint depth = pathInfo->depth.depth;

		__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
		__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
		__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
				sample, sampleDataPathBase, depth);

		__global GPUTask *task = &tasks[gid];
		Seed seedValue = task->seed;
		// This trick is required by Sampler_GetSample() macro
		Seed *seed = &seedValue;

		// Initialize the pass-through event for the shadow ray
		const float passThroughEvent = Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, depth, IDX_DIRECTLIGHT_A);
		Seed seedPassThroughEvent;
		Rnd_InitFloat(passThroughEvent, &seedPassThroughEvent);
		tasksDirectLight[gid].seedPassThroughEvent = seedPassThroughEvent;

		// Save the seed
		task->seed = seedValue;

		// Initialize the trough a shadow transparency flag used by Scene_Intersect()
		tasksDirectLight[gid].throughShadowTransparency = false;

#if defined(PARAM_HAS_VOLUMES)
		// Make a copy of current PathVolumeInfo for tracing the
		// shadow ray
		directLightVolInfos[gid] = pathInfo->volume;
#endif
		// I have to trace the shadow ray
		taskState->state = MK_RT_DL;
	} else {
		// No shadow ray to trace, move to the next vertex ray
		// however, I have to check if this is the last path vertex
		taskState->state = (sample->result.lastPathVertex) ? MK_SPLAT_SAMPLE : MK_GENERATE_NEXT_VERTEX_RAY;
	}
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

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY(state = %d)\n", pathState);
#endif
	if (pathState != MK_GENERATE_NEXT_VERTEX_RAY)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	const uint depth = pathInfo->depth.depth;

	__global BSDF *bsdf = &taskState->bsdf;

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, depth);

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	__global Ray *ray = &rays[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Sample the BSDF
	float3 sampledDir;
	float3 bsdfSample;
	float cosSampledDir;
	float bsdfPdfW;
	BSDFEvent bsdfEvent;

	if (BSDF_IsShadowCatcher(bsdf MATERIALS_PARAM) && (tasksDirectLight[gid].directLightResult  != SHADOWED)) {
		bsdfSample = BSDF_ShadowCatcherSample(bsdf,
				&sampledDir, &bsdfPdfW, &cosSampledDir, &bsdfEvent
				MATERIALS_PARAM);

#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		if (sample->result.firstPathVertex) {
			// In this case I have also to set the value of the alpha channel to 0.0
			sample->result.alpha = 0.f;
		}
#endif
	} else {
		bsdfSample = BSDF_Sample(bsdf,
				Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, depth, IDX_BSDF_X),
				Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, depth, IDX_BSDF_Y),
				&sampledDir, &bsdfPdfW, &cosSampledDir, &bsdfEvent
				MATERIALS_PARAM);

		pathInfo->isPassThroughPath = false;
	}

	if (sample->result.firstPathVertex)
		sample->result.firstPathVertexEvent = bsdfEvent;

	EyePathInfo_AddVertex(pathInfo, bsdf, bsdfEvent, bsdfPdfW,
#if defined(PARAM_HYBRID_BACKFORWARD_GLOSSINESSTHRESHOLD)
			PARAM_HYBRID_BACKFORWARD_GLOSSINESSTHRESHOLD
#else
			0.f
#endif
			MATERIALS_PARAM);

	// Russian Roulette
	const bool rrEnabled = EyePathInfo_UseRR(pathInfo, PARAM_RR_DEPTH);
	const float rrProb = rrEnabled ? RussianRouletteProb(bsdfSample) : 1.f;
	const bool rrContinuePath = !rrEnabled ||
		!(rrProb < Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, pathInfo->depth.depth, IDX_RR));

	// Max. path depth
	const bool maxPathDepth = (pathInfo->depth.depth >= PARAM_MAX_PATH_DEPTH);

	const bool continuePath = !Spectrum_IsBlack(bsdfSample) && rrContinuePath && !maxPathDepth;
	if (continuePath) {
		float3 throughputFactor = WHITE;

		// RR increases path contribution
		throughputFactor /= rrProb;
		throughputFactor *= bsdfSample;

		VSTORE3F(throughputFactor * VLOAD3F(taskState->throughput.c), taskState->throughput.c);

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
		// This is valid for irradiance AOV only if it is not a SPECULAR material and
		// first path vertex. Set or update sampleResult.irradiancePathThroughput
		if (sample->result.firstPathVertex) {
			if (!(BSDF_GetEventTypes(&taskState->bsdf
						MATERIALS_PARAM) & SPECULAR))
				VSTORE3F(M_1_PI_F * fabs(dot(
						VLOAD3F(&bsdf->hitPoint.shadeN.x),
						sampledDir)) / rrProb,
						sample->result.irradiancePathThroughput.c);
			else
				VSTORE3F(BLACK, sample->result.irradiancePathThroughput.c);
		} else
			VSTORE3F(throughputFactor * VLOAD3F(sample->result.irradiancePathThroughput.c), sample->result.irradiancePathThroughput.c);
#endif

		Ray_Init2(ray, BSDF_GetRayOrigin(bsdf, sampledDir), sampledDir, ray->time);

		sample->result.firstPathVertex = false;

		// Initialize the pass-through event seed
		const float passThroughEvent = Sampler_GetSamplePathVertex(seed, sample, sampleDataPathVertexBase, pathInfo->depth.depth, IDX_PASSTHROUGH);
		Seed seedPassThroughEvent;
		Rnd_InitFloat(passThroughEvent, &seedPassThroughEvent);
		taskState->seedPassThroughEvent = seedPassThroughEvent;

		// Initialize the trough a shadow transparency flag used by Scene_Intersect()
		taskState->throughShadowTransparency = false;


		pathState = MK_RT_NEXT_VERTEX;
	} else
		pathState = MK_SPLAT_SAMPLE;

	// Save the state
	taskState->state = pathState;

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed = seedValue;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_SPLAT_SAMPLE
// To: MK_NEXT_SAMPLE
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_SPLAT_SAMPLE(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_SPLAT_SAMPLE(state = %d)\n", pathState);
#endif
	if (pathState != MK_SPLAT_SAMPLE)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Read the seed
	Seed seedValue = task->seed;
	
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

#if defined(PARAM_FILM_DENOISER)
	// Initialize Film radiance group scale table
	float3 filmRadianceGroupScale[PARAM_FILM_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	filmRadianceGroupScale[0] = (float3)(filmRadianceGroupScale0_R, filmRadianceGroupScale0_G, filmRadianceGroupScale0_B);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	filmRadianceGroupScale[1] = (float3)(filmRadianceGroupScale1_R, filmRadianceGroupScale1_G, filmRadianceGroupScale1_B);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	filmRadianceGroupScale[2] = (float3)(filmRadianceGroupScale2_R, filmRadianceGroupScale2_G, filmRadianceGroupScale2_B);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	filmRadianceGroupScale[3] = (float3)(filmRadianceGroupScale3_R, filmRadianceGroupScale3_G, filmRadianceGroupScale3_B);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	filmRadianceGroupScale[4] = (float3)(filmRadianceGroupScale4_R, filmRadianceGroupScale4_G, filmRadianceGroupScale4_B);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroupScale[5] = (float3)(filmRadianceGroupScale5_R, filmRadianceGroupScale5_G, filmRadianceGroupScale5_B);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroupScale[6] = (float3)(filmRadianceGroupScale6_R, filmRadianceGroupScale6_G, filmRadianceGroupScale6_B);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	filmRadianceGroupScale[7] = (float3)(filmRadianceGroupScale7_R, filmRadianceGroupScale7_G, filmRadianceGroupScale7_B);
#endif
#endif

#if defined(PARAM_PGIC_ENABLED) && defined(PARAM_PGIC_DEBUG_SHOWINDIRECTPATHMIX)
	if (!taskState->photonGIShowIndirectPathMixUsed)
		VSTORE3F((float3)(1.f, 0.f, 0.f), sample->result.radiancePerPixelNormalized[0].c);
#endif

	//--------------------------------------------------------------------------
	// Variance clamping
	//--------------------------------------------------------------------------

	if (PARAM_SQRT_VARIANCE_CLAMP_MAX_VALUE > 0.f) {
		// Radiance clamping
		VarianceClamping_Clamp(&sample->result, PARAM_SQRT_VARIANCE_CLAMP_MAX_VALUE
				FILM_PARAM);
	}

	//--------------------------------------------------------------------------
	// Sampler splat sample
	//--------------------------------------------------------------------------

	Sampler_SplatSample(&seedValue, samplerSharedData, sample, sampleData
			FILM_PARAM);
	taskStats[gid].sampleCount += 1;

	// Save the state
	taskState->state = MK_NEXT_SAMPLE;

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed = seedValue;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_NEXT_SAMPLE
// To: MK_GENERATE_CAMERA_RAY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths_MK_NEXT_SAMPLE(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_NEXT_SAMPLE(state = %d)\n", pathState);
#endif
	if (pathState != MK_NEXT_SAMPLE)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Read the seed
	Seed seedValue = task->seed;

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	Sampler_NextSample(&seedValue, samplerSharedData, sample, sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
			filmNoise,
#endif
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);

	// Save the state

	// Generate a new path and camera ray only it is not TILEPATHOCL
#if !defined(RENDER_ENGINE_TILEPATHOCL) && !defined(RENDER_ENGINE_RTPATHOCL)
	taskState->state = MK_GENERATE_CAMERA_RAY;
#else
	taskState->state = MK_DONE;
	// Mark the ray like like one to NOT trace
	rays[gid].flags = RAY_FLAGS_MASKED;
#endif

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed = seedValue;
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
	// Generate a new path and camera ray only it is not TILEPATHOCL: path regeneration
	// is not used in this case
#if !defined(RENDER_ENGINE_TILEPATHOCL) && !defined(RENDER_ENGINE_RTPATHOCL)
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0) printf("Kernel: AdvancePaths_MK_GENERATE_CAMERA_RAY(state = %d)\n", pathState);
#endif
	if (pathState != MK_GENERATE_CAMERA_RAY)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);

	// Read the seed
	Seed seedValue = task->seed;

	__global Ray *ray = &rays[gid];

	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Re-initialize the volume information
#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo_Init(&pathInfo->volume);
#endif

	GenerateEyePath(&tasksDirectLight[gid], taskState, sample, sampleDataPathBase, camera,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3,
			pixelFilterDistribution,
			ray,
			pathInfo,
			&seedValue);
	// taskState->state is set to RT_NEXT_VERTEX inside GenerateEyePath()

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed = seedValue;

#endif
}
