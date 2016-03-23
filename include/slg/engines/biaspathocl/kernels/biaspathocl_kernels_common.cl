#line 2 "biaspathocl_kernel_common.cl"

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

//------------------------------------------------------------------------------
// Kernel parameters
//------------------------------------------------------------------------------

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_0 \
		, __global float *filmRadianceGroup0
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_1 \
		, __global float *filmRadianceGroup1
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_2 \
		, __global float *filmRadianceGroup2
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_3 \
		, __global float *filmRadianceGroup3
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_4 \
		, __global float *filmRadianceGroup4
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_5 \
		, __global float *filmRadianceGroup5
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_6 \
		, __global float *filmRadianceGroup6
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_7 \
		, __global float *filmRadianceGroup7
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
#define KERNEL_ARGS_FILM_CHANNELS_ALPHA \
		, __global float *filmAlpha
#else
#define KERNEL_ARGS_FILM_CHANNELS_ALPHA
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
#define KERNEL_ARGS_FILM_CHANNELS_DEPTH \
		, __global float *filmDepth
#else
#define KERNEL_ARGS_FILM_CHANNELS_DEPTH
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
#define KERNEL_ARGS_FILM_CHANNELS_POSITION \
		, __global float *filmPosition
#else
#define KERNEL_ARGS_FILM_CHANNELS_POSITION
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
#define KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL \
		, __global float *filmGeometryNormal
#else
#define KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
#define KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL \
		, __global float *filmShadingNormal
#else
#define KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID \
		, __global uint *filmMaterialID
#else
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE \
		, __global float *filmDirectDiffuse
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY \
		, __global float *filmDirectGlossy
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
#define KERNEL_ARGS_FILM_CHANNELS_EMISSION \
		, __global float *filmEmission
#else
#define KERNEL_ARGS_FILM_CHANNELS_EMISSION
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE \
		, __global float *filmIndirectDiffuse
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY \
		, __global float *filmIndirectGlossy
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR \
		, __global float *filmIndirectSpecular
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK \
		, __global float *filmMaterialIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		, __global float *filmDirectShadowMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		, __global float *filmIndirectShadowMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
#define KERNEL_ARGS_FILM_CHANNELS_UV \
		, __global float *filmUV
#else
#define KERNEL_ARGS_FILM_CHANNELS_UV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
#define KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		, __global float *filmRayCount
#else
#define KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
#define KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID \
		, __global float *filmByMaterialID
#else
#define KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
#define KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE \
		, __global float *filmIrradiance
#else
#define KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID \
		, __global uint *filmObjectID
#else
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK \
		, __global float *filmObjectIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
#define KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID \
		, __global float *filmByObjectID
#else
#define KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID
#endif

#define KERNEL_ARGS_FILM \
		, const uint filmWidth, const uint filmHeight \
		, const uint filmSubRegion0, const uint filmSubRegion1 \
		, const uint filmSubRegion2, const uint filmSubRegion3 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_0 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_1 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_2 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_3 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_4 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_5 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_6 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_7 \
		KERNEL_ARGS_FILM_CHANNELS_ALPHA \
		KERNEL_ARGS_FILM_CHANNELS_DEPTH \
		KERNEL_ARGS_FILM_CHANNELS_POSITION \
		KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL \
		KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL \
		KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY \
		KERNEL_ARGS_FILM_CHANNELS_EMISSION \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR \
		KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_UV \
		KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID \
		KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE \
		KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID \
		KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID

#if defined(PARAM_HAS_INFINITELIGHTS)
#define KERNEL_ARGS_INFINITELIGHTS \
		, const float worldCenterX \
		, const float worldCenterY \
		, const float worldCenterZ \
		, const float worldRadius
#else
#define KERNEL_ARGS_INFINITELIGHTS
#endif

#if defined(PARAM_HAS_NORMALS_BUFFER)
#define KERNEL_ARGS_NORMALS_BUFFER \
		, __global const Vector* restrict vertNormals
#else
#define KERNEL_ARGS_NORMALS_BUFFER
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
#define KERNEL_ARGS_UVS_BUFFER \
		, __global const UV* restrict vertUVs
#else
#define KERNEL_ARGS_UVS_BUFFER
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
#define KERNEL_ARGS_COLS_BUFFER \
		, __global const Spectrum* restrict vertCols
#else
#define KERNEL_ARGS_COLS_BUFFER
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
#define KERNEL_ARGS_ALPHAS_BUFFER \
		, __global const float* restrict vertAlphas
#else
#define KERNEL_ARGS_ALPHAS_BUFFER
#endif

#if defined(PARAM_HAS_ENVLIGHTS)
#define KERNEL_ARGS_ENVLIGHTS \
		, __global const uint* restrict envLightIndices \
		, const uint envLightCount
#else
#define KERNEL_ARGS_ENVLIGHTS
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
#define KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict infiniteLightDistribution
#else
#define KERNEL_ARGS_INFINITELIGHT
#endif

#if defined(PARAM_IMAGEMAPS_PAGE_0)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0 \
		, __global const ImageMap* restrict imageMapDescs, __global const float* restrict imageMapBuff0
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1 \
		, __global const float* restrict imageMapBuff1
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2 \
		, __global const float* restrict imageMapBuff2
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3 \
		, __global const float* restrict imageMapBuff3
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4 \
		, __global const float* restrict imageMapBuff4
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5 \
		, __global const float* restrict imageMapBuff5
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6 \
		, __global const float* restrict imageMapBuff6
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_7 \
		, __global const float* restrict imageMapBuff7
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_7
#endif
#define KERNEL_ARGS_IMAGEMAPS_PAGES \
		KERNEL_ARGS_IMAGEMAPS_PAGE_0 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_1 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_2 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_3 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_4 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_5 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_6 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_7

#define KERNEL_ARGS \
		const uint engineFilmWidth, const uint engineFilmHeight \
		, __global GPUTask *tasks \
		, __global GPUTaskDirectLight *tasksDirectLight \
		, __global GPUTaskPathVertexN *tasksPathVertexN \
		, __global GPUTaskStats *taskStats \
		, __global SampleResult *taskResults \
		, __global float *pixelFilterDistribution \
		/* Film parameters */ \
		KERNEL_ARGS_FILM \
		/* Scene parameters */ \
		KERNEL_ARGS_INFINITELIGHTS \
		, __global const Material* restrict mats \
		, __global const Texture* restrict texs \
		, __global const SceneObject* restrict sceneObjs \
		, __global const Mesh* restrict meshDescs \
		, __global const Point* restrict vertices \
		KERNEL_ARGS_NORMALS_BUFFER \
		KERNEL_ARGS_UVS_BUFFER \
		KERNEL_ARGS_COLS_BUFFER \
		KERNEL_ARGS_ALPHAS_BUFFER \
		, __global const Triangle* restrict triangles \
		, __global const Camera* restrict camera \
		/* Lights */ \
		, __global const LightSource* restrict lights \
		KERNEL_ARGS_ENVLIGHTS \
		, __global const uint* restrict meshTriLightDefsOffset \
		KERNEL_ARGS_INFINITELIGHT \
		, __global const float* restrict lightsDistribution \
		, __global const float* restrict infiniteLightSourcesDistribution \
		/* Images */ \
		KERNEL_ARGS_IMAGEMAPS_PAGES \
		ACCELERATOR_INTERSECT_PARAM_DECL


//------------------------------------------------------------------------------
// To initialize image maps page pointer table
//------------------------------------------------------------------------------

#if defined(PARAM_IMAGEMAPS_PAGE_0)
#define INIT_IMAGEMAPS_PAGE_0 imageMapBuff[0] = imageMapBuff0;
#else
#define INIT_IMAGEMAPS_PAGE_0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
#define INIT_IMAGEMAPS_PAGE_1 imageMapBuff[1] = imageMapBuff1;
#else
#define INIT_IMAGEMAPS_PAGE_1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
#define INIT_IMAGEMAPS_PAGE_2 imageMapBuff[2] = imageMapBuff2;
#else
#define INIT_IMAGEMAPS_PAGE_2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
#define INIT_IMAGEMAPS_PAGE_3 imageMapBuff[3] = imageMapBuff3;
#else
#define INIT_IMAGEMAPS_PAGE_3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define INIT_IMAGEMAPS_PAGE_4 imageMapBuff[4] = imageMapBuff4;
#else
#define INIT_IMAGEMAPS_PAGE_4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
#define INIT_IMAGEMAPS_PAGE_5 imageMapBuff[5] = imageMapBuff5;
#else
#define INIT_IMAGEMAPS_PAGE_5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
#define INIT_IMAGEMAPS_PAGE_6 imageMapBuff[6] = imageMapBuff6;
#else
#define INIT_IMAGEMAPS_PAGE_6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
#define INIT_IMAGEMAPS_PAGE_7 imageMapBuff[7] = imageMapBuff7;
#else
#define INIT_IMAGEMAPS_PAGE_7
#endif

#if defined(PARAM_HAS_IMAGEMAPS)
#define INIT_IMAGEMAPS_PAGES \
	__global const float* restrict imageMapBuff[PARAM_IMAGEMAPS_COUNT]; \
	INIT_IMAGEMAPS_PAGE_0 \
	INIT_IMAGEMAPS_PAGE_1 \
	INIT_IMAGEMAPS_PAGE_2 \
	INIT_IMAGEMAPS_PAGE_3 \
	INIT_IMAGEMAPS_PAGE_4 \
	INIT_IMAGEMAPS_PAGE_5 \
	INIT_IMAGEMAPS_PAGE_6 \
	INIT_IMAGEMAPS_PAGE_7
#else
#define INIT_IMAGEMAPS_PAGES
#endif

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

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->raysCount = 0;
}

//------------------------------------------------------------------------------
// MergePixelSamples
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergePixelSamples(
		const uint pass,
		const uint tileStartX, const uint tileStartY,
		const uint tileWidth, const uint tileHeight,
		const uint tileTotalWidth, const uint tileTotalHeight,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global SampleResult *taskResults
		// Film parameters
		KERNEL_ARGS_FILM
		) {
	const size_t gid = get_global_id(0);

	const uint pixelX = gid % tileTotalWidth;
	const uint pixelY = gid / tileTotalWidth;

	if ((gid >= tileTotalWidth * tileTotalHeight) ||
			(pixelX >= tileWidth) ||
			(pixelY >= tileHeight) ||
			(tileStartX + pixelX >= engineFilmWidth) ||
			(tileStartY + pixelY >= engineFilmHeight))
		return;

	uint index;
	if (!GetSampleResultIndex(pass, tileTotalWidth, tileTotalHeight, pixelX, pixelY, &index))
		return;

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
	// Radiance clamping
	//--------------------------------------------------------------------------

	// It is now done at tile level inside the TileRepository on the CPU

	//--------------------------------------------------------------------------
	// Merge all samples and accumulate statistics
	//--------------------------------------------------------------------------

#if defined(RENDER_ENGINE_RTBIASPATHOCL)
	// .05f is just a fake weight to allow the preview samples to be fast out-weighted
	// by normal samples
	const float weight = RT_IsPreview(pass) ? .05f : (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
#else
	const float weight = PARAM_AA_SAMPLES * PARAM_AA_SAMPLES;
#endif

#if (PARAM_AA_SAMPLES == 1)
	Film_AddSample(pixelX, pixelY, sampleResult, weight
			FILM_PARAM);
#else
	SampleResult result = sampleResult[0];
	for (uint i = 1; i < PARAM_AA_SAMPLES * PARAM_AA_SAMPLES; ++i)
		SR_Accumulate(&sampleResult[i], &result);
	SR_Normalize(&result, 1.f / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES));

	// I have to save result in __global space in order to be able
	// to use Film_AddSample(). OpenCL can be so stupid some time...
	sampleResult[0] = result;
	Film_AddSample(pixelX, pixelY, sampleResult, weight
			FILM_PARAM);
#endif
}
