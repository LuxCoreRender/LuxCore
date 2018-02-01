#line 2 "sampler_types.cl"

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
// Indices of Sample related u[] array
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define IDX_SCREEN_X 0
#define IDX_SCREEN_Y 1
#define IDX_EYE_TIME 2
#if defined(PARAM_HAS_PASSTHROUGH)
#define IDX_EYE_PASSTHROUGH 3
#define IDX_DOF_X 4
#define IDX_DOF_Y 5
#define IDX_BSDF_OFFSET 6
#else
#define IDX_DOF_X 3
#define IDX_DOF_Y 4
#define IDX_BSDF_OFFSET 5
#endif

// Relative to IDX_BSDF_OFFSET + PathDepth * VERTEX_SAMPLE_SIZE
#if defined(PARAM_HAS_PASSTHROUGH)

#define IDX_PASSTHROUGH 0
#define IDX_BSDF_X 1
#define IDX_BSDF_Y 2
#define IDX_DIRECTLIGHT_X 3
#define IDX_DIRECTLIGHT_Y 4
#define IDX_DIRECTLIGHT_Z 5
#define IDX_DIRECTLIGHT_W 6
#define IDX_DIRECTLIGHT_A 7
#define IDX_RR 8

#define VERTEX_SAMPLE_SIZE 9

#else

#define IDX_BSDF_X 0
#define IDX_BSDF_Y 1
#define IDX_DIRECTLIGHT_X 2
#define IDX_DIRECTLIGHT_Y 3
#define IDX_DIRECTLIGHT_Z 4
#define IDX_DIRECTLIGHT_W 5
#define IDX_RR 6

#define VERTEX_SAMPLE_SIZE 7
#endif

#if (PARAM_SAMPLER_TYPE == 0)
#define TOTAL_U_SIZE 2
#endif

#if (PARAM_SAMPLER_TYPE == 1)
#define TOTAL_U_SIZE (IDX_BSDF_OFFSET + PARAM_MAX_PATH_DEPTH * VERTEX_SAMPLE_SIZE)
#endif

#if (PARAM_SAMPLER_TYPE == 2)
#define TOTAL_U_SIZE 2
#endif

#if (PARAM_SAMPLER_TYPE == 3)
#define TOTAL_U_SIZE 2
#endif

#endif

//------------------------------------------------------------------------------
// Sample data types
//------------------------------------------------------------------------------

// This is defined only under OpenCL because of variable size structures
#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	unsigned int pixelIndexBase, pixelIndexOffset, pixelIndexRandomStart;

	SampleResult result;
} RandomSample;

typedef struct {
	SampleResult result;

	float totalI;

	// Using ushort here totally freeze the ATI driver
	unsigned int largeMutationCount, smallMutationCount;
	unsigned int current, proposed, consecutiveRejects;

	float weight;
	SampleResult currentResult;
} MetropolisSample;

typedef struct {
	unsigned int pixelIndexBase, pixelIndexOffset, pass, pixelIndexRandomStart;

	Seed rngGeneratorSeed;
	unsigned int rngPass;
	float rng0, rng1;

	SampleResult result;
} SobolSample;

typedef struct {
	unsigned int rngPass;
	float rng0, rng1;

	unsigned int pass;

	SampleResult result;
} TilePathSample;

#if (PARAM_SAMPLER_TYPE == 0)
typedef RandomSample Sample;
#endif

#if (PARAM_SAMPLER_TYPE == 1)
typedef MetropolisSample Sample;
#endif

#if (PARAM_SAMPLER_TYPE == 2)
typedef SobolSample Sample;
#endif

// This is a special Sampler used by TILEPATHOCL
#if (PARAM_SAMPLER_TYPE == 3)
typedef TilePathSample Sample;
#endif

#endif

//------------------------------------------------------------------------------
// Sampler shared data types
//------------------------------------------------------------------------------

typedef struct {
	unsigned int pixelBucketIndex;
	float adaptiveStrength;
} RandomSamplerSharedData;

typedef struct {
	unsigned int seedBase;
	unsigned int pixelBucketIndex;
	float adaptiveStrength;
	// Plus the a pass field for each buckets
} SobolSamplerSharedData;

// An array of TilePathSamplerSharedData with one for each thread
typedef struct {
	unsigned int rngPass;
	float rng0, rng1;
} TilePathSamplerSharedData;

#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	unsigned int dummy;
} DummySamplerSharedData ;

#if (PARAM_SAMPLER_TYPE == 0)
typedef RandomSamplerSharedData SamplerSharedData;
#endif

#if (PARAM_SAMPLER_TYPE == 1)
typedef DummySamplerSharedData SamplerSharedData;
#endif

#if (PARAM_SAMPLER_TYPE == 2)
typedef SobolSamplerSharedData SamplerSharedData;
#endif

#if (PARAM_SAMPLER_TYPE == 3)
typedef TilePathSamplerSharedData SamplerSharedData;
#endif

#endif

//------------------------------------------------------------------------------
// Sampler data types
//------------------------------------------------------------------------------

typedef enum {
	RANDOM = 0,
	METROPOLIS = 1,
	SOBOL = 2,
	TILEPATHSAMPLER = 3
} SamplerType;

typedef struct {
	SamplerType type;
	union {
		struct {
			float adaptiveStrength;
		} random;
		struct {
			float adaptiveStrength;
		} sobol;
		struct {
			float largeMutationProbability, imageMutationRange;
			unsigned int maxRejects;
		} metropolis;
	};
} Sampler;

#define RANDOM_OCL_WORK_SIZE 32

#define SOBOL_OCL_WORK_SIZE 32
#define SOBOL_BITS 32
#define SOBOL_MAX_DIMENSIONS 21201
#define SOBOL_MAX_DEPTH 16
