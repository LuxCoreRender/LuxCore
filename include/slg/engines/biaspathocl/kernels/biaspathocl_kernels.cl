#line 2 "biaspatchocl_kernels.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

//------------------------------------------------------------------------------
// InitSeed Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitSeed(
		uint seedBase,
		__global GPUTask *tasks) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

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

//------------------------------------------------------------------------------
// BiasAdvancePaths Kernel
//------------------------------------------------------------------------------

void GenerateCameraPath(
		Seed *seed,
		__global GPUTask *task,
		__global Camera *camera,
		const uint pixelX, const uint pixelY,
		const uint tile_xStart, const uint tile_yStart,
		const uint engineFilmWidth, const uint engineFilmHeight,
		Ray *ray) {
	const float filmX = pixelX + Rnd_FloatValue(seed);
	const float filmY = pixelY + Rnd_FloatValue(seed);
	task->result.filmX = filmX;
	task->result.filmY = filmY;

#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Rnd_FloatValue(seed);
	const float dofSampleY = Rnd_FloatValue(seed);
#endif

	Camera_GenerateRay(camera, engineFilmWidth, engineFilmHeight, ray, tile_xStart + filmX, tile_yStart + filmY
#if defined(PARAM_CAMERA_HAS_DOF)
			, dofSampleX, dofSampleY
#endif
			);	
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample(
		const uint tile_xStart,
		const uint tile_yStart,
		const int tile_sampleIndex,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global GPUTask *tasks,
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
		,
		// Scene parameters
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
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
		__global float *lightsDistribution
#if defined(PARAM_HAS_INFINITELIGHT)
		, __global InfiniteLight *infiniteLight
		, __global float *infiniteLightDistribution
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		, __global SunLight *sunLight
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		, __global SkyLight *skyLight
#endif
#if (PARAM_DL_LIGHT_COUNT > 0)
		, __global TriangleLight *triLightDefs
		, __global uint *meshTriLightDefsOffset
#endif
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
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	const size_t gid = get_global_id(0);

	uint sampleX, sampleY, sampleIndex;
	if (tile_sampleIndex >= 0) {
		sampleIndex = tile_sampleIndex;
		sampleX = gid % PARAM_TILE_SIZE;
		sampleY = gid / PARAM_TILE_SIZE;
	} else {
		sampleIndex = gid % (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
		const uint pixelIndex = gid / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
		sampleX = pixelIndex % PARAM_TILE_SIZE;
		sampleY = pixelIndex / PARAM_TILE_SIZE;
	}

	if ((tile_xStart + sampleX >= engineFilmWidth) ||
			(tile_yStart + sampleY >= engineFilmHeight))
		return;

	__global GPUTask *task = &tasks[gid];
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
		filmRadianceGroup[3] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		filmRadianceGroup[3] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		filmRadianceGroup[3] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		filmRadianceGroup[3] = filmRadianceGroup7;
#endif

	//--------------------------------------------------------------------------
	// Initialize the first ray
	//--------------------------------------------------------------------------

	// Read the seed
	Seed seed;
	seed.s1 = task->seed.s1;
	seed.s2 = task->seed.s2;
	seed.s3 = task->seed.s3;

	SampleResult_Init(&task->result);

	Ray ray;
	RayHit rayHit;
	GenerateCameraPath(&seed, task, camera, sampleX, sampleY, tile_xStart, tile_yStart, engineFilmWidth, engineFilmHeight, &ray);
	PathState pathState = RT_NEXT_VERTEX;

	//--------------------------------------------------------------------------
	// Render a sample
	//--------------------------------------------------------------------------

	uint tracedRaysCount = taskStat->raysCount;
	do {
		//----------------------------------------------------------------------
		// Ray trace step
		//----------------------------------------------------------------------

		Accelerator_Intersect(&ray, &rayHit
			ACCELERATOR_INTERSECT_PARAM);
		++tracedRaysCount;

		//----------------------------------------------------------------------
		// Advance the finite state machine step
		//----------------------------------------------------------------------

		//----------------------------------------------------------------------
		// Evaluation of the Path finite state machine.
		//
		// From: RT_NEXT_VERTEX
		// To: SPLAT_SAMPLE or GENERATE_DL_RAY
		//----------------------------------------------------------------------

		if (pathState == RT_NEXT_VERTEX) {
			if (rayHit.meshIndex != NULL_INDEX) {
				//--------------------------------------------------------------
				// Something was hit
				//--------------------------------------------------------------

				VADD3F(&task->result.radiancePerPixelNormalized[0].r, WHITE);
				pathState = SPLAT_SAMPLE;
			} else {
				//--------------------------------------------------------------
				// Nothing was hit, add environmental lights radiance
				//--------------------------------------------------------------

				VADD3F(&task->result.radiancePerPixelNormalized[0].r, BLACK);
				pathState = SPLAT_SAMPLE;
			}
		}

		//----------------------------------------------------------------------
		// Evaluation of the Path finite state machine.
		//
		// From: SPLAT_SAMPLE
		// To: RT_NEXT_VERTEX
		//----------------------------------------------------------------------

		if (pathState == SPLAT_SAMPLE) {
			Film_AddSample(sampleX, sampleY, &task->result, 1.f
					FILM_PARAM);
			pathState = DONE;
		}

		pathState = DONE;
	} while (pathState != DONE);

	//--------------------------------------------------------------------------

	taskStat->raysCount = tracedRaysCount;

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// MergePixelSamples
//------------------------------------------------------------------------------

void SR_Init(SampleResult *sampleResult) {
	// Initialize only Spectrum fields

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	sampleResult->radiancePerPixelNormalized[0].r = 0.f;
	sampleResult->radiancePerPixelNormalized[0].g = 0.f;
	sampleResult->radiancePerPixelNormalized[0].b = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	sampleResult->radiancePerPixelNormalized[1].r = 0.f;
	sampleResult->radiancePerPixelNormalized[1].g = 0.f;
	sampleResult->radiancePerPixelNormalized[1].b = 0.f
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	sampleResult->radiancePerPixelNormalized[2].r = 0.f;
	sampleResult->radiancePerPixelNormalized[2].g = 0.f;
	sampleResult->radiancePerPixelNormalized[2].b = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	sampleResult->radiancePerPixelNormalized[3].r = 0.f;
	sampleResult->radiancePerPixelNormalized[3].g = 0.f;
	sampleResult->radiancePerPixelNormalized[3].b = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	sampleResult->radiancePerPixelNormalized[4].r = 0.f;
	sampleResult->radiancePerPixelNormalized[4].g = 0.f;
	sampleResult->radiancePerPixelNormalized[4].b = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	sampleResult->radiancePerPixelNormalized[5].r = 0.f;
	sampleResult->radiancePerPixelNormalized[5].g = 0.f;
	sampleResult->radiancePerPixelNormalized[5].b = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	sampleResult->radiancePerPixelNormalized[6].r = 0.f;
	sampleResult->radiancePerPixelNormalized[6].g = 0.f;
	sampleResult->radiancePerPixelNormalized[6].b = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	sampleResult->radiancePerPixelNormalized[7].r = 0.f;
	sampleResult->radiancePerPixelNormalized[7].g = 0.f;
	sampleResult->radiancePerPixelNormalized[7].b = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	sampleResult->directDiffuse.r = 0.f;
	sampleResult->directDiffuse.g = 0.f;
	sampleResult->directDiffuse.b = 0.f
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	sampleResult->directGlossy.r = 0.f;
	sampleResult->directGlossy.g = 0.f;
	sampleResult->directGlossy.b = 0.f
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	sampleResult->emission.r = 0.f;
	sampleResult->emission.g = 0.f;
	sampleResult->emission.b = 0.f
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	sampleResult->indirectDiffuse.r = 0.f;
	sampleResult->indirectDiffuse.g = 0.f;
	sampleResult->indirectDiffuse.b = 0.f
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	sampleResult->indirectGlossy.r = 0.f;
	sampleResult->indirectGlossy.g = 0.f;
	sampleResult->indirectGlossy.b = 0.f
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	sampleResult->indirectSpecular.r = 0.f;
	sampleResult->indirectSpecular.g = 0.f;
	sampleResult->indirectSpecular.b = 0.f
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	sampleResult->depth = INFINITY;
#endif
}

void SR_Accumulate(__global SampleResult *src, SampleResult *dst) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].r += src->radiancePerPixelNormalized[0].r;
	dst->radiancePerPixelNormalized[0].g += src->radiancePerPixelNormalized[0].g;
	dst->radiancePerPixelNormalized[0].b += src->radiancePerPixelNormalized[0].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].r += src->radiancePerPixelNormalized[1].r;
	dst->radiancePerPixelNormalized[1].g += src->radiancePerPixelNormalized[1].g;
	dst->radiancePerPixelNormalized[1].b += src->radiancePerPixelNormalized[1].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].r += src->radiancePerPixelNormalized[2].r;
	dst->radiancePerPixelNormalized[2].g += src->radiancePerPixelNormalized[2].g;
	dst->radiancePerPixelNormalized[2].b += src->radiancePerPixelNormalized[2].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].r += src->radiancePerPixelNormalized[3].r;
	dst->radiancePerPixelNormalized[3].g += src->radiancePerPixelNormalized[3].g;
	dst->radiancePerPixelNormalized[3].b += src->radiancePerPixelNormalized[3].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].r += src->radiancePerPixelNormalized[4].r;
	dst->radiancePerPixelNormalized[4].g += src->radiancePerPixelNormalized[4].g;
	dst->radiancePerPixelNormalized[4].b += src->radiancePerPixelNormalized[4].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].r += src->radiancePerPixelNormalized[5].r;
	dst->radiancePerPixelNormalized[5].g += src->radiancePerPixelNormalized[5].g;
	dst->radiancePerPixelNormalized[5].b += src->radiancePerPixelNormalized[5].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].r += src->radiancePerPixelNormalized[6].r;
	dst->radiancePerPixelNormalized[6].g += src->radiancePerPixelNormalized[6].g;
	dst->radiancePerPixelNormalized[6].b += src->radiancePerPixelNormalized[6].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].r += src->radiancePerPixelNormalized[7].r;
	dst->radiancePerPixelNormalized[7].g += src->radiancePerPixelNormalized[7].g;
	dst->radiancePerPixelNormalized[7].b += src->radiancePerPixelNormalized[7].b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha += dst->alpha + src->alpha;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.r += src->directDiffuse.r;
	dst->directDiffuse.g += src->directDiffuse.g;
	dst->directDiffuse.b += src->directDiffuse.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.r += src->directGlossy.r;
	dst->directGlossy.g += src->directGlossy.g;
	dst->directGlossy.b += src->directGlossy.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.r += src->emission.r;
	dst->emission.g += src->emission.g;
	dst->emission.b += src->emission.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.r += src->indirectDiffuse.r;
	dst->indirectDiffuse.g += src->indirectDiffuse.g;
	dst->indirectDiffuse.b += src->indirectDiffuse.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.r += src->indirectGlossy.r;
	dst->indirectGlossy.g += src->indirectGlossy.g;
	dst->indirectGlossy.b += src->indirectGlossy.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.r += src->indirectSpecular.r;
	dst->indirectSpecular.g += src->indirectSpecular.g;
	dst->indirectSpecular.b += src->indirectSpecular.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	dst->directShadowMask += src->directShadowMask;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	dst->indirectShadowMask += src->indirectShadowMask;
#endif

	bool depthWrite = true;
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	const float srcDepthValue = src->depth;
	if (srcDepthValue <= dst->depth)
		dst->depth = srcDepthValue;
	else
		depthWrite = false;
#endif
	if (depthWrite) {
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		dst->position = src->position;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		dst->geometryNormal = src->geometryNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		dst->shadingNormal = src->shadingNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		// Note: MATERIAL_ID_MASK is calculated starting from materialID field
		dst->materialID = src->materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		dst->uv = src->uv;
#endif
	}
}
void SR_Normalize(SampleResult *dst, const float k) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].r *= k;
	dst->radiancePerPixelNormalized[0].g *= k;
	dst->radiancePerPixelNormalized[0].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].r *= k;
	dst->radiancePerPixelNormalized[1].g *= k;
	dst->radiancePerPixelNormalized[1].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].r *= k;
	dst->radiancePerPixelNormalized[2].g *= k;
	dst->radiancePerPixelNormalized[2].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].r *= k;
	dst->radiancePerPixelNormalized[3].g *= k;
	dst->radiancePerPixelNormalized[3].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].r *= k;
	dst->radiancePerPixelNormalized[4].g *= k;
	dst->radiancePerPixelNormalized[4].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].r *= k;
	dst->radiancePerPixelNormalized[5].g *= k;
	dst->radiancePerPixelNormalized[5].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].r *= k;
	dst->radiancePerPixelNormalized[6].g *= k;
	dst->radiancePerPixelNormalized[6].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].r *= k;
	dst->radiancePerPixelNormalized[7].g *= k;
	dst->radiancePerPixelNormalized[7].b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha *= k;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.r *= k;
	dst->directDiffuse.g *= k;
	dst->directDiffuse.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.r *= k;
	dst->directGlossy.g *= k;
	dst->directGlossy.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.r *= k;
	dst->emission.g *= k;
	dst->emission.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.r *= k;
	dst->indirectDiffuse.g *= k;
	dst->indirectDiffuse.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.r *= k;
	dst->indirectGlossy.g *= k;
	dst->indirectGlossy.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.r *= k;
	dst->indirectSpecular.g *= k;
	dst->indirectSpecular.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	dst->directShadowMask *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	dst->indirectShadowMask *= k;
#endif
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergePixelSamples(
		const uint tile_xStart,
		const uint tile_yStart,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global GPUTask *tasks,
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
		) {
	const size_t gid = get_global_id(0);

	uint sampleX, sampleY;
	sampleX = gid % PARAM_TILE_SIZE;
	sampleY = gid / PARAM_TILE_SIZE;

	if ((tile_xStart + sampleX >= engineFilmWidth) ||
			(tile_yStart + sampleY >= engineFilmHeight))
		return;

	__global GPUTask *sampleTasks = &tasks[gid * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES];

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
		filmRadianceGroup[3] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		filmRadianceGroup[3] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		filmRadianceGroup[3] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		filmRadianceGroup[3] = filmRadianceGroup7;
	#endif

	//--------------------------------------------------------------------------
	// Merge all samples
	//--------------------------------------------------------------------------

	SampleResult result;
	SR_Init(&result);

	for (uint i = 0; i < PARAM_AA_SAMPLES * PARAM_AA_SAMPLES; ++i)
		SR_Accumulate(&sampleTasks[i].result, &result);
	SR_Normalize(&result, 1.f / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES));

	// I have to save result in __global space in order to be able
	// to use Film_AddSample(). OpenCL can be so stupid some time...
	sampleTasks[0].result = result;
	Film_AddSample(sampleX, sampleY, &sampleTasks[0].result, PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);
}
