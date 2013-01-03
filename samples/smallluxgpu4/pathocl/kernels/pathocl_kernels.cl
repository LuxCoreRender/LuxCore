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

// To enable single material support (work around for ATI compiler problems)
//  PARAM_ENABLE_MAT_MATTE
//  PARAM_ENABLE_MAT_AREALIGHT
//  PARAM_ENABLE_MAT_MIRROR
//  PARAM_ENABLE_MAT_GLASS
//  PARAM_ENABLE_MAT_MATTEMIRROR
//  PARAM_ENABLE_MAT_METAL
//  PARAM_ENABLE_MAT_MATTEMETAL
//  PARAM_ENABLE_MAT_ALLOY
//  PARAM_ENABLE_MAT_ARCHGLASS

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

	//if (gid == 0)
	//	printf("GPUTask: %d\n", sizeof(GPUTask));

	// Initialize the task
	__global GPUTask *task = &tasks[gid];
	__global Sample *sample = &task->sample;
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Initialize the sample
	Sampler_Init(&seed, &task->sample, sampleData);

	// Initialize the path
	GenerateCameraPath(task, sampleData,
#if (PARAM_SAMPLER_TYPE == 0)
		&seed,
#endif
		camera, &rays[gid]);

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
		__global Vector *vertNormals,
		__global Point *vertices,
		__global Triangle *triangles,
		__global Camera *camera
#if defined(PARAM_HAS_INFINITELIGHT)
		, __global InfiniteLight *infiniteLight
#endif
#if defined(PARAM_HAS_IMAGEMAPS)
		, __global ImageMap *imageMapDescs
#if defined(PARAM_IMAGEMAPS_PAGE_0)
		, __global float *imageMapBuff0
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
		, __global UV *vertUVs
#endif
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
#endif
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	__global Sample *sample = &task->sample;
	__global float *sampleData = Sampler_GetSampleData(sample, samplesData);

	// Read the seed
	Seed seedValue;
	seedValue.s1 = task->seed.s1;
	seedValue.s2 = task->seed.s2;
	seedValue.s3 = task->seed.s3;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	// read the path state
	PathState pathState = task->pathStateBase.state;

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	const uint currentTriangleIndex = rayHit->index;

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: RT_NEXT_VERTEX
	// To: SPLAT_SAMPLE or GENERATE_DL_RAY
	//--------------------------------------------------------------------------

	if (pathState == RT_NEXT_VERTEX) {
		if (currentTriangleIndex != 0xffffffffu) {
			// Something was hit

			__global BSDF *bsdf = &task->pathStateBase.bsdf;
			BSDF_Init(bsdf,
#if defined(PARAM_ACCEL_MQBVH)
					meshFirstTriangleOffset,
					meshDescs,
#endif
					mats, texs, meshMats, meshIDs, vertNormals, vertices,
					triangles, ray, rayHit
#if defined(PARAM_HAS_PASSTHROUGHT)
					, Sampler_GetSample(IDX_PASSTROUGHT)
#endif
					);

			// Sample next path vertex
			pathState = GENERATE_NEXT_VERTEX_RAY;
		} else {
#if defined(PARAM_HAS_INFINITELIGHT)
			const float3 lightRadiance = InfiniteLight_GetRadiance(
				infiniteLight,
#if defined(PARAM_HAS_IMAGEMAPS)
				imageMapDescs,
#if defined(PARAM_IMAGEMAPS_PAGE_0)
				imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
				imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
				imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
				imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
				imageMapBuff4,
#endif
#endif
				-vload3(0, &ray->d.x));
			float3 radiance = vload3(0, &sample->radiance.r);
			radiance += vload3(0, &task->pathStateBase.throughput.r) * lightRadiance;
			vstore3(radiance, 0, &sample->radiance.r);
#endif
			pathState = SPLAT_SAMPLE;
		}
	}

	//--------------------------------------------------------------------------
	// Evaluation of the Path finite state machine.
	//
	// From: GENERATE_NEXT_VERTEX_RAY
	// To: SPLAT_SAMPLE or RT_NEXT_VERTEX
	//--------------------------------------------------------------------------

	if (pathState == GENERATE_NEXT_VERTEX_RAY) {
		uint depth = task->pathStateBase.depth;

		if (depth < PARAM_MAX_PATH_DEPTH) {
			// Sample the BSDF
			__global BSDF *bsdf = &task->pathStateBase.bsdf;
			float3 sampledDir;
			float lastPdfW;
			float cosSampledDir;
			BSDFEvent event;

			const float3 bsdfSample = BSDF_Sample(bsdf, mats, texs,
#if defined(PARAM_HAS_IMAGEMAPS)
					imageMapDescs,
#if defined(PARAM_IMAGEMAPS_PAGE_0)
					imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
					imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
					imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
					imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
					imageMapBuff4,
#endif
#endif
					Sampler_GetSample(IDX_BSDF_X), Sampler_GetSample(IDX_BSDF_Y),
					&sampledDir, &lastPdfW, &cosSampledDir, &event);
			const bool lastSpecular = ((event & SPECULAR) != 0);

			// Russian Roulette
			const float rrProb = fmax(Spectrum_Filter(bsdfSample), PARAM_RR_CAP);
			const bool rrEnabled = (depth >= PARAM_RR_DEPTH) && !lastSpecular;
			const bool rrContinuePath = !rrEnabled || (Sampler_GetSample(IDX_RR) < rrProb);

			const bool continuePath = !all(isequal(bsdfSample, BLACK)) && rrContinuePath;
			if (continuePath) {
				if (rrEnabled)
					lastPdfW *= rrProb; // Russian Roulette

				float3 throughput = vload3(0, &task->pathStateBase.throughput.r);
				throughput *= bsdfSample * (cosSampledDir / lastPdfW);
				vstore3(throughput, 0, &task->pathStateBase.throughput.r);

				Ray_Init2(ray, vload3(0, &bsdf->hitPoint.x), sampledDir);

				task->pathStateBase.depth = depth + 1;
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
		Sampler_NextSample(task, sample, sampleData, seed, frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				alphaFrameBuffer,
#endif
				camera, ray);
		taskStats[gid].sampleCount += 1;

		pathState = RT_NEXT_VERTEX;
	}

	//--------------------------------------------------------------------------

	// Save the seed
	task->seed.s1 = seed->s1;
	task->seed.s2 = seed->s2;
	task->seed.s3 = seed->s3;

	// Save the state
	task->pathStateBase.state = pathState;
}
