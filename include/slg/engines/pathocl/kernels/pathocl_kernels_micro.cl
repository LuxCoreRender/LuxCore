#line 2 "patchocl_kernels_micro.cl"

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
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
	if (pathState != MK_RT_NEXT_VERTEX)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	const bool continueToTrace = Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
			&pathVolInfos[gid],
			&tasks[gid].tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
			taskState->bsdf.hitPoint.passThroughEvent,
#endif
			&rays[gid], &rayHits[gid], &taskState->bsdf,
			&taskState->throughput,
			&samples[gid].result,
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
		const bool rayMiss = (rayHits[gid].meshIndex == NULL_INDEX);
		taskState->state = rayMiss ? MK_HIT_NOTHING : MK_HIT_OBJECT;
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
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
	if (pathState != MK_HIT_NOTHING)
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

	// Nothing was hit, add environmental lights radiance

#if defined(PARAM_HAS_ENVLIGHTS)
	DirectHitInfiniteLight(
			taskDirectLight->lastBSDFEvent,
			&taskState->throughput,
			VLOAD3F(&rays[gid].d.x), taskDirectLight->lastPdfW,
			&samples[gid].result
			LIGHTS_PARAM);
#endif

	if (taskState->depth == 1) {
		__global Sample *sample = &samples[gid];

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
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
	if (pathState != MK_HIT_OBJECT)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global BSDF *bsdf = &taskState->bsdf;
	__global Sample *sample = &samples[gid];

	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	// Something was hit

	if (taskState->depth == 1) {
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
		__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];

		// Initialize image maps page pointer table
		INIT_IMAGEMAPS_PAGES

		DirectHitFiniteLight(
				taskDirectLight->lastBSDFEvent,
				&taskState->throughput,
				rayHits[gid].t, bsdf, taskDirectLight->lastPdfW,
				&sample->result
				LIGHTS_PARAM);
	}
#endif

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
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
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
			&rays[gid], &rayHits[gid], &task->tmpBsdf,
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
	const bool rayMiss = (rayHits[gid].meshIndex == NULL_INDEX);

	// If continueToTrace, there is nothing to do, just keep the same state
	if (!continueToTrace) {
		__global Sample *sample = &samples[gid];

		if (rayMiss) {
			// Nothing was hit, the light source is visible

			SampleResult_AddDirectLight(&sample->result, taskDirectLight->illumInfo.lightID,
					BSDF_GetEventTypes(&taskState->bsdf
						MATERIALS_PARAM),
					VLOAD3F(taskDirectLight->illumInfo.lightRadiance.c),
					1.f);
		}

		// Check if this is the last path vertex
		if (sample->result.lastPathVertex)
			pathState = MK_SPLAT_SAMPLE;
		else
			pathState = MK_GENERATE_NEXT_VERTEX_RAY;

		// Save the state
		taskState->state = pathState;
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
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	if (taskState->state != MK_DL_ILLUMINATE)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	const uint depth = taskState->depth;

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
	Seed seedValue = task->seed;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;


	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	if (!BSDF_IsDelta(bsdf
			MATERIALS_PARAM) &&
			DirectLight_Illuminate(
#if defined(PARAM_HAS_INFINITELIGHTS)
				worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
				&task->tmpHitPoint,
#endif
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_X),
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Y),
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_Z),
#if defined(PARAM_HAS_PASSTHROUGH)
				Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_W),
#endif
				VLOAD3F(&bsdf->hitPoint.p.x), &tasksDirectLight[gid].illumInfo
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
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
	if (pathState != MK_DL_SAMPLE_BSDF)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	const uint depth = taskState->depth;
	__global Sample *sample = &samples[gid];

	// Initialize image maps page pointer table
	INIT_IMAGEMAPS_PAGES
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	if (DirectLight_BSDFSampling(
			&tasksDirectLight[gid].illumInfo,
			rays[gid].time, sample->result.lastPathVertex, depth,
			&taskState->throughput, &taskState->bsdf,
			&rays[gid]
			LIGHTS_PARAM)) {
#if defined(PARAM_HAS_PASSTHROUGH)
		__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
		__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
#if (PARAM_SAMPLER_TYPE != 0)
		// Used by Sampler_GetSamplePathVertex() macro
		__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
				sample, sampleDataPathBase, depth);
#endif
		__global GPUTask *task = &tasks[gid];
		Seed seedValue = task->seed;
		// This trick is required by Sampler_GetSample() macro
		Seed *seed = &seedValue;


		// Initialize the pass-through event for the shadow ray
		tasksDirectLight[gid].rayPassThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_DIRECTLIGHT_A);
		
		// Save the seed
		task->seed = seedValue;
#endif
#if defined(PARAM_HAS_VOLUMES)
		// Make a copy of current PathVolumeInfo for tracing the
		// shadow ray
		directLightVolInfos[gid] = pathVolInfos[gid];
#endif
		// I have to trace the shadow ray
		taskState->state = MK_RT_DL;
	} else {
		// No shadow ray to trace, move to the next vertex ray
		// however, I have to Check if this is the last path vertex
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
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
	if (pathState != MK_GENERATE_NEXT_VERTEX_RAY)
		return;

 	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

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
		tasksDirectLight[gid].lastBSDFEvent = event;
		tasksDirectLight[gid].lastPdfW = lastPdfW;
#if defined(PARAM_HAS_PASSTHROUGH)
		// This is a bit tricky. I store the passThroughEvent in the BSDF
		// before of the initialization because it can be use during the
		// tracing of next path vertex ray.

		// This sampleDataPathVertexBase is used inside Sampler_GetSamplePathVertex() macro
		__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
			sample, sampleDataPathBase, depth);
		taskState->bsdf.hitPoint.passThroughEvent = Sampler_GetSamplePathVertex(depth, IDX_PASSTHROUGH);
#endif

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
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
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

	if (PARAM_RADIANCE_CLAMP_MAXVALUE > 0.f) {
		// Radiance clamping
		SampleResult_ClampRadiance(&sample->result, PARAM_RADIANCE_CLAMP_MAXVALUE);
	}

	Sampler_SplatSample(&seedValue, sample, sampleData
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
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
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

	Sampler_NextSample(&seedValue, sample, sampleData, filmWidth, filmHeight);

	// Save the state
	taskState->state = MK_GENERATE_CAMERA_RAY;

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
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Read the path state
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskState *taskState = &tasksState[gid];
	PathState pathState = taskState->state;
	if (pathState != MK_GENERATE_CAMERA_RAY)
		return;

	//--------------------------------------------------------------------------
	// Start of variables setup
	//--------------------------------------------------------------------------

	__global Sample *sample = &samples[gid];
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Read the seed
	Seed seedValue = task->seed;

	__global Ray *ray = &rays[gid];
	
	//--------------------------------------------------------------------------
	// End of variables setup
	//--------------------------------------------------------------------------

	GenerateCameraPath(&tasksDirectLight[gid], taskState, sample, sampleData, camera, filmWidth, filmHeight, ray, &seedValue);
	// taskState->state is set to RT_NEXT_VERTEX inside GenerateCameraPath()

	// Re-initialize the volume information
#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo_Init(&pathVolInfos[gid]);
#endif

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed = seedValue;
}
