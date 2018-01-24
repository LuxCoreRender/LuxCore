#line 2 "sampler_sobol_funcs.cl"

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
// Sobol Sequence
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 2) || ((PARAM_SAMPLER_TYPE == 3) && defined(RENDER_ENGINE_TILEPATHOCL))

uint SobolSequence_SobolDimension(const uint index, const uint dimension) {
	const uint offset = dimension * SOBOL_BITS;
	uint result = 0;
	uint i = index;

	for (uint j = 0; i; i >>= 1, j++) {
		if (i & 1)
			result ^= SOBOL_DIRECTIONS[offset + j];
	}

	return result;
}

float SobolSequence_GetSample(__global Sample *sample, const uint index) {
	const uint pass = sample->pass;

	// I scramble pass too in order avoid correlations visible with LIGHTCPU and PATHCPU
	const uint iResult = SobolSequence_SobolDimension(pass + sample->rngPass, index);
	const float fResult = iResult * (1.f / 0xffffffffu);

	// Cranley-Patterson rotation to reduce visible regular patterns
	const float shift = (index & 1) ? sample->rng0 : sample->rng1;
	const float val = fResult + shift;

	return val - floor(val);
}

#endif

//------------------------------------------------------------------------------
// Sobol Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 2)

void SamplerSharedData_GetNewBucket(__global SamplerSharedData *samplerSharedData,
		const uint filmRegionPixelCount,
		uint *pixelBucketIndex, uint *pass, uint *seed) {
	*pixelBucketIndex = atomic_inc(&samplerSharedData->pixelBucketIndex) %
				(filmRegionPixelCount / SOBOL_OCL_WORK_SIZE);

	// The array of fields is attached to the SamplerSharedData structure
	__global uint *bucketPass = (__global uint *)(&samplerSharedData->pixelBucketIndex + sizeof(SamplerSharedData));
	*pass = atomic_inc(&bucketPass[*pixelBucketIndex]);

	*seed = (samplerSharedData->seedBase + *pixelBucketIndex) % (0xFFFFFFFFu - 1u) + 1u;
}

void Sampler_InitNewSample(Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleDataPathBase,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	const uint filmRegionPixelCount = (filmSubRegion1 - filmSubRegion0 + 1) * (filmSubRegion3 - filmSubRegion2 + 1);

	// Update pixelIndexOffset

	uint pixelIndexBase  = sample->pixelIndexBase;
	uint pixelIndexOffset = sample->pixelIndexOffset;
	uint pass = sample->pass;
	// pixelIndexRandomStart is used to jitter the order of the pixel rendering
	uint pixelIndexRandomStart = sample->pixelIndexRandomStart;

	pixelIndexOffset++;
	if ((pixelIndexOffset > SOBOL_OCL_WORK_SIZE) ||
			(pixelIndexBase + pixelIndexOffset >= filmRegionPixelCount)) {
		// Ask for a new base
		uint bucketSeed;
		SamplerSharedData_GetNewBucket(samplerSharedData, filmRegionPixelCount,
				&pixelIndexBase, &pass, &bucketSeed);
		
		// Transform the bucket index in a pixel index
		pixelIndexBase = pixelIndexBase * SOBOL_OCL_WORK_SIZE;
		pixelIndexOffset = 0;

		sample->pixelIndexBase = pixelIndexBase;
		sample->pass = pass;
		
		pixelIndexRandomStart = Floor2UInt(Rnd_FloatValue(seed) * SOBOL_OCL_WORK_SIZE);
		sample->pixelIndexRandomStart = pixelIndexRandomStart;

		Seed rngGeneratorSeed;
		Rnd_Init(bucketSeed, &rngGeneratorSeed);
		sample->rngGeneratorSeed = rngGeneratorSeed;
	}
	
	// Save the new value
	sample->pixelIndexOffset = pixelIndexOffset;

	// Initialize rng0 and rng1

	Seed rngGeneratorSeed = sample->rngGeneratorSeed;
	// Limit the number of pass skipped
	sample->rngPass = Rnd_UintValue(&rngGeneratorSeed) % 512;
	sample->rng0 = Rnd_FloatValue(&rngGeneratorSeed);
	sample->rng1 = Rnd_FloatValue(&rngGeneratorSeed);
	sample->rngGeneratorSeed = rngGeneratorSeed;

	// Initialize IDX_SCREEN_X and IDX_SCREEN_Y sample

	const uint pixelIndex = (pixelIndexBase + pixelIndexOffset + pixelIndexRandomStart) % filmRegionPixelCount;
	const uint subRegionWidth = filmSubRegion1 - filmSubRegion0 + 1;
	const uint pixelX = filmSubRegion0 + (pixelIndex % subRegionWidth);
	const uint pixelY = filmSubRegion2 + (pixelIndex / subRegionWidth);

	sampleDataPathBase[IDX_SCREEN_X] = (pixelX + SobolSequence_GetSample(sample, IDX_SCREEN_X)) / filmWidth;
	sampleDataPathBase[IDX_SCREEN_Y] = (pixelY + SobolSequence_GetSample(sample, IDX_SCREEN_Y)) / filmHeight;
}

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return sampleData;
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
		return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

float Sampler_GetSamplePath(Seed *seed, __global Sample *sample,
		__global float *sampleDataPathBase, const uint index) {
	switch (index) {
		case IDX_SCREEN_X:
			return sampleDataPathBase[IDX_SCREEN_X];
		case IDX_SCREEN_Y:
			return sampleDataPathBase[IDX_SCREEN_Y];
		default:
			return SobolSequence_GetSample(sample, index);
	}
}

float Sampler_GetSamplePathVertex(Seed *seed, __global Sample *sample,
		__global float *sampleDataPathVertexBase,
		const uint depth, const uint index) {
	if (depth < SOBOL_MAX_DEPTH)
		return SobolSequence_GetSample(sample, IDX_BSDF_OFFSET + (depth - 1) * VERTEX_SAMPLE_SIZE + index);
	else
		return Rnd_FloatValue(seed);
}

void Sampler_SplatSample(
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData
		FILM_PARAM_DECL
		) {
	Film_AddSample(sample->result.pixelX, sample->result.pixelY,
			&sample->result, 1.f
			FILM_PARAM);
}

void Sampler_NextSample(
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample,
		__global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	Sampler_InitNewSample(seed, samplerSharedData, sample, sampleData, filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);
}

bool Sampler_Init(Seed *seed, __global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	sample->pixelIndexOffset = SOBOL_OCL_WORK_SIZE;

	Sampler_NextSample(seed, samplerSharedData, sample, sampleData, filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);

	return true;
}

#endif
