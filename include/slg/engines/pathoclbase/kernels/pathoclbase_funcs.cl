#line 2 "pathoclbase_funcs.cl"

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

// List of symbols defined at compile time:
//  PARAM_RAY_EPSILON_MIN
//  PARAM_RAY_EPSILON_MAX

/*void MangleMemory(__global unsigned char *ptr, const size_t size) {
	Seed seed;
	Rnd_Init(7 + get_global_id(0), &seed);

	for (uint i = 0; i < size; ++i)
		*ptr++ = (unsigned char)(Rnd_UintValue(&seed) & 0xff);
}*/

//------------------------------------------------------------------------------
// Path state and work queues
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE int PathState_QueueIndex(const PathState state) {
	switch (state) {
		case MK_HIT_NOTHING:
			return PATH_QUEUE_HIT_NOTHING;
		case MK_HIT_OBJECT:
			return PATH_QUEUE_HIT_OBJECT;
		case MK_DL_ILLUMINATE:
			return PATH_QUEUE_DL_ILLUMINATE;
		case MK_DL_SAMPLE_BSDF:
			return PATH_QUEUE_DL_SAMPLE_BSDF;
		case MK_GENERATE_NEXT_VERTEX_RAY:
			return PATH_QUEUE_GENERATE_NEXT_VERTEX_RAY;
		case MK_SPLAT_SAMPLE:
			return PATH_QUEUE_SPLAT_SAMPLE;
		case MK_NEXT_SAMPLE:
			return PATH_QUEUE_NEXT_SAMPLE;
		case MK_GENERATE_CAMERA_RAY:
			return PATH_QUEUE_GENERATE_CAMERA_RAY;
		case MK_RT_NEXT_VERTEX:
			return PATH_QUEUE_RT_NEXT_VERTEX;
		case MK_RT_DL:
			return PATH_QUEUE_RT_DL;
		default:
			// MK_DONE has no queue
			return -1;
	}
}

// Returns the position of a new entry in the queue
OPENCL_FORCE_INLINE uint PathQueue_Reserve(__global uint *pathQueueCounts, const int queueIndex) {
#if defined(LUXRAYS_CUDA_DEVICE) && (__CUDA_ARCH__ >= 700)
	// One atomic for all the active threads of the warp pushing to the same
	// queue instead of one per thread
	const unsigned int peers = __match_any_sync(__activemask(), queueIndex);
	unsigned int lane;
	asm("mov.u32 %0, %%laneid;" : "=r"(lane));
	const int leader = __ffs(peers) - 1;

	unsigned int base = 0;
	if (lane == leader)
		base = atomicAdd(&pathQueueCounts[queueIndex], __popc(peers));
	base = __shfl_sync(peers, base, leader);

	return base + __popc(peers & ((1u << lane) - 1u));
#else
	return atomic_inc(&pathQueueCounts[queueIndex]);
#endif
}

// Sets the state of path gid: QueuePaths or the material sort
// (QueuePathsByMaterial) queues it for the micro-kernel of that state
#define PATH_STATE_SET(state) pathStates[gid] = (state)

// Used by the micro-kernels with a queue: one thread for each queued path.
// Kernels are launched with taskCount threads, the threads past the number of
// queued paths exit at once.
#define PATH_QUEUE_GET_GID(queueIndex) \
	if (get_global_id(0) >= pathQueueCounts[queueIndex]) \
		return; \
	const size_t gid = pathQueues[(queueIndex) * get_global_size(0) + get_global_id(0)]

__kernel void ResetPathQueues(__global uint *pathQueueCounts) {
	const size_t gid = get_global_id(0);
	if (gid < PATH_QUEUE_COUNT)
		pathQueueCounts[gid] = 0;
}

// Queues the paths in the given state for its micro-kernel, run right before
// it. Scanning the states keeps each queue in ascending path order (the
// threads of a warp append consecutive paths), so the warps of the
// micro-kernel access nearby paths and their rays, hits and sample results
// share cache sectors. Pushing in PATH_STATE_SET() gives the queues in the
// order the atomics arrive.
__kernel void QueuePaths(__global PathState *pathStates,
		__global uint *pathQueues, __global uint *pathQueueCounts,
		const uint state) {
	const size_t gid = get_global_id(0);

	if (pathStates[gid] == state) {
		const int queueIndex = PathState_QueueIndex((PathState)state);
		const uint pos = PathQueue_Reserve(pathQueueCounts, queueIndex);
		pathQueues[queueIndex * get_global_size(0) + pos] = gid;
	}
}

//------------------------------------------------------------------------------
// Material sorted queues
//
// For the micro-kernels evaluating the BSDF, the paths can be queued grouped
// by the material of their BSDF (a counting sort): the warps mostly run the
// code of one material, with less divergence. The paths in a state are the
// same as with QueuePaths, only their order changes.
// materialCounts and materialCursors have materialCount + 1 entries (the
// last one for an out of range index). Launch order: CountPathsByMaterial,
// ScanPathsByMaterial, QueuePathsByMaterial.
//------------------------------------------------------------------------------

// Also stores the material of each path in the state in materialKeys, so
// QueuePathsByMaterial reads a path order array instead of the BSDF again
__kernel void CountPathsByMaterial(__global PathState *pathStates,
		__global const GPUTaskState *tasksState, __global uint *materialKeys,
		__global uint *materialCounts,
		const uint state, const uint materialCount) {
	const size_t gid = get_global_id(0);

	if (pathStates[gid] == state) {
		const uint key = min(tasksState[gid].bsdf.materialIndex, materialCount);
		materialKeys[gid] = key;
		PathQueue_Reserve(materialCounts, key);
	}
}

// Run by a single work-item: turns the counts into the position of the first
// path of each material in the queue, clears the counts for the next sort and
// sets the size of the queue
__kernel void ScanPathsByMaterial(__global uint *materialCounts,
		__global uint *materialCursors, __global uint *pathQueueCounts,
		const uint state, const uint materialCount) {
	if (get_global_id(0) != 0)
		return;

	uint sum = 0;
	for (uint i = 0; i <= materialCount; ++i) {
		const uint count = materialCounts[i];
		materialCursors[i] = sum;
		materialCounts[i] = 0;
		sum += count;
	}
	pathQueueCounts[PathState_QueueIndex((PathState)state)] = sum;
}

__kernel void QueuePathsByMaterial(__global PathState *pathStates,
		__global const uint *materialKeys, __global uint *pathQueues,
		__global uint *materialCursors,
		const uint state) {
	const size_t gid = get_global_id(0);

	if (pathStates[gid] == state) {
		const uint pos = PathQueue_Reserve(materialCursors, materialKeys[gid]);
		pathQueues[PathState_QueueIndex((PathState)state) * get_global_size(0) + pos] = gid;
	}
}

//------------------------------------------------------------------------------
// Init functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void InitSampleResult(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		__global float *pixelFilterDistribution
		SAMPLER_PARAM_DECL) {
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	SampleResult_Init(&taskConfig->film, sampleResult);

	float filmX = Sampler_GetSample(taskConfig, IDX_SCREEN_X SAMPLER_PARAM);
	float filmY = Sampler_GetSample(taskConfig, IDX_SCREEN_Y SAMPLER_PARAM);

	// Metropolis return IDX_SCREEN_X and IDX_SCREEN_Y between [0.0, 1.0] instead
	// that in film pixels like RANDOM and SOBOL samplers
	if (taskConfig->sampler.type == METROPOLIS) {
		filmX = filmSubRegion0 + filmX * (filmSubRegion1 - filmSubRegion0 + 1);
		filmY = filmSubRegion2 + filmY * (filmSubRegion3 - filmSubRegion2 + 1);
	}

	const uint pixelX = min(Floor2UInt(filmX), filmSubRegion1);
	const uint pixelY = min(Floor2UInt(filmY), filmSubRegion3);
	const float uSubPixelX = filmX - pixelX;
	const float uSubPixelY = filmY - pixelY;

	sampleResult->pixelX = pixelX;
	sampleResult->pixelY = pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(&taskConfig->pixelFilter, pixelFilterDistribution,
			uSubPixelX, uSubPixelY, &distX, &distY);

	sampleResult->filmX = pixelX + .5f + distX;
	sampleResult->filmY = pixelY + .5f + distY;

	sampleResult->directShadowMask = 1.f;
	sampleResult->indirectShadowMask = 1.f;

	sampleResult->lastPathVertex = (taskConfig->pathTracer.maxPathDepth.depth == 1);
}

OPENCL_FORCE_INLINE void GenerateEyePath(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global GPUTaskDirectLight *taskDirectLight,
		__global GPUTaskState *taskState,
		__global const Camera* restrict camera,
		__global const float* restrict cameraBokehDistribution,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		__global float *pixelFilterDistribution,
		__global Ray *ray,
		__global EyePathInfo *pathInfo
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		// cameraFilmWidth/cameraFilmHeight and filmWidth/filmHeight are usually
		// the same. They are different when doing tile rendering
		, const uint cameraFilmWidth, const uint cameraFilmHeight,
		const uint tileStartX, const uint tileStartY
#endif
		SAMPLER_PARAM_DECL) {
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	EyePathInfo_Init(pathInfo);

	InitSampleResult(taskConfig,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1,
			filmSubRegion2, filmSubRegion3,
			pixelFilterDistribution
			SAMPLER_PARAM);

	// Generate the came ray
	const float timeSample = Sampler_GetSample(taskConfig, IDX_EYE_TIME SAMPLER_PARAM);

	const float dofSampleX = Sampler_GetSample(taskConfig, IDX_DOF_X SAMPLER_PARAM);
	const float dofSampleY = Sampler_GetSample(taskConfig, IDX_DOF_Y SAMPLER_PARAM);

#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
	Camera_GenerateRay(camera, cameraBokehDistribution, cameraFilmWidth, cameraFilmHeight,
			ray,
			&pathInfo->volume,
			sampleResult->filmX + tileStartX, sampleResult->filmY + tileStartY,
			timeSample,
			dofSampleX, dofSampleY);
#else
	Camera_GenerateRay(camera, cameraBokehDistribution, filmWidth, filmHeight,
			ray,
			&pathInfo->volume,
			sampleResult->filmX, sampleResult->filmY,
			timeSample,
			dofSampleX, dofSampleY);
#endif

	// Initialize the path state (the caller sets pathStates[gid] to
	// MK_RT_NEXT_VERTEX)
	VSTORE3F(WHITE, taskState->throughput.c);
	taskState->albedoToDo = true;
	VSTORE3F(BLACK, sampleResult->albedo.c);  // Just in case albedoToDo is never true
	VSTORE3F(BLACK, &sampleResult->shadingNormal.x);
	taskState->photonGICacheEnabledOnLastHit = false;
	taskState->photonGICausticCacheUsed = false;
	taskState->photonGIShowIndirectPathMixUsed = false;
	// Initialize the trough a shadow transparency flag used by Scene_Intersect()
	taskState->throughShadowTransparency = false;

	// Initialize the pass-through event seed
	//
	// Note: using the IDX_PASSTHROUGH of path depth 0
	const float passThroughEvent = Sampler_GetSample(taskConfig, IDX_BSDF_OFFSET + IDX_PASSTHROUGH SAMPLER_PARAM);
	Seed seedPassThroughEvent;
	Rnd_InitFloat(passThroughEvent, &seedPassThroughEvent);
	taskState->seedPassThroughEvent = seedPassThroughEvent;
}

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE bool CheckDirectHitVisibilityFlags(__global const LightSource* restrict lightSource,
		__global PathDepthInfo *depthInfo,
		const BSDFEvent lastBSDFEvent) {
	if (depthInfo->depth == 0)
		return true;

	if ((lastBSDFEvent & DIFFUSE) && (lightSource->visibility & DIFFUSE))
		return true;
	if ((lastBSDFEvent & GLOSSY) && (lightSource->visibility & GLOSSY))
		return true;
	if ((lastBSDFEvent & SPECULAR) && (lightSource->visibility & SPECULAR))
		return true;

	return false;
}

OPENCL_FORCE_INLINE void DirectHitInfiniteLight(__constant const Film* restrict film,
		__global EyePathInfo *pathInfo, __global const Spectrum* restrict pathThroughput,
		const __global Ray *ray, __global const BSDF *bsdf, __global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	// If the material is shadow transparent, Direct Light sampling
	// will take care of transporting all emitted light
	if (bsdf && HitPoint_IsThroughShadowTransparency(&bsdf->hitPoint))
		return;

	const float3 throughput = VLOAD3F(pathThroughput->c);

	for (uint i = 0; i < envLightCount; ++i) {
		__global const LightSource* restrict light = &lights[envLightIndices[i]];

		// Check if the light source is visible according the settings
		if (!CheckDirectHitVisibilityFlags(light, &pathInfo->depth, pathInfo->lastBSDFEvent))
			continue;

		float directPdfW;
		const float3 envRadiance = EnvLight_GetRadiance(light, bsdf,
				-VLOAD3F(&ray->d.x), &directPdfW
				LIGHTS_PARAM);

		if (!Spectrum_IsBlack(envRadiance)) {
			float weight;
			if (!(pathInfo->lastBSDFEvent & SPECULAR)) {
				const float lightPickProb = LightStrategy_SampleLightPdf(lightsDistribution,
						dlscAllEntries,
						dlscDistributions, dlscBVHNodes,
						dlscRadius2, dlscNormalCosAngle,
						VLOAD3F(&ray->o.x), VLOAD3F(&pathInfo->lastShadeN.x),
						pathInfo->lastFromVolume,
						light->lightSceneIndex);

				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(pathInfo->lastBSDFPdfW, directPdfW * lightPickProb);
			} else
				weight = 1.f;
			
			SampleResult_AddEmission(film, sampleResult, light->lightID, throughput, weight * envRadiance);
		}
	}
}

OPENCL_FORCE_INLINE void DirectHitFiniteLight(__constant const Film* restrict film,
		__global EyePathInfo *pathInfo,
		__global const Spectrum* restrict pathThroughput, const __global Ray *ray,
		const float distance, __global const BSDF *bsdf,
		__global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	__global const LightSource* restrict light = &lights[bsdf->triangleLightSourceIndex];

	// Check if the light source is visible according the settings
	if (!CheckDirectHitVisibilityFlags(light, &pathInfo->depth, pathInfo->lastBSDFEvent) ||
			// If the material is shadow transparent, Direct Light sampling
			// will take care of transporting all emitted light
			HitPoint_IsThroughShadowTransparency(&bsdf->hitPoint))
		return;
	
	float directPdfA;
	const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf, &directPdfA
			LIGHTS_PARAM);

	if (!Spectrum_IsBlack(emittedRadiance)) {
		// Add emitted radiance
		float weight = 1.f;
		if (!(pathInfo->lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = LightStrategy_SampleLightPdf(lightsDistribution,
					dlscAllEntries,
					dlscDistributions, dlscBVHNodes,
					dlscRadius2, dlscNormalCosAngle,
					VLOAD3F(&ray->o.x), VLOAD3F(&pathInfo->lastShadeN.x),
					pathInfo->lastFromVolume,
					light->lightSceneIndex);

#if !defined(RENDER_ENGINE_RTPATHOCL)
			// This is a specific check to avoid fireflies with DLSC
			if ((lightPickProb == 0.f) && light->isDirectLightSamplingEnabled && dlscAllEntries)
				return;
#endif
			
			const float directPdfW = PdfAtoW(directPdfA, distance,
					fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

			// MIS between BSDF sampling and direct light sampling
			//
			// Note: mats[bsdf->materialIndex].avgPassThroughTransparency = lightSource->GetAvgPassThroughTransparency()
			weight = PowerHeuristic(pathInfo->lastBSDFPdfW * Light_GetAvgPassThroughTransparency(light LIGHTS_PARAM), directPdfW * lightPickProb);
		}

		SampleResult_AddEmission(film, sampleResult, BSDF_GetLightID(bsdf
				MATERIALS_PARAM), VLOAD3F(pathThroughput->c), weight * emittedRadiance);
	}
}

OPENCL_FORCE_INLINE float RussianRouletteProb(const float importanceCap, const float3 color) {
	return clamp(Spectrum_Filter(color), importanceCap, 1.f);
}

OPENCL_FORCE_INLINE bool DirectLight_Illuminate(
		__global const BSDF *bsdf,
		__global Ray *shadowRay,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		__global HitPoint *tmpHitPoint,
		const float time, const float u0, const float u1, const float u2,
		const float lightPassThroughEvent,
		__global DirectLightIlluminateInfo *info
		LIGHTS_PARAM_DECL) {
	// Select the light strategy to use
	__global const float* restrict lightDist = BSDF_IsShadowCatcherOnlyInfiniteLights(bsdf MATERIALS_PARAM) ?
		infiniteLightSourcesDistribution : lightsDistribution;

	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = LightStrategy_SampleLights(lightDist,
			dlscAllEntries,
			dlscDistributions, dlscBVHNodes,
			dlscRadius2, dlscNormalCosAngle,
			VLOAD3F(&bsdf->hitPoint.p.x), BSDF_GetLandingGeometryN(bsdf), 
			bsdf->isVolume,
			u0, &lightPickPdf);
	if ((lightIndex == NULL_INDEX) || (lightPickPdf <= 0.f))
		return false;

	__global const LightSource* restrict light = &lights[lightIndex];

	info->lightIndex = lightIndex;
	info->lightID = light->lightID;
	info->pickPdf = lightPickPdf;

	// Illuminate the point
	float directPdfW;
	const float3 lightRadiance = Light_Illuminate(
			&lights[lightIndex],
			bsdf,
			time, u1, u2,
			lightPassThroughEvent,
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			tmpHitPoint,		
			shadowRay, &directPdfW
			LIGHTS_PARAM);
	
	if (Spectrum_IsBlack(lightRadiance))
		return false;
	else {
		info->directPdfW = directPdfW;
		VSTORE3F(lightRadiance, info->lightRadiance.c);
		VSTORE3F(lightRadiance, info->lightIrradiance.c);
		return true;
	}
}

OPENCL_FORCE_INLINE bool DirectLight_BSDFSampling(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global DirectLightIlluminateInfo *info,
		const float time,
		const bool lastPathVertex,
		__global EyePathInfo *pathInfo,
		__global PathDepthInfo *tmpDepthInfo,
		__global const BSDF *bsdf,
		const float3 shadowRayDir
		PATHGUIDING_CDF_PARAM_DECL
		LIGHTS_PARAM_DECL) {
	// Sample the BSDF
	BSDFEvent event;
	float bsdfPdfW;
	const float3 bsdfEval = BSDF_Evaluate(bsdf,
			shadowRayDir, &event, &bsdfPdfW
			MATERIALS_PARAM);
#if defined(PARAM_PATH_GUIDING)
	// A guided vertex samples its next direction with the mix of the BSDF
	// and the guide: the MIS weight uses the pdf of the mix
	if (guidingCDF)
		bsdfPdfW = PathGuiding_MISPdf(bsdf, guidingCDF, shadowRayDir, bsdfPdfW
				MATERIALS_PARAM);
#endif

	if (Spectrum_IsBlack(bsdfEval) ||
			(taskConfig->pathTracer.hybridBackForward.enabled &&
			EyePathInfo_IsCausticPathWithEvent(pathInfo, event,
				BSDF_GetGlossiness(bsdf MATERIALS_PARAM),
				taskConfig->pathTracer.hybridBackForward.glossinessThreshold))
			)
		return false;

	// Create a new DepthInfo for the path to the light source
	//
	// Note: I was using a local variable before to save, use and than restore
	// the depthInfo variable but it was triggering a AMD OpenCL compiler bug.
	*tmpDepthInfo = pathInfo->depth;
	PathDepthInfo_IncDepths(tmpDepthInfo, event);

	const float directLightSamplingPdfW = info->directPdfW * info->pickPdf;
	const float factor = 1.f / directLightSamplingPdfW;

	// Russian Roulette
	bsdfPdfW *= (PathDepthInfo_GetRRDepth(tmpDepthInfo) >= taskConfig->pathTracer.rrDepth) ?
		RussianRouletteProb(taskConfig->pathTracer.rrImportanceCap, bsdfEval) :
		1.f;

	// Account for material transparency
	__global const LightSource* restrict light = &lights[info->lightIndex];
	bsdfPdfW *= Light_GetAvgPassThroughTransparency(light
			LIGHTS_PARAM);
	
	// MIS between direct light sampling and BSDF sampling
	//
	// Note: I have to avoiding MIS on the last path vertex

	const bool misEnabled = !lastPathVertex &&
			Light_IsEnvOrIntersectable(light) &&
			CheckDirectHitVisibilityFlags(light, tmpDepthInfo, event) &&
			!HitPoint_IsThroughShadowTransparency(&bsdf->hitPoint);

	const float weight = misEnabled ? PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

	const float3 lightRadiance = VLOAD3F(info->lightRadiance.c);
	VSTORE3F(bsdfEval * (weight * factor) * lightRadiance, info->lightRadiance.c);
	VSTORE3F(factor * lightRadiance, info->lightIrradiance.c);

	return true;
}

//------------------------------------------------------------------------------
// Kernel parameters
//------------------------------------------------------------------------------

#define KERNEL_ARGS_VOLUMES \
		, __global PathVolumeInfo *directLightVolInfos

#define KERNEL_ARGS_INFINITELIGHTS \
		, const float worldCenterX \
		, const float worldCenterY \
		, const float worldCenterZ \
		, const float worldRadius

#define KERNEL_ARGS_NORMALS_BUFFER \
		, __global const Normal* restrict vertNormals
#define KERNEL_ARGS_TRINORMALS_BUFFER \
		, __global const Normal* restrict triNormals
#define KERNEL_ARGS_UVS_BUFFER \
		, __global const UV* restrict vertUVs
#define KERNEL_ARGS_COLS_BUFFER \
		, __global const Spectrum* restrict vertCols
#define KERNEL_ARGS_ALPHAS_BUFFER \
		, __global const float* restrict vertAlphas
#define KERNEL_ARGS_VERTEXAOVS_BUFFER \
		, __global const float* restrict vertexAOVs
#define KERNEL_ARGS_TRIAOVS_BUFFER \
		, __global const float* restrict triAOVs

#define KERNEL_ARGS_ENVLIGHTS \
		, __global const uint* restrict envLightIndices \
		, const uint envLightCount

#define KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict envLightDistribution

#define KERNEL_ARGS_IMAGEMAPS_PAGES \
		, __global const ImageMap* restrict imageMapDescs \
		, __global const float* restrict imageMapBuff0 \
		, __global const float* restrict imageMapBuff1 \
		, __global const float* restrict imageMapBuff2 \
		, __global const float* restrict imageMapBuff3 \
		, __global const float* restrict imageMapBuff4 \
		, __global const float* restrict imageMapBuff5 \
		, __global const float* restrict imageMapBuff6 \
		, __global const float* restrict imageMapBuff7

#define KERNEL_ARGS_FAST_PIXEL_FILTER \
		, __global float *pixelFilterDistribution

#define KERNEL_ARGS_PHOTONGI \
		, __global const RadiancePhoton* restrict pgicRadiancePhotons \
		, uint pgicLightGroupCounts \
		, __global const Spectrum* restrict pgicRadiancePhotonsValues \
		, __global const IndexBVHArrayNode* restrict pgicRadiancePhotonsBVHNodes \
		, __global const Photon* restrict pgicCausticPhotons \
		, __global const IndexBVHArrayNode* restrict pgicCausticPhotonsBVHNodes

// Path guiding buffers (see pathguiding_funcs.cl), NULL without
// PARAM_PATH_GUIDING
#define KERNEL_ARGS_PATHGUIDING \
		, __global uint *guidingCellKeys \
		, __global float *guidingTrainBins \
		, __global uint *guidingTrainCounts \
		, __global float *guidingCDFs \
		, __global PathGuidingRecord *guidingRecords \
		, __global PathGuidingPathState *guidingPathStates \
		, __global uint *guidingState

#define KERNEL_ARGS \
		__constant const GPUTaskConfiguration* restrict taskConfig \
		, __global GPUTask *tasks \
		, __global GPUTaskDirectLight *tasksDirectLight \
		, __global GPUTaskState *tasksState \
		, __global PathState *pathStates \
		, __global uint *pathQueues \
		, __global uint *pathQueueCounts \
		, __global GPUTaskStats *taskStats \
		KERNEL_ARGS_FAST_PIXEL_FILTER \
		, __global void *samplerSharedDataBuff \
		, __global void *samplesBuff \
		, __global float *samplesDataBuff \
		, __global SampleResult *sampleResultsBuff \
		, __global EyePathInfo *eyePathInfos \
		KERNEL_ARGS_VOLUMES \
		, __global Ray *rays \
		, __global RayHit *rayHits \
		/* Film parameters */ \
		KERNEL_ARGS_FILM \
		/* Scene parameters */ \
		KERNEL_ARGS_INFINITELIGHTS \
		, __global const Material* restrict mats \
		, __global const MaterialEvalOp* restrict matEvalOps \
		, __global float *matEvalStacks \
		, const uint maxMaterialEvalStackSize \
		, __global const Texture* restrict texs \
		, __global const TextureEvalOp* restrict texEvalOps \
		, __global float *texEvalStacks \
		, const uint maxTextureEvalStackSize \
		, __global const SceneObject* restrict sceneObjs \
		, __global const ExtMesh* restrict meshDescs \
		, __global const Point* restrict vertices \
		KERNEL_ARGS_NORMALS_BUFFER \
		KERNEL_ARGS_TRINORMALS_BUFFER \
		KERNEL_ARGS_UVS_BUFFER \
		KERNEL_ARGS_COLS_BUFFER \
		KERNEL_ARGS_ALPHAS_BUFFER \
		KERNEL_ARGS_VERTEXAOVS_BUFFER \
		KERNEL_ARGS_TRIAOVS_BUFFER \
		, __global const Triangle* restrict triangles \
		, __global const InterpolatedTransform* restrict interpolatedTransforms \
		, __global const Camera* restrict camera \
		, __global const float* restrict cameraBokehDistribution \
		/* Lights */ \
		, __global const LightSource* restrict lights \
		KERNEL_ARGS_ENVLIGHTS \
		, __global const uint* restrict lightIndexOffsetByMeshIndex \
		, __global const uint* restrict lightIndexByTriIndex \
		KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict lightsDistribution \
		, __global const float* restrict infiniteLightSourcesDistribution \
		, __global const DLSCacheEntry* restrict dlscAllEntries \
		, __global const float* restrict dlscDistributions \
		, __global const IndexBVHArrayNode* restrict dlscBVHNodes \
		, const float dlscRadius2 \
		, const float dlscNormalCosAngle \
		, __global const ELVCacheEntry* restrict elvcAllEntries \
		, __global const float* restrict elvcDistributions \
		, __global const uint* restrict elvcTileDistributionOffsets \
		, __global const IndexBVHArrayNode* restrict elvcBVHNodes \
		, const float elvcRadius2 \
		, const float elvcNormalCosAngle \
		, const uint elvcTilesXCount \
		, const uint elvcTilesYCount \
		/* Images */ \
		KERNEL_ARGS_IMAGEMAPS_PAGES \
		KERNEL_ARGS_PHOTONGI \
		KERNEL_ARGS_PATHGUIDING


//------------------------------------------------------------------------------
// To initialize image maps page pointer table
//------------------------------------------------------------------------------

#define INIT_IMAGEMAPS_PAGES \
	__global const float* restrict imageMapBuff[8]; \
	imageMapBuff[0] = imageMapBuff0; \
	imageMapBuff[1] = imageMapBuff1; \
	imageMapBuff[2] = imageMapBuff2; \
	imageMapBuff[3] = imageMapBuff3; \
	imageMapBuff[4] = imageMapBuff4; \
	imageMapBuff[5] = imageMapBuff5; \
	imageMapBuff[6] = imageMapBuff6; \
	imageMapBuff[7] = imageMapBuff7;

//------------------------------------------------------------------------------
// Init Kernels
//------------------------------------------------------------------------------

__kernel void InitSeed(__global GPUTask *tasks,
		const uint seedBase) {
	const size_t gid = get_global_id(0);

	// Initialize random number generator

	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Save the seed
	__global GPUTask *task = &tasks[gid];
	task->seed = seed;
}

__kernel void Init(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global GPUTask *tasks,
		__global GPUTaskDirectLight *tasksDirectLight,
		__global GPUTaskState *tasksState,
		__global PathState *pathStates,
		__global uint *pathQueues,
		__global uint *pathQueueCounts,
		__global GPUTaskStats *taskStats,
		__global void *samplerSharedDataBuff,
		__global void *samplesBuff,
		__global float *samplesDataBuff,
		__global SampleResult *sampleResultsBuff,
		__global EyePathInfo *eyePathInfos,
		__global float *pixelFilterDistribution,
		__global Ray *rays,
		__global Camera *camera,
		__global const float* restrict cameraBokehDistribution
		KERNEL_ARGS_FILM
		) {
	const size_t gid = get_global_id(0);

	__global GPUTaskState *taskState = &tasksState[gid];

#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
	__global TilePathSamplerSharedData *samplerSharedData = (__global TilePathSamplerSharedData *)samplerSharedDataBuff;

	if (gid >= filmWidth * filmHeight * Sqr(samplerSharedData->aaSamples)) {
		PATH_STATE_SET(MK_DONE);
		// Mark the ray like like one to NOT trace
		rays[gid].flags = RAY_FLAGS_MASKED;

		return;
	}
#endif

	// Initialize the task
	__global GPUTask *task = &tasks[gid];
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];

	// Read the seed
	Seed seedValue = task->seed;
	// This trick is required by Sampler_GetSample() macro
	Seed *seed = &seedValue;

	// Initialize the sample and path
	const bool validSample = Sampler_Init(taskConfig,
			filmNoise,
			filmUserImportance,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3
			SAMPLER_PARAM);

	if (validSample) {
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		__global TilePathSamplerSharedData *samplerSharedData = (__global TilePathSamplerSharedData *)samplerSharedDataBuff;
		const uint cameraFilmWidth = samplerSharedData->cameraFilmWidth;
		const uint cameraFilmHeight = samplerSharedData->cameraFilmHeight;
		const uint tileStartX = samplerSharedData->tileStartX;
		const uint tileStartY =  samplerSharedData->tileStartY;
#endif

		// Generate the eye path
		GenerateEyePath(taskConfig,
				taskDirectLight, taskState,
				camera,
				cameraBokehDistribution,
				filmWidth, filmHeight,
				filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3,
				pixelFilterDistribution,
				&rays[gid],
				&eyePathInfos[gid]
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
				, cameraFilmWidth, cameraFilmHeight,
				tileStartX, tileStartY
#endif
				SAMPLER_PARAM);
		PATH_STATE_SET(MK_RT_NEXT_VERTEX);
	} else {
#if defined(RENDER_ENGINE_TILEPATHOCL) || defined(RENDER_ENGINE_RTPATHOCL)
		PATH_STATE_SET(MK_DONE);
#else
		PATH_STATE_SET(MK_GENERATE_CAMERA_RAY);
#endif
		// Mark the ray like like one to NOT trace
		rays[gid].flags = RAY_FLAGS_MASKED;
	}

	// Save the seed
	task->seed = seedValue;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->sampleCount = 0;
}

//------------------------------------------------------------------------------
// Light tracing (see LightPathTask)
//------------------------------------------------------------------------------

#define KERNEL_ARGS_LIGHT_TRACING \
		, __global LightPathTask *lightTasks \
		/* The light paths done by each light task, read by the host for */ \
		/* the film RADIANCE_PER_SCREEN_NORMALIZED sample count */ \
		, __global uint *lightPathCounts \
		, const uint lightTaskCount \
		, const uint eyeTaskCount \
		, __global const float* restrict emitLightsDistribution \
		, __global const float* restrict lightFilterLUTs \
		/* The bounding spheres of the casters for the guided emission (see */ \
		/* LightEmitGuide) and the probability of the guided emission */ \
		, __global const float* restrict lightCasters \
		, const uint lightCasterCount \
		, const float lightGuidedProb \
		, __global float *filmScreenRadianceGroup0 \
		, __global float *filmScreenRadianceGroup1 \
		, __global float *filmScreenRadianceGroup2 \
		, __global float *filmScreenRadianceGroup3 \
		, __global float *filmScreenRadianceGroup4 \
		, __global float *filmScreenRadianceGroup5 \
		, __global float *filmScreenRadianceGroup6 \
		, __global float *filmScreenRadianceGroup7

// The light task rays are after the eye path rays and are not traced until
// AdvanceLightPaths() starts the light paths
__kernel void InitLightPaths(__global LightPathTask *lightTasks,
		__global uint *lightPathCounts,
		__global Ray *rays,
		const uint lightTaskCount, const uint eyeTaskCount,
		const uint seedBase) {
	const size_t gid = get_global_id(0);
	if (gid >= lightTaskCount)
		return;

	__global LightPathTask *task = &lightTasks[gid];

	Seed seed;
	Rnd_Init(seedBase + gid, &seed);
	task->seed = seed;

	task->state = LT_GENERATE;
	lightPathCounts[gid] = 0;

	rays[eyeTaskCount + gid].flags = RAY_FLAGS_MASKED;
}

// Clears one radiance group of the film RADIANCE_PER_SCREEN_NORMALIZED channel
__kernel void LightTracing_ClearFilm(const uint filmPixelCount,
		__global float *filmScreenRadianceGroup) {
	const size_t gid = get_global_id(0);
	if (gid < filmPixelCount * 3)
		filmScreenRadianceGroup[gid] = 0.f;
}

#if defined(PARAM_LIGHT_TRACING)

// Starts a new light path, as the beginning of PathTracer::RenderLightSample().
// Returns false if the light emission failed (the light path is done).
OPENCL_FORCE_INLINE bool LightTracing_GenerateLightPath(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global LightPathTask *task, Seed *seed,
		__global Ray *ray,
		__global HitPoint *tmpHitPoint,
		__global const Camera* restrict camera,
		__global const float* restrict cameraBokehDistribution,
		__global const float* restrict emitLightsDistribution,
		__global const float* restrict lightCasters, const uint lightCasterCount,
		const float lightGuidedProb,
		const float worldCenterX, const float worldCenterY, const float worldCenterZ,
		const float worldRadius
		LIGHTS_PARAM_DECL) {
	const float time = mix(camera->base.shutterOpen, camera->base.shutterClose, Rnd_FloatValue(seed));

	// Select one light source
	float lightPickPdf;
	const uint lightIndex = Distribution1D_SampleDiscrete(emitLightsDistribution,
			Rnd_FloatValue(seed), &lightPickPdf);
	if (lightPickPdf <= 0.f)
		return false;
	__global const LightSource* restrict light = &lights[lightIndex];

	// Initialize the light path
	const float u0 = Rnd_FloatValue(seed);
	const float u1 = Rnd_FloatValue(seed);
	const float u2 = Rnd_FloatValue(seed);
	const float u3 = Rnd_FloatValue(seed);
	const float passThroughEvent = Rnd_FloatValue(seed);

	LightEmitGuide guide;
	guide.casters = lightCasters;
	guide.casterCount = lightCasterCount;
	guide.prob = (lightCasterCount > 0) ? lightGuidedProb : 0.f;
	guide.u = Rnd_FloatValue(seed);

	float3 rayOrig, rayDir;
	float lightEmitPdfW;
	const float3 flux = Light_Emit(light, &guide,
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			tmpHitPoint, time, u0, u1, u2, u3, passThroughEvent,
			&rayOrig, &rayDir, &lightEmitPdfW
			LIGHTS_PARAM);
	if (Spectrum_IsBlack(flux) || (lightEmitPdfW <= 0.f))
		return false;

	VSTORE3F(flux / (lightEmitPdfW * lightPickPdf), task->flux.c);
	task->lightID = light->lightID;
	task->time = time;

	const float lensU = Rnd_FloatValue(seed);
	const float lensV = Rnd_FloatValue(seed);
	VSTORE3F(PerspectiveCamera_SampleLens(camera, cameraBokehDistribution, time, lensU, lensV),
			&task->lensPoint.x);

	// LightPathInfo initialization
	PathDepthInfo_Init(&task->depth);
	PathVolumeInfo_Init(&task->volume);
	task->lastBSDFEvent = SPECULAR;
	task->isNearlyS = false;

	Ray_Init2(ray, rayOrig, rayDir, time);

	return true;
}

// As FilmSampleSplatter::AtomicSplatSample() of a light sample.
// lightFilterLUTs holds the FilterLUTs of the pixel filter: the number of LUTs
// per axis (0 if there is no filter), the filter width and height, then the
// width, height and data offset of each LUT, then the LUT data.
OPENCL_FORCE_INLINE void LightTracing_SplatSample(
		__global const float* restrict lightFilterLUTs,
		__global float *filmScreenRadianceGroup,
		const uint filmWidth,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		const float filmX, const float filmY, const float3 radiance) {
	if (isnan(radiance.x) || isinf(radiance.x) ||
			isnan(radiance.y) || isinf(radiance.y) ||
			isnan(radiance.z) || isinf(radiance.z))
		return;

	const int lutsSize = (int)lightFilterLUTs[0];
	if (lutsSize == 0) {
		const int x = Floor2Int(filmX);
		const int y = Floor2Int(filmY);

		if ((x >= (int)filmSubRegion0) && (x <= (int)filmSubRegion1) &&
				(y >= (int)filmSubRegion2) && (y <= (int)filmSubRegion3)) {
			__global float *pixel = &filmScreenRadianceGroup[(x + y * filmWidth) * 3];
			AtomicAdd(&pixel[0], radiance.x);
			AtomicAdd(&pixel[1], radiance.y);
			AtomicAdd(&pixel[2], radiance.z);
		}
		return;
	}

	const float filterWidthX = lightFilterLUTs[1];
	const float filterWidthY = lightFilterLUTs[2];

	// Compute sample's raster extent
	const float dImageX = filmX - .5f;
	const float dImageY = filmY - .5f;

	// FilterLUTs::GetLUT()
	const int lutX = clamp(Floor2Int(lutsSize * (dImageX - floor(filmX) + .5f)), 0, lutsSize - 1);
	const int lutY = clamp(Floor2Int(lutsSize * (dImageY - floor(filmY) + .5f)), 0, lutsSize - 1);
	__global const float* restrict lutInfo = &lightFilterLUTs[3 + 3 * (lutX + lutY * lutsSize)];
	const int lutWidth = (int)lutInfo[0];
	const int lutHeight = (int)lutInfo[1];
	__global const float* restrict lut = &lightFilterLUTs[(uint)lutInfo[2]];

	const int x0 = Floor2Int(dImageX - filterWidthX * .5f + .5f);
	const int y0 = Floor2Int(dImageY - filterWidthY * .5f + .5f);

	for (int iy = 0; iy < lutHeight; ++iy) {
		const int y = y0 + iy;
		if ((y < (int)filmSubRegion2) || (y > (int)filmSubRegion3))
			continue;

		for (int ix = 0; ix < lutWidth; ++ix) {
			const int x = x0 + ix;
			if ((x < (int)filmSubRegion0) || (x > (int)filmSubRegion1))
				continue;

			const float filterWeight = lut[ix + iy * lutWidth];
			if (filterWeight == 0.f)
				continue;

			__global float *pixel = &filmScreenRadianceGroup[(x + y * filmWidth) * 3];
			AtomicAdd(&pixel[0], radiance.x * filterWeight);
			AtomicAdd(&pixel[1], radiance.y * filterWeight);
			AtomicAdd(&pixel[2], radiance.z * filterWeight);
		}
	}
}

OPENCL_FORCE_INLINE bool LightTracing_IsNearlySpecular(const BSDFEvent event,
		const float glossiness, const float glossinessThreshold) {
	return (event & SPECULAR) || ((event & GLOSSY) && (glossiness <= glossinessThreshold));
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
