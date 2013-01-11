#line 2 "patchocl_kernels.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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
//  PARAM_MAX_RR_DEPTH
//  PARAM_MAX_RR_CAP
//  PARAM_HAS_IMAGEMAPS
//  PARAM_HAS_PASSTHROUGHT
//  PARAM_USE_PIXEL_ATOMICS
//  PARAM_HAS_BUMPMAPS
//  PARAM_HAS_NORMALMAPS
//  PARAM_ACCEL_BVH or PARAM_ACCEL_QBVH or PARAM_ACCEL_MQBVH

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

// (optional)
//  PARAM_DIRECT_LIGHT_SAMPLING
//  PARAM_DL_LIGHT_COUNT

// (optional)
//  PARAM_CAMERA_HAS_DOF

// (optional)
//  PARAM_HAS_INFINITELIGHT

// (optional, requires PARAM_DIRECT_LIGHT_SAMPLING)
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
//  PARAM_SAMPLER_TYPE (0 = Inlined Random, 1 = Metropolis)
// (Metropolis)
//  PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE
//  PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT
//  PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE

// TODO: IDX_BSDF_Z used only if needed

// (optional)
//  PARAM_ENABLE_ALPHA_CHANNEL

//------------------------------------------------------------------------------
// Init Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Init(
		uint seedBase,
		__global GPUTask *tasks,
		__global float *samplesData,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global Camera *camera
		) {
	const size_t gid = get_global_id(0);

	// Initialize the task
	__global GPUTask *task = &tasks[gid];
	__global Sample *sample = &task->sample;

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Initialize the sample
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);
	Sampler_Init(&seed, &task->sample, sampleData);

	// Initialize the path
	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	GenerateCameraPath(task, sampleDataPathBase, &seed, camera, &rays[gid]);

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

	__global Pixel *p = &frameBuffer[gid];
	p->c.r = 0.f;
	p->c.g = 0.f;
	p->c.b = 0.f;
	p->count = 0.f;

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	__global AlphaPixel *ap = &alphaFrameBuffer[gid];
	ap->alpha = 0.f;
#endif
}

//------------------------------------------------------------------------------
// AdvancePaths Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void AdvancePaths(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global float *samplesData,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
		__global Material *mats,
		__global Texture *texs,
		__global uint *meshMats,
		__global uint *meshIDs,
#if defined(PARAM_ACCEL_MQBVH)
		__global uint *meshFirstTriangleOffset,
		__global Mesh *meshDescs,
#endif
		__global Point *vertices,
		__global Vector *vertNormals,
		__global UV *vertUVs,
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
		, __global uint *meshLights
#endif
		IMAGEMAPS_PARAM_DECL
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
#endif
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];

//	if (gid == 0) {
//		// Statistic study for state reordering
//		// State averages for LuxBall HDR: 3.95 4.04 0
//		// State averages for LuxBall:     0.83 4.45 2.71
//		// State averages for Cornell:     1.69 3.35 2.95
//
//		uint RT_EYE_RAY_count = 0;
//		uint RT_NEXT_VERTEX_count = 0;
//		uint RT_DL_count = 0;
//		for (uint i = 0; i < 8; ++i) {
//			PathState s = tasks[i * 64].pathStateBase.state;
//			switch (s) {
//				case RT_EYE_RAY:
//					++RT_EYE_RAY_count;
//					break;
//				case RT_NEXT_VERTEX:
//					++RT_NEXT_VERTEX_count;
//					break;
//				case RT_DL:
//					++RT_DL_count;
//					break;
//			}
//		}
//
//		printf("%d %d %d\n", RT_EYE_RAY_count, RT_NEXT_VERTEX_count, RT_DL_count);
//	}

//	if (gid == 0) {
//		// Statistic study of work group state divergence
//		// State averages for LuxBall HDR: 49% 51% 0
//		// State averages for LuxBall:     11% 55% 34%
//		// State averages for Cornell:     21% 42% 37%
//		//
//		// Full state divergence in all scenes
//
//		bool noDivergence = true;
//		bool RT_EYE_RAY_state = false;
//		bool RT_NEXT_VERTEX_state = false;
//		bool RT_DL_state = false;
//		for (uint i = 0; i < 64; ++i) {
//			PathState s = tasks[i].pathStateBase.state;
//			switch (s) {
//				case RT_EYE_RAY:
//					RT_EYE_RAY_state = true;
//					noDivergence &= !RT_NEXT_VERTEX_state & !RT_DL_state;
//					break;
//				case RT_NEXT_VERTEX:
//					RT_NEXT_VERTEX_state = true;
//					noDivergence &= !RT_EYE_RAY_state & !RT_DL_state;
//					break;
//				case RT_DL:
//					RT_DL_state = true;
//					noDivergence &= !RT_EYE_RAY_state & !RT_NEXT_VERTEX;
//					break;
//			}
//		}
//
//		printf("%d\n", noDivergence ? 1 : 0);
//	}

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

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	const uint currentTriangleIndex = rayHit->index;

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_NEXT_VERTEX or RT_EYE_RAY
	// To: SPLAT_SAMPLE or GENERATE_DL_RAY
	//--------------------------------------------------------------------------

	if ((pathState == RT_NEXT_VERTEX) || (pathState == RT_EYE_RAY)) {
		if (currentTriangleIndex != NULL_INDEX) {
			// Something was hit

			BSDF_Init(bsdf,
#if defined(PARAM_ACCEL_MQBVH)
					meshFirstTriangleOffset,
					meshDescs,
#endif
					meshMats, meshIDs,
#if (PARAM_DL_LIGHT_COUNT > 0)
					meshLights,
#endif
					vertices, vertNormals, vertUVs,
					triangles, ray, rayHit
#if defined(PARAM_HAS_PASSTHROUGHT)
					, task->pathStateBase.bsdf.passThroughEvent
#endif
#if defined(PARAM_HAS_BUMPMAPS) || defined(PARAM_HAS_NORMALMAPS)
					MATERIALS_PARAM
					IMAGEMAPS_PARAM
#endif
					);

#if defined(PARAM_HAS_PASSTHROUGHT)
			const float3 passThroughTrans = BSDF_GetPassThroughTransparency(bsdf
					MATERIALS_PARAM
					IMAGEMAPS_PARAM);
			if (!Spectrum_IsBlack(passThroughTrans)) {
				const float3 pathThroughput = vload3(0, &task->pathStateBase.throughput.r) * passThroughTrans;
				vstore3(pathThroughput, 0, &task->pathStateBase.throughput.r);

				// It is a pass through point, continue to trace the ray
				ray->mint = rayHit->t + MachineEpsilon_E(rayHit->t);

				// Keep the same path state
			}
#endif
#if defined(PARAM_HAS_PASSTHROUGHT) && (PARAM_DL_LIGHT_COUNT > 0)
			else
#endif
			{
#if (PARAM_DL_LIGHT_COUNT > 0)
				// Check if it is a light source (note: I can hit only triangle area light sources)
				if (bsdf->triangleLightSourceIndex != NULL_INDEX) {
					float directPdfA;
					const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf, mats, texs,
							triLightDefs, &directPdfA
							IMAGEMAPS_PARAM);
					if (!Spectrum_IsBlack(emittedRadiance)) {
						// Add emitted radiance
						float weight = 1.f;
						if (!task->directLightState.lastSpecular) {
							const float lightPickProb = Scene_PickLightPdf();
							const float directPdfW = PdfAtoW(directPdfA, rayHit->t,
								fabs(dot(vload3(0, &bsdf->fixedDir.x), vload3(0, &bsdf->shadeN.x))));

							// MIS between BSDF sampling and direct light sampling
							weight = PowerHeuristic(task->directLightState.lastPdfW, directPdfW * lightPickProb);
						}

						float3 radiance = vload3(0, &sample->radiance.r);
						const float3 pathThroughput = vload3(0, &task->pathStateBase.throughput.r);
						const float3 le = pathThroughput * weight * emittedRadiance;
						radiance += le;
						vstore3(radiance, 0, &sample->radiance.r);
					}
				}
#endif

#if defined(PARAM_HAS_SUNLIGHT) || (PARAM_DL_LIGHT_COUNT > 0)
				// Direct light sampling
				pathState = GENERATE_DL_RAY;
#else
				// Sample next path vertex
				pathState = GENERATE_NEXT_VERTEX_RAY;
#endif
			}
		} else {
			//------------------------------------------------------------------
			// Nothing was hit, get environmental lights radiance
			//------------------------------------------------------------------
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_SUNLIGHT)
			float3 radiance = vload3(0, &sample->radiance.r);
			const float3 pathThroughput = vload3(0, &task->pathStateBase.throughput.r);
			const float3 dir = -vload3(0, &ray->d.x);
			float3 lightRadiance = BLACK;

#if defined(PARAM_HAS_INFINITELIGHT)
			lightRadiance += InfiniteLight_GetRadiance(infiniteLight, dir
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_HAS_SKYLIGHT)
			lightRadiance += SkyLight_GetRadiance(skyLight, dir);
#endif
#if defined(PARAM_HAS_SUNLIGHT)
			float directPdfW;
			const float3 sunRadiance = SunLight_GetRadiance(sunLight, dir, &directPdfW);
			if (!Spectrum_IsBlack(sunRadiance)) {
				// MIS between BSDF sampling and direct light sampling
				const float weight = (task->directLightState.lastSpecular ? 1.f : PowerHeuristic(task->directLightState.lastPdfW, directPdfW));
				lightRadiance += weight * sunRadiance;
			}
#endif

			radiance += pathThroughput * lightRadiance;
			vstore3(radiance, 0, &sample->radiance.r);
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

#if defined(PARAM_HAS_SUNLIGHT) || (PARAM_DL_LIGHT_COUNT > 0)
	if (pathState == RT_DL) {
		pathState = GENERATE_NEXT_VERTEX_RAY;

		if (currentTriangleIndex == NULL_INDEX) {
			// Nothing was hit, the light source is visible
			float3 radiance = vload3(0, &sample->radiance.r);
			const float3 lightRadiance = vload3(0, &task->directLightState.lightRadiance.r);
			radiance += lightRadiance;
			vstore3(radiance, 0, &sample->radiance.r);
		}
#if defined(PARAM_HAS_PASSTHROUGHT)
		else {
			BSDF_Init(&task->passThroughState.passThroughBsdf,
#if defined(PARAM_ACCEL_MQBVH)
					meshFirstTriangleOffset,
					meshDescs,
#endif
					meshMats, meshIDs,
#if (PARAM_DL_LIGHT_COUNT > 0)
					meshLights,
#endif
					vertices, vertNormals, vertUVs,
					triangles, ray, rayHit,
					task->passThroughState.passThroughEvent
#if defined(PARAM_HAS_BUMPMAPS) || defined(PARAM_HAS_NORMALMAPS)
					MATERIALS_PARAM
					IMAGEMAPS_PARAM
#endif
					);

			const float3 passthroughTrans = BSDF_GetPassThroughTransparency(&task->passThroughState.passThroughBsdf
					MATERIALS_PARAM
					IMAGEMAPS_PARAM);
			if (!Spectrum_IsBlack(passthroughTrans)) {
				const float3 lightRadiance = vload3(0, &task->directLightState.lightRadiance.r) * passthroughTrans;
				vstore3(lightRadiance, 0, &task->directLightState.lightRadiance.r);

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

#if defined(PARAM_HAS_SUNLIGHT) || (PARAM_DL_LIGHT_COUNT > 0)
	if (pathState == GENERATE_DL_RAY) {
		pathState = GENERATE_NEXT_VERTEX_RAY;

		if (!BSDF_IsDelta(bsdf
				MATERIALS_PARAM)) {
			// TODO: sunlight

			// Pick a triangle light source to sample
			const float lu0 = Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_X);
			const float lightPickPdf = Scene_PickLightPdf();

			float3 lightRayDir;
			float distance, directPdfW;
			float3 lightRadiance;
#if defined(PARAM_HAS_SUNLIGHT) && (PARAM_DL_LIGHT_COUNT > 0)
			const uint lightIndex = min((uint)floor((PARAM_DL_LIGHT_COUNT + 1) * lu0), (uint)(PARAM_DL_LIGHT_COUNT));

			if (lightIndex == PARAM_DL_LIGHT_COUNT) {
				lightRadiance = SunLight_Illuminate(
					sunLight,
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Z),
					&lightRayDir, &distance, &directPdfW);
			} else {
				lightRadiance = TriangleLight_Illuminate(
					&triLightDefs[lightIndex], vload3(0, &bsdf->hitPoint.x),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Z),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_W),
					&lightRayDir, &distance, &directPdfW
					MATERIALS_PARAM
					IMAGEMAPS_PARAM);
			}
			
#elif (PARAM_DL_LIGHT_COUNT > 0)
			const uint lightIndex = min((uint)floor(PARAM_DL_LIGHT_COUNT * lu0), (uint)(PARAM_DL_LIGHT_COUNT - 1));

			lightRadiance = TriangleLight_Illuminate(
					&triLightDefs[lightIndex], vload3(0, &bsdf->hitPoint.x),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Z),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_W),
					&lightRayDir, &distance, &directPdfW
					MATERIALS_PARAM
					IMAGEMAPS_PARAM);
#elif defined(PARAM_HAS_SUNLIGHT)
			lightRadiance = SunLight_Illuminate(
					sunLight,
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Y),
					Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_Z),
					&lightRayDir, &distance, &directPdfW);
#endif

			if (!Spectrum_IsBlack(lightRadiance)) {
				BSDFEvent event;
				float bsdfPdfW;
				const float3 bsdfEval = BSDF_Evaluate(bsdf,
						lightRayDir, &event, &bsdfPdfW
						MATERIALS_PARAM
						IMAGEMAPS_PARAM);

				if (!Spectrum_IsBlack(bsdfEval)) {
					const float3 pathThroughput = vload3(0, &task->pathStateBase.throughput.r);
					const float cosThetaToLight = fabs(dot(lightRayDir, vload3(0, &bsdf->shadeN.x)));
					const float directLightSamplingPdfW = directPdfW * lightPickPdf;
					const float factor = cosThetaToLight / directLightSamplingPdfW;

					// Russian Roulette
					bsdfPdfW *= (depth >= PARAM_RR_DEPTH) ? fmax(Spectrum_Filter(bsdfEval), PARAM_RR_CAP) : 1.f;

					// MIS between direct light sampling and BSDF sampling
					const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

					vstore3((weight * factor) * pathThroughput * bsdfEval * lightRadiance, 0, &task->directLightState.lightRadiance.r);
#if defined(PARAM_HAS_PASSTHROUGHT)
					task->passThroughState.passThroughEvent = Sampler_GetSamplePathVertex(IDX_DIRECTLIGHT_A);
#endif

					// Setup the shadow ray
					const float3 hitPoint = vload3(0, &bsdf->hitPoint.x);
					Ray_Init4(ray, hitPoint, lightRayDir,
						MachineEpsilon_E_Float3(hitPoint),
						distance - MachineEpsilon_E(distance));
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
					Sampler_GetSamplePathVertex(IDX_BSDF_X), Sampler_GetSamplePathVertex(IDX_BSDF_Y),
					&sampledDir, &lastPdfW, &cosSampledDir, &event
					MATERIALS_PARAM
					IMAGEMAPS_PARAM);
			const bool lastSpecular = ((event & SPECULAR) != 0);

			// Russian Roulette
			const float rrProb = fmax(Spectrum_Filter(bsdfSample), PARAM_RR_CAP);
			const bool rrEnabled = (depth >= PARAM_RR_DEPTH) && !lastSpecular;
			const bool rrContinuePath = !rrEnabled || (Sampler_GetSamplePathVertex(IDX_RR) < rrProb);

			const bool continuePath = !Spectrum_IsBlack(bsdfSample) && rrContinuePath;
			if (continuePath) {
				if (rrEnabled)
					lastPdfW *= rrProb; // Russian Roulette

				float3 throughput = vload3(0, &task->pathStateBase.throughput.r);
				throughput *= bsdfSample * (cosSampledDir / lastPdfW);
				vstore3(throughput, 0, &task->pathStateBase.throughput.r);

				Ray_Init2(ray, vload3(0, &bsdf->hitPoint.x), sampledDir);

				task->pathStateBase.depth = depth + 1;
#if defined(PARAM_HAS_SUNLIGHT) || (PARAM_DL_LIGHT_COUNT > 0)
				task->directLightState.lastPdfW = lastPdfW;
				task->directLightState.lastSpecular = lastSpecular;
#endif
#if defined(PARAM_HAS_PASSTHROUGHT)
				// This is a bit tricky. I store the passThroughEvent in the BSDF
				// before of the initialization because it can be use during the
				// tracing of next path vertex ray.

				// This sampleDataPathVertexBase is used inside Sampler_GetSamplePathVertex() macro
				__global float *sampleDataPathVertexBase = Sampler_GetSampleDataPathVertex(
					sample, sampleDataPathBase, depth + 1);
				task->pathStateBase.bsdf.passThroughEvent = Sampler_GetSamplePathVertex(IDX_PASSTROUGHT);
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
	// To: RT_EYE_RAY
	//--------------------------------------------------------------------------

	if (pathState == SPLAT_SAMPLE) {
		Sampler_NextSample(task, sample, sampleData, seed, frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				alphaFrameBuffer,
#endif
				camera, ray);
		taskStats[gid].sampleCount += 1;

		// task->pathStateBase.state is set to RT_EYE_RAY inside Sampler_NextSample() => GenerateCameraPath()
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
