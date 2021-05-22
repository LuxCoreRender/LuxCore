#line 2 "pathoclbase_kernels_micro.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

__kernel void AdvancePaths_MK_RT_NEXT_VERTEX(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	// This has to be done by the first kernel to run after RT kernel
	sampleResult->rayCount += 1;

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_RT_NEXT_VERTEX(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_RT_NEXT_VERTEX)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------
	
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	__constant const Scene* restrict scene = &taskConfig->scene;

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
	const bool continueToTrace = Scene_Intersect(taskConfig,
			EYE_RAY | ((pathInfo->depth.depth == 0) ? CAMERA_RAY : INDIRECT_RAY),
			&throughShadowTransparency,
			&pathInfo->volume,
			&tasks[gid].tmpHitPoint,
			passThroughEvent,
			&rays[gid], &rayHits[gid], &taskState->bsdf,
			&connectionThroughput, VLOAD3F(taskState->throughput.c),
			sampleResult,
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
			const BSDFEvent eventTypes = BSDF_GetEventTypes(&taskState->bsdf
					MATERIALS_PARAM);

			sampleResult->lastPathVertex = PathDepthInfo_IsLastPathVertex(&pathInfo->depth, 
					&taskConfig->pathTracer.maxPathDepth, eventTypes);

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

__kernel void AdvancePaths_MK_HIT_NOTHING(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_HIT_NOTHING(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_HIT_NOTHING)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	__constant const Scene* restrict scene = &taskConfig->scene;
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Nothing was hit, add environmental lights radiance

	bool checkDirectLightHit = true;
	
	checkDirectLightHit = checkDirectLightHit &&
			(!(taskConfig->pathTracer.forceBlackBackground && pathInfo->isPassThroughPath) || !pathInfo->isPassThroughPath);

	checkDirectLightHit = checkDirectLightHit &&
			// Avoid to render caustic path if hybridBackForwardEnable
			(!taskConfig->pathTracer.hybridBackForward.enabled || !EyePathInfo_IsCausticPath(pathInfo));

	checkDirectLightHit = checkDirectLightHit &&
			((!taskConfig->pathTracer.pgic.indirectEnabled && !taskConfig->pathTracer.pgic.causticEnabled) ||
			PhotonGICache_IsDirectLightHitVisible(taskConfig, pathInfo, taskState->photonGICausticCacheUsed));

	if (checkDirectLightHit) {
		DirectHitInfiniteLight(
				&taskConfig->film,
				pathInfo,
				&taskState->throughput,
				&rays[gid],
				sampleResult->firstPathVertex ? NULL : &taskState->bsdf,
				sampleResult
				LIGHTS_PARAM);
	}

	if (pathInfo->depth.depth == 0) {
		sampleResult->alpha = 0.f;
		sampleResult->depth = INFINITY;
		sampleResult->position.x = INFINITY;
		sampleResult->position.y = INFINITY;
		sampleResult->position.z = INFINITY;
		sampleResult->geometryNormal.x = 0.f;
		sampleResult->geometryNormal.y = 0.f;
		sampleResult->geometryNormal.z = 0.f;
		sampleResult->shadingNormal.x = 0.f;
		sampleResult->shadingNormal.y = 0.f;
		sampleResult->shadingNormal.z = 0.f;
		sampleResult->materialID = 0;
		sampleResult->objectID = 0;
		sampleResult->uv.u = INFINITY;
		sampleResult->uv.v = INFINITY;
		sampleResult->isHoldout = false;
	} else if (!sampleResult->isHoldout && pathInfo->isTransmittedPath) {
		// I set to 0.0 also the alpha all purely transmitted paths hitting nothing
		sampleResult->alpha = 0.f;
	}

	taskState->state = MK_SPLAT_SAMPLE;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_HIT_OBJECT
// To: MK_DL_ILLUMINATE or MK_SPLAT_SAMPLE
//------------------------------------------------------------------------------

__kernel void AdvancePaths_MK_HIT_OBJECT(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_HIT_OBJECT(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_HIT_OBJECT)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global BSDF *bsdf = &taskState->bsdf;
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	__constant const Scene* restrict scene = &taskConfig->scene;
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];
	

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Something was hit

	if (taskState->albedoToDo && BSDF_IsAlbedoEndPoint(bsdf MATERIALS_PARAM)) {
		const float3 albedo = VLOAD3F(taskState->throughput.c) * BSDF_Albedo(bsdf
				MATERIALS_PARAM);
		VSTORE3F(albedo, sampleResult->albedo.c);

		taskState->albedoToDo = false;
	}

	if (pathInfo->depth.depth == 0) {
		const bool isHoldout = BSDF_IsHoldout(bsdf
				MATERIALS_PARAM);
		sampleResult->alpha = isHoldout ? 0.f : 1.f;
		sampleResult->depth = rayHits[gid].t;
		sampleResult->position = bsdf->hitPoint.p;
		sampleResult->geometryNormal = bsdf->hitPoint.geometryN;
		sampleResult->shadingNormal = bsdf->hitPoint.shadeN;
		sampleResult->materialID = BSDF_GetMaterialID(bsdf
				MATERIALS_PARAM);
		sampleResult->objectID = BSDF_GetObjectID(bsdf, sceneObjs);
		sampleResult->uv = bsdf->hitPoint.defaultUV;
		sampleResult->isHoldout = isHoldout;
	}

	//----------------------------------------------------------------------
	// Check if it is a baked material
	//----------------------------------------------------------------------

	if (BSDF_HasBakeMap(bsdf, COMBINED MATERIALS_PARAM)) {
		const float3 radiance = VLOAD3F(&taskState->throughput.c[0]) * BSDF_GetBakeMapValue(bsdf MATERIALS_PARAM);
		VADD3F(sampleResult->radiancePerPixelNormalized[0].c, radiance);

		taskState->state = MK_SPLAT_SAMPLE;
		return;
	} else if (BSDF_HasBakeMap(bsdf, LIGHTMAP MATERIALS_PARAM)) {
		const float3 radiance = VLOAD3F(&taskState->throughput.c[0]) *
				BSDF_Albedo(bsdf MATERIALS_PARAM) *
				BSDF_GetBakeMapValue(bsdf MATERIALS_PARAM);
		VADD3F(sampleResult->radiancePerPixelNormalized[0].c, radiance);

		taskState->state = MK_SPLAT_SAMPLE;
		return;
	}

	//--------------------------------------------------------------------------
	// Check if it is a light source and I have to add light emission
	//--------------------------------------------------------------------------

	bool checkDirectLightHit = true;

	checkDirectLightHit = checkDirectLightHit &&
			// Avoid to render caustic path if hybridBackForwardEnable
			(!taskConfig->pathTracer.hybridBackForward.enabled || !EyePathInfo_IsCausticPath(pathInfo));

	checkDirectLightHit = checkDirectLightHit &&
			((!taskConfig->pathTracer.pgic.indirectEnabled && !taskConfig->pathTracer.pgic.causticEnabled) ||
			PhotonGICache_IsDirectLightHitVisible(taskConfig, pathInfo, taskState->photonGICausticCacheUsed));

	// Check if it is a light source (note: I can hit only triangle area light sources)
	if (BSDF_IsLightSource(bsdf) && checkDirectLightHit) {
		DirectHitFiniteLight(
				&taskConfig->film,
				pathInfo,
				&taskState->throughput,
				&rays[gid],
				rayHits[gid].t,
				bsdf,
				sampleResult
				LIGHTS_PARAM);
	}

	//----------------------------------------------------------------------
	// Check if I can use the photon cache
	//----------------------------------------------------------------------

	if (taskConfig->pathTracer.pgic.indirectEnabled || taskConfig->pathTracer.pgic.causticEnabled) {
		const bool isPhotonGIEnabled = PhotonGICache_IsPhotonGIEnabled(bsdf,
				taskConfig->pathTracer.pgic.glossinessUsageThreshold
				MATERIALS_PARAM);

		switch (taskConfig->pathTracer.pgic.debugType) {
			case PGIC_DEBUG_SHOWINDIRECT: {
				if (isPhotonGIEnabled) {
					__global const Spectrum* restrict radiance = PhotonGICache_GetIndirectRadiance(bsdf,
							pgicRadiancePhotons, pgicLightGroupCounts, pgicRadiancePhotonsValues, pgicRadiancePhotonsBVHNodes,
							taskConfig->pathTracer.pgic.indirectLookUpRadius * taskConfig->pathTracer.pgic.indirectLookUpRadius,
							taskConfig->pathTracer.pgic.indirectLookUpNormalCosAngle);
					if (radiance) {
						for (uint i = 0; i < pgicLightGroupCounts; ++i)
							VADD3F(sampleResult->radiancePerPixelNormalized[i].c, VLOAD3F(radiance[i].c));
					}
				}
				taskState->state = MK_SPLAT_SAMPLE;
				return;
			}
			case PGIC_DEBUG_SHOWCAUSTIC: {
				if (isPhotonGIEnabled) {
					PhotonGICache_ConnectWithCausticPaths(bsdf,
							pgicCausticPhotons, pgicCausticPhotonsBVHNodes,
							taskConfig->pathTracer.pgic.causticPhotonTracedCount,
							taskConfig->pathTracer.pgic.causticLookUpRadius,
							taskConfig->pathTracer.pgic.causticLookUpNormalCosAngle,
							WHITE,
							&sampleResult->radiancePerPixelNormalized[0]
							MATERIALS_PARAM);
				}
				taskState->state = MK_SPLAT_SAMPLE;
				return;
			}
			case PGIC_DEBUG_SHOWINDIRECTPATHMIX: {
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
								taskConfig->pathTracer.pgic.glossinessUsageThreshold,
								taskConfig->pathTracer.pgic.indirectUsageThresholdScale,
								taskConfig->pathTracer.pgic.indirectLookUpRadius))) {
						VSTORE3F(MAKE_FLOAT3(0.f, 0.f, 1.f), sampleResult->radiancePerPixelNormalized[0].c);
						taskState->photonGIShowIndirectPathMixUsed = true;

						taskState->state = MK_SPLAT_SAMPLE;
						return;
					}

					taskState->photonGICacheEnabledOnLastHit = true;
				} else
					taskState->photonGICacheEnabledOnLastHit = false;

				break;
			}
			case PGIC_DEBUG_NONE:
			default: {
				if (isPhotonGIEnabled) {
					if (taskConfig->pathTracer.pgic.causticEnabled &&
							(!taskConfig->pathTracer.hybridBackForward.enabled || (pathInfo->depth.depth != 0))) {
						const bool isEmpty = PhotonGICache_ConnectWithCausticPaths(bsdf,
								pgicCausticPhotons, pgicCausticPhotonsBVHNodes,
								taskConfig->pathTracer.pgic.causticPhotonTracedCount,
								taskConfig->pathTracer.pgic.causticLookUpRadius,
								taskConfig->pathTracer.pgic.causticLookUpNormalCosAngle,
								VLOAD3F(taskState->throughput.c),
								&sampleResult->radiancePerPixelNormalized[0]
								MATERIALS_PARAM);

						if (!isEmpty)
							taskState->photonGICausticCacheUsed = true;
					}

					if (taskConfig->pathTracer.pgic.indirectEnabled) {
						Seed seedPassThroughEvent = taskState->seedPassThroughEvent;
						const float passThroughEvent = Rnd_FloatValue(&seedPassThroughEvent);

						if (taskState->photonGICacheEnabledOnLastHit &&
								(rayHits[gid].t > PhotonGICache_GetIndirectUsageThreshold(
									pathInfo->lastBSDFEvent,
									pathInfo->lastGlossiness,
									// I hope to not introduce strange sample correlations
									// by using passThrough here
									passThroughEvent,
									taskConfig->pathTracer.pgic.glossinessUsageThreshold,
									taskConfig->pathTracer.pgic.indirectUsageThresholdScale,
									taskConfig->pathTracer.pgic.indirectLookUpRadius))) {
							__global const Spectrum* restrict radiance = PhotonGICache_GetIndirectRadiance(bsdf,
								pgicRadiancePhotons, pgicLightGroupCounts, pgicRadiancePhotonsValues, pgicRadiancePhotonsBVHNodes,
								taskConfig->pathTracer.pgic.indirectLookUpRadius * taskConfig->pathTracer.pgic.indirectLookUpRadius,
								taskConfig->pathTracer.pgic.indirectLookUpNormalCosAngle);

							if (radiance) {
								for (uint i = 0; i < pgicLightGroupCounts; ++i)
									VADD3F(sampleResult->radiancePerPixelNormalized[i].c, VLOAD3F(taskState->throughput.c) * VLOAD3F(radiance[i].c));
							}

							// I can terminate the path, all done
							taskState->state = MK_SPLAT_SAMPLE;
							return;
						}
					}

					taskState->photonGICacheEnabledOnLastHit = true;
				} else
					taskState->photonGICacheEnabledOnLastHit = false;

				break;
			}
		}
	}

	//----------------------------------------------------------------------
	// Check if this is the last path vertex (but not also the first)
	//
	// I handle as a special case when the path vertex is both the first
	// and the last: I do direct light sampling without MIS.
	taskState->state = (sampleResult->lastPathVertex && !sampleResult->firstPathVertex) ?
		MK_SPLAT_SAMPLE : MK_DL_ILLUMINATE;
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_RT_DL
// To: MK_SPLAT_SAMPLE or MK_GENERATE_NEXT_VERTEX_RAY
//------------------------------------------------------------------------------

__kernel void AdvancePaths_MK_RT_DL(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_RT_DL(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_RT_DL)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__constant const Scene* restrict scene = &taskConfig->scene;
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

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
		Scene_Intersect(taskConfig,
			EYE_RAY | SHADOW_RAY,
			&throughShadowTransparency,
			&directLightVolInfos[gid],
			&task->tmpHitPoint,
			passThroughEvent,
			&rays[gid], &rayHits[gid], &task->tmpBsdf,
			&connectionThroughput, WHITE,
			NULL,
			true
			MATERIALS_PARAM
			);
	taskDirectLight->throughShadowTransparency = throughShadowTransparency;
	VSTORE3F(connectionThroughput * VLOAD3F(taskDirectLight->illumInfo.lightRadiance.c), taskDirectLight->illumInfo.lightRadiance.c);
	VSTORE3F(connectionThroughput * VLOAD3F(taskDirectLight->illumInfo.lightIrradiance.c), taskDirectLight->illumInfo.lightIrradiance.c);

	const bool rayMiss = (rayHits[gid].meshIndex == NULL_INDEX);

	// If continueToTrace, there is nothing to do, just keep the same state
	if (!continueToTrace) {
		if (rayMiss) {
			// Nothing was hit, the light source is visible

			__global BSDF *bsdf = &taskState->bsdf;

			if (!BSDF_IsShadowCatcher(bsdf MATERIALS_PARAM)) {
				const float3 lightRadiance = VLOAD3F(taskDirectLight->illumInfo.lightRadiance.c);
				SampleResult_AddDirectLight(&taskConfig->film,
						sampleResult, taskDirectLight->illumInfo.lightID,
						BSDF_GetEventTypes(bsdf
							MATERIALS_PARAM),
						VLOAD3F(taskState->throughput.c), lightRadiance,
						1.f);

				// The first path vertex is not handled by AddDirectLight(). This is valid
				// for irradiance AOV only if it is not a SPECULAR material.
				//
				// Note: irradiance samples the light sources only here (i.e. no
				// direct hit, no MIS, it would be useless)
				if ((sampleResult->firstPathVertex) && !(BSDF_GetEventTypes(bsdf
							MATERIALS_PARAM) & SPECULAR)) {
					const float3 irradiance = (M_1_PI_F * fabs(dot(
								VLOAD3F(&bsdf->hitPoint.shadeN.x),
								VLOAD3F(&rays[gid].d.x)))) *
							VLOAD3F(taskDirectLight->illumInfo.lightIrradiance.c);
					VSTORE3F(irradiance, sampleResult->irradiance.c);
				}
			}

			taskDirectLight->directLightResult = ILLUMINATED;
		} else
			taskDirectLight->directLightResult = SHADOWED;

		// Check if this is the last path vertex
		if (sampleResult->lastPathVertex)
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

__kernel void AdvancePaths_MK_DL_ILLUMINATE(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_DL_ILLUMINATE(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_DL_ILLUMINATE)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global EyePathInfo *pathInfo = &eyePathInfos[gid];

	__global BSDF *bsdf = &taskState->bsdf;

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by SAMPLER_PARAM macro
	Seed *seed = &seedValue;

	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__constant const Scene* restrict scene = &taskConfig->scene;
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];
	const uint sampleOffset = taskConfig->pathTracer.eyeSampleBootSize + pathInfo->depth.depth * taskConfig->pathTracer.eyeSampleStepSize;

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
				Sampler_GetSample(taskConfig, sampleOffset + IDX_DIRECTLIGHT_X SAMPLER_PARAM),
				Sampler_GetSample(taskConfig, sampleOffset + IDX_DIRECTLIGHT_Y SAMPLER_PARAM),
				Sampler_GetSample(taskConfig, sampleOffset + IDX_DIRECTLIGHT_Z SAMPLER_PARAM),
				Sampler_GetSample(taskConfig, sampleOffset + IDX_DIRECTLIGHT_W SAMPLER_PARAM),
				&taskDirectLight->illumInfo
				LIGHTS_PARAM)) {
		// I have now to evaluate the BSDF
		taskState->state = MK_DL_SAMPLE_BSDF;
	} else {
		// No shadow ray to trace, move to the next vertex ray
		// however, I have to Check if this is the last path vertex
		taskState->state = (sampleResult->lastPathVertex) ? MK_SPLAT_SAMPLE : MK_GENERATE_NEXT_VERTEX_RAY;
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

__kernel void AdvancePaths_MK_DL_SAMPLE_BSDF(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_DL_SAMPLE_BSDF(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_DL_SAMPLE_BSDF)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global GPUTask *task = &tasks[gid];
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	__constant const Scene* restrict scene = &taskConfig->scene;
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];
	const uint sampleOffset = taskConfig->pathTracer.eyeSampleBootSize + pathInfo->depth.depth * taskConfig->pathTracer.eyeSampleStepSize;

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	if (DirectLight_BSDFSampling(
			taskConfig,
			&tasksDirectLight[gid].illumInfo,
			rays[gid].time, sampleResult->lastPathVertex,
			pathInfo,
			&task->tmpPathDepthInfo,
			&taskState->bsdf,
			VLOAD3F(&rays[gid].d.x)
			LIGHTS_PARAM)) {
		__global GPUTask *task = &tasks[gid];
		Seed seedValue = task->seed;
		// This trick is required by SAMPLER_PARAM macro
		Seed *seed = &seedValue;

		// Initialize the pass-through event for the shadow ray
		const float passThroughEvent = Sampler_GetSample(taskConfig, sampleOffset + IDX_DIRECTLIGHT_A SAMPLER_PARAM);
		Seed seedPassThroughEvent;
		Rnd_InitFloat(passThroughEvent, &seedPassThroughEvent);
		tasksDirectLight[gid].seedPassThroughEvent = seedPassThroughEvent;

		// Save the seed
		task->seed = seedValue;

		// Initialize the trough a shadow transparency flag used by Scene_Intersect()
		tasksDirectLight[gid].throughShadowTransparency = false;

		// Make a copy of current PathVolumeInfo for tracing the
		// shadow ray
		directLightVolInfos[gid] = pathInfo->volume;

		// I have to trace the shadow ray
		taskState->state = MK_RT_DL;
	} else {
		// No shadow ray to trace, move to the next vertex ray
		// however, I have to check if this is the last path vertex
		taskState->state = (sampleResult->lastPathVertex) ? MK_SPLAT_SAMPLE : MK_GENERATE_NEXT_VERTEX_RAY;
	}
}

//------------------------------------------------------------------------------
// Evaluation of the Path finite state machine.
//
// From: MK_GENERATE_NEXT_VERTEX_RAY
// To: MK_SPLAT_SAMPLE or MK_RT_NEXT_VERTEX
//------------------------------------------------------------------------------

__kernel void AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_GENERATE_NEXT_VERTEX_RAY)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	__global BSDF *bsdf = &taskState->bsdf;

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by SAMPLER_PARAM macro
	Seed *seed = &seedValue;

	__constant const Scene* restrict scene = &taskConfig->scene;
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];
	const uint sampleOffset = taskConfig->pathTracer.eyeSampleBootSize + pathInfo->depth.depth * taskConfig->pathTracer.eyeSampleStepSize;

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

	if (BSDF_IsShadowCatcher(bsdf MATERIALS_PARAM) && (tasksDirectLight[gid].directLightResult != SHADOWED)) {
		bsdfSample = BSDF_ShadowCatcherSample(bsdf,
				&sampledDir, &bsdfPdfW, &cosSampledDir, &bsdfEvent
				MATERIALS_PARAM);

		if (sampleResult->firstPathVertex) {
			// In this case I have also to set the value of the alpha channel to 0.0
			sampleResult->alpha = 0.f;
		}
	} else {
		const float3 shadowTransparency = BSDF_GetPassThroughShadowTransparency(bsdf
				MATERIALS_PARAM);
		if (!sampleResult->firstPathVertex && !Spectrum_IsBlack(shadowTransparency) && !pathInfo->isNearlyS) {
			sampledDir = -VLOAD3F(&bsdf->hitPoint.fixedDir.x);
			bsdfSample = shadowTransparency;
			bsdfPdfW = pathInfo->lastBSDFPdfW;
			cosSampledDir = -1.f;
			bsdfEvent = pathInfo->lastBSDFEvent;
		} else {
			bsdfSample = BSDF_Sample(bsdf,
					Sampler_GetSample(taskConfig, sampleOffset + IDX_BSDF_X SAMPLER_PARAM),
					Sampler_GetSample(taskConfig, sampleOffset + IDX_BSDF_Y SAMPLER_PARAM),
					&sampledDir, &bsdfPdfW, &cosSampledDir, &bsdfEvent
					MATERIALS_PARAM);

			pathInfo->isPassThroughPath = false;
		}
	}

	if (sampleResult->firstPathVertex)
		sampleResult->firstPathVertexEvent = bsdfEvent;

	EyePathInfo_AddVertex(pathInfo, bsdf, bsdfEvent, bsdfPdfW,
			taskConfig->pathTracer.hybridBackForward.glossinessThreshold
			MATERIALS_PARAM);

	// Russian Roulette
	const bool rrEnabled = EyePathInfo_UseRR(pathInfo, taskConfig->pathTracer.rrDepth);
	const float rrProb = rrEnabled ?
		RussianRouletteProb(taskConfig->pathTracer.rrImportanceCap, bsdfSample) :
		1.f;
	const bool rrContinuePath = !rrEnabled ||
		!(rrProb < Sampler_GetSample(taskConfig, sampleOffset + IDX_RR SAMPLER_PARAM));

	// Max. path depth
	const bool maxPathDepth = (pathInfo->depth.depth >= taskConfig->pathTracer.maxPathDepth.depth);

	const bool continuePath = !Spectrum_IsBlack(bsdfSample) && rrContinuePath && !maxPathDepth;
	if (continuePath) {
		float3 throughputFactor = WHITE;

		// RR increases path contribution
		throughputFactor /= rrProb;
		throughputFactor *= bsdfSample;

		VSTORE3F(throughputFactor * VLOAD3F(taskState->throughput.c), taskState->throughput.c);

		// This is valid for irradiance AOV only if it is not a SPECULAR material and
		// first path vertex. Set or update sampleResult.irradiancePathThroughput
		if (sampleResult->firstPathVertex) {
			if (!(BSDF_GetEventTypes(&taskState->bsdf
						MATERIALS_PARAM) & SPECULAR))
				VSTORE3F(TO_FLOAT3(M_1_PI_F * fabs(dot(
						VLOAD3F(&bsdf->hitPoint.shadeN.x),
						sampledDir)) / rrProb),
						sampleResult->irradiancePathThroughput.c);
			else
				VSTORE3F(BLACK, sampleResult->irradiancePathThroughput.c);
		} else
			VSTORE3F(throughputFactor * VLOAD3F(sampleResult->irradiancePathThroughput.c), sampleResult->irradiancePathThroughput.c);

		Ray_Init2(ray, BSDF_GetRayOrigin(bsdf, sampledDir), sampledDir, ray->time);

		sampleResult->firstPathVertex = false;

		// Initialize the pass-through event seed
		//
		// Note: I use the IDX_PASSTHROUGH of the next path depth
		const uint nextSampleOffset = taskConfig->pathTracer.eyeSampleBootSize + pathInfo->depth.depth * taskConfig->pathTracer.eyeSampleStepSize;
		const float passThroughEvent = Sampler_GetSample(taskConfig, nextSampleOffset + IDX_PASSTHROUGH SAMPLER_PARAM);
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

__kernel void AdvancePaths_MK_SPLAT_SAMPLE(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_SPLAT_SAMPLE(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_SPLAT_SAMPLE)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by SAMPLER_PARAM macro
	Seed *seed = &seedValue;

	__constant const Film* restrict film = &taskConfig->film;
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Initialize Film radiance group pointer table
	__global float *filmRadianceGroup[FILM_MAX_RADIANCE_GROUP_COUNT];
	filmRadianceGroup[0] = filmRadianceGroup0;
	filmRadianceGroup[1] = filmRadianceGroup1;
	filmRadianceGroup[2] = filmRadianceGroup2;
	filmRadianceGroup[3] = filmRadianceGroup3;
	filmRadianceGroup[4] = filmRadianceGroup4;
	filmRadianceGroup[5] = filmRadianceGroup5;
	filmRadianceGroup[6] = filmRadianceGroup6;
	filmRadianceGroup[7] = filmRadianceGroup7;

	// Initialize Film radiance group scale table
	float3 filmRadianceGroupScale[FILM_MAX_RADIANCE_GROUP_COUNT];
	filmRadianceGroupScale[0] = MAKE_FLOAT3(filmRadianceGroupScale0_R, filmRadianceGroupScale0_G, filmRadianceGroupScale0_B);
	filmRadianceGroupScale[1] = MAKE_FLOAT3(filmRadianceGroupScale1_R, filmRadianceGroupScale1_G, filmRadianceGroupScale1_B);
	filmRadianceGroupScale[2] = MAKE_FLOAT3(filmRadianceGroupScale2_R, filmRadianceGroupScale2_G, filmRadianceGroupScale2_B);
	filmRadianceGroupScale[3] = MAKE_FLOAT3(filmRadianceGroupScale3_R, filmRadianceGroupScale3_G, filmRadianceGroupScale3_B);
	filmRadianceGroupScale[4] = MAKE_FLOAT3(filmRadianceGroupScale4_R, filmRadianceGroupScale4_G, filmRadianceGroupScale4_B);
	filmRadianceGroupScale[5] = MAKE_FLOAT3(filmRadianceGroupScale5_R, filmRadianceGroupScale5_G, filmRadianceGroupScale5_B);
	filmRadianceGroupScale[6] = MAKE_FLOAT3(filmRadianceGroupScale6_R, filmRadianceGroupScale6_G, filmRadianceGroupScale6_B);
	filmRadianceGroupScale[7] = MAKE_FLOAT3(filmRadianceGroupScale7_R, filmRadianceGroupScale7_G, filmRadianceGroupScale7_B);

	if (sampleResult->isHoldout) {
		SampleResult_ClearRadiance(sampleResult);
		VSTORE3F(BLACK, sampleResult->albedo.c);
	}

	if (taskConfig->pathTracer.pgic.indirectEnabled &&
			(taskConfig->pathTracer.pgic.debugType == PGIC_DEBUG_SHOWINDIRECTPATHMIX) &&
			!taskState->photonGIShowIndirectPathMixUsed)
		VSTORE3F(MAKE_FLOAT3(1.f, 0.f, 0.f), sampleResult->radiancePerPixelNormalized[0].c);

	//--------------------------------------------------------------------------
	// Variance clamping
	//--------------------------------------------------------------------------

	const float sqrtVarianceClampMaxValue = taskConfig->pathTracer.sqrtVarianceClampMaxValue;
	if (sqrtVarianceClampMaxValue > 0.f) {
		// Radiance clamping
		VarianceClamping_Clamp(sampleResult, sqrtVarianceClampMaxValue
				FILM_PARAM);
	}

	//--------------------------------------------------------------------------
	// Sampler splat sample
	//--------------------------------------------------------------------------

	Sampler_SplatSample(taskConfig
			SAMPLER_PARAM
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

__kernel void AdvancePaths_MK_NEXT_SAMPLE(
		KERNEL_ARGS
		) {
	const size_t gid = get_global_id(0);

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
#if defined(DEBUG_PRINTF_KERNEL_NAME)
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_NEXT_SAMPLE(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_NEXT_SAMPLE)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by SAMPLER_PARAM macro
	Seed *seed = &seedValue;

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	Sampler_NextSample(taskConfig,
			filmNoise,
			filmUserImportance,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3
			SAMPLER_PARAM);

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

__kernel void AdvancePaths_MK_GENERATE_CAMERA_RAY(
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
	if (gid == 0)
		printf("Kernel: AdvancePaths_MK_GENERATE_CAMERA_RAY(state = %d)\n", pathState);
	else
		return;
#endif
	if (pathState != MK_GENERATE_CAMERA_RAY)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by SAMPLER_PARAM macro
	Seed *seed = &seedValue;

	__global Ray *ray = &rays[gid];
	__global EyePathInfo *pathInfo = &eyePathInfos[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Re-initialize the volume information
	PathVolumeInfo_Init(&pathInfo->volume);

	GenerateEyePath(taskConfig,
			&tasksDirectLight[gid], taskState,
			camera,
			cameraBokehDistribution,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3,
			pixelFilterDistribution,
			ray,
			pathInfo
			SAMPLER_PARAM);
	// taskState->state is set to RT_NEXT_VERTEX inside GenerateEyePath()

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed = seedValue;

#endif
}

