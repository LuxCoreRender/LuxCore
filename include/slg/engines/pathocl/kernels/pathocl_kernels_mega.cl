#line 2 "patchocl_kernels_mega.cl"

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

bool DirectLightSampling_ONE(
	__global DirectLightIlluminateInfo *info,
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
		__global Ray *shadowRay
		LIGHTS_PARAM_DECL) {
	const bool illuminated = DirectLight_Illuminate(
#if defined(PARAM_HAS_INFINITELIGHTS)
		worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		tmpHitPoint,
#endif
		u0, u1, u2,
#if defined(PARAM_HAS_PASSTHROUGH)
		lightPassThroughEvent,
#endif
		VLOAD3F(&bsdf->hitPoint.p.x), info
		LIGHTS_PARAM);

	if (!illuminated)
		return false;

	return DirectLight_BSDFSampling(
			info,
			time, lastPathVertex, depth,
			pathThroughput, bsdf,
			shadowRay
			LIGHTS_PARAM);
}

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
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global GPUTaskState *taskState = &tasksState[gid];

	// Read the path state
	PathState pathState = taskState->state;
	uint depth = taskState->depth;

	__global BSDF *bsdf = &taskState->bsdf;

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

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

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
			taskState->bsdf.hitPoint.passThroughEvent,
#endif
			ray, rayHit, bsdf,
			&taskState->throughput,
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
							taskDirectLight->lastBSDFEvent,
							&taskState->throughput,
							rayHit->t, bsdf, taskDirectLight->lastPdfW,
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
						taskDirectLight->lastBSDFEvent,
						&taskState->throughput,
						VLOAD3F(&ray->d.x), taskDirectLight->lastPdfW,
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
			taskState->bsdf.hitPoint.passThroughEvent = fabs(taskState->bsdf.hitPoint.passThroughEvent - .5f) * 2.f;
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
				taskDirectLight->rayPassThroughEvent,
#endif
				ray, rayHit, &task->tmpBsdf,
				&taskDirectLight->illumInfo.lightRadiance,
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

				SampleResult_AddDirectLight(&sample->result, taskDirectLight->illumInfo.lightID,
						BSDF_GetEventTypes(bsdf
							MATERIALS_PARAM),
						VLOAD3F(taskDirectLight->illumInfo.lightRadiance.c),
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
			taskDirectLight->rayPassThroughEvent = fabs(taskDirectLight->rayPassThroughEvent - .5f) * 2.f;
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
				&taskDirectLight->illumInfo,
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
				sample->result.lastPathVertex, depth, &taskState->throughput,
				bsdf, ray
				LIGHTS_PARAM)) {
#if defined(PARAM_HAS_PASSTHROUGH)
			// Initialize the pass-through event for the shadow ray
			taskDirectLight->rayPassThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_A);
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
		__global BSDF *bsdf = &taskState->bsdf;
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
			float3 throughput = VLOAD3F(taskState->throughput.c) *
					((event & SPECULAR) ? 1.f : min(1.f, lastPdfW / PARAM_PDF_CLAMP_VALUE));
			throughput *= bsdfSample;
			if (rrEnabled)
				throughput /= rrProb; // Russian Roulette

			VSTORE3F(throughput, taskState->throughput.c);

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

			taskState->depth = depth;
			taskDirectLight->lastBSDFEvent = event;
			taskDirectLight->lastPdfW = lastPdfW;
#if defined(PARAM_HAS_PASSTHROUGH)
			// This is a bit tricky. I store the passThroughEvent in the BSDF
			// before of the initialization because it can be use during the
			// tracing of next path vertex ray.

			// This sampleDataPathVertexBase is used inside Sampler_GetSamplePathVertex() macro
			__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
				sample, sampleDataPathBase, depth);
			taskState->bsdf.hitPoint.passThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_PASSTHROUGH);
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

		if (PARAM_RADIANCE_CLAMP_MAXVALUE > 0.f) {
			// Radiance clamping
			SampleResult_ClampRadiance(&sample->result, PARAM_RADIANCE_CLAMP_MAXVALUE);
		}

		Sampler_SplatSample(&seedValue, sample, sampleData
			FILM_PARAM);
		Sampler_NextSample(seed, sample, sampleData, filmWidth, filmHeight);
		taskStats[gid].sampleCount += 1;

		GenerateCameraPath(taskDirectLight, taskState, sample, sampleData, camera, filmWidth, filmHeight, ray, seed);
		// taskState->state is set to RT_NEXT_VERTEX inside Sampler_NextSample() => GenerateCameraPath()
		
		// Re-initialize the volume information
#if defined(PARAM_HAS_VOLUMES)
		PathVolumeInfo_Init(&pathVolInfos[gid]);
#endif
	} else {
		// Save the state
		taskState->state = pathState;
	}

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;
}
