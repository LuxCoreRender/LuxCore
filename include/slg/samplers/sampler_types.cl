#line 2 "sampler_types.cl"

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
// Indices of Sample related u[] array
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define IDX_SCREEN_X 0
#define IDX_SCREEN_Y 1
#define IDX_EYE_TIME 2
#define IDX_DOF_X 3
#define IDX_DOF_Y 4
#define IDX_BSDF_OFFSET 5

// Relative to IDX_BSDF_OFFSET + PathDepth * VERTEX_SAMPLE_SIZE
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

#endif

//------------------------------------------------------------------------------
// Sample data types
//------------------------------------------------------------------------------

typedef struct {
	unsigned int bucketIndex, pixelOffset, passOffset, pass;
} RandomSample;

typedef struct {
	float totalI;

	// Using ushort here totally freeze the ATI driver
	unsigned int largeMutationCount, smallMutationCount;
	unsigned int current, proposed, consecutiveRejects;

	float weight;

	SampleResult currentResult;
} MetropolisSample;

typedef struct {
	unsigned int bucketIndex, pixelOffset, passOffset, pass;

	Seed rngGeneratorSeed;
	unsigned int rngPass;
	float rng0, rng1;
} SobolSample;

typedef struct {
	unsigned int rngPass;
	float rng0, rng1;

	unsigned int pass;
} TilePathSample;

//------------------------------------------------------------------------------
// Sampler shared data types
//------------------------------------------------------------------------------

typedef struct {
	unsigned int bucketIndex;
} RandomSamplerSharedData;

typedef struct {
	unsigned int seedBase;
	unsigned int bucketIndex;

	// This is used to compute the size of appended data at the end
	// of SobolSamplerSharedData
	unsigned int filmRegionPixelCount;

	// Plus the a pass field for each pixel
	// Plus Sobol directions array
} SobolSamplerSharedData;

typedef struct {
	// cameraFilmWidth/cameraFilmHeight and filmWidth/filmHeight are usually
	// the same. They are different when doing tile rendering
	unsigned int cameraFilmWidth, cameraFilmHeight;
	unsigned int tileStartX, tileStartY;
	unsigned int tileWidth, tileHeight;
	unsigned int tilePass, aaSamples;
	unsigned int multipassIndexToRender;

	// Plus Sobol directions array
} TilePathSamplerSharedData;

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
			float adaptiveStrength, adaptiveUserImportanceWeight;
			unsigned int bucketSize, tileSize, superSampling, overlapping;
		} random;
		struct {
			float adaptiveStrength, adaptiveUserImportanceWeight;
			unsigned int bucketSize, tileSize, superSampling, overlapping;
		} sobol;
		struct {
			float largeMutationProbability, imageMutationRange;
			unsigned int maxRejects;
		} metropolis;
	};
} Sampler;

#define SOBOL_BITS 32
#define SOBOL_MAX_DIMENSIONS 21201
#define SOBOL_STARTOFFSET 32

#define SAMPLER_PARAM_DECL \
		, Seed *seed \
		, __global void *samplerSharedDataBuff \
		, __global void *samplesBuff \
		, __global float *samplesDataBuff \
		, __global SampleResult *sampleResultsBuff
#define SAMPLER_PARAM \
		, seed \
		, samplerSharedDataBuff \
		, samplesBuff \
		, samplesDataBuff \
		, sampleResultsBuff
