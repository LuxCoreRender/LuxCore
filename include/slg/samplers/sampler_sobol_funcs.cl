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

OPENCL_FORCE_INLINE uint SobolSequence_SobolDimension(const uint index, const uint dimension) {
	const uint offset = dimension * SOBOL_BITS;
	uint result = 0;
	uint i = index;

	for (uint j = 0; i; i >>= 1, j++) {
		if (i & 1)
			result ^= SOBOL_DIRECTIONS[offset + j];
	}

	return result;
}

OPENCL_FORCE_INLINE float SobolSequence_GetSample(__global Sample *sample, const uint index) {
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

OPENCL_FORCE_INLINE void SamplerSharedData_GetNewBucket(__global SamplerSharedData *samplerSharedData,
		const uint filmRegionPixelCount,
		uint *pixelBucketIndex, uint *seed) {
    *pixelBucketIndex = atomic_inc(&samplerSharedData->pixelBucketIndex) %
                            ((filmRegionPixelCount + SOBOL_OCL_WORK_SIZE - 1) / SOBOL_OCL_WORK_SIZE);

    *seed = (samplerSharedData->seedBase + *pixelBucketIndex) % (0xFFFFFFFFu - 1u) + 1u;
}

OPENCL_FORCE_NOT_INLINE void Sampler_InitNewSample(__constant const GPUTaskConfiguration* restrict taskConfig,
		Seed *seed, __global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleDataPathBase,
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
		__global float *filmNoise,
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
		__global float * filmUserImportance,
#endif
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	const uint filmRegionPixelCount = (filmSubRegion1 - filmSubRegion0 + 1) * (filmSubRegion3 - filmSubRegion2 + 1);

	// Update pixelIndexOffset

	uint pixelIndexBase  = sample->pixelIndexBase;
	uint pixelIndexOffset = sample->pixelIndexOffset;

	// pixelIndexRandomStart is used to jitter the order of the pixel rendering
	uint pixelIndexRandomStart = sample->pixelIndexRandomStart;

	Seed rngGeneratorSeed = sample->rngGeneratorSeed;
	for (;;) {
		pixelIndexOffset++;
		if (pixelIndexOffset >= SOBOL_OCL_WORK_SIZE) {
			// Ask for a new base
			uint bucketSeed;
			SamplerSharedData_GetNewBucket(samplerSharedData, filmRegionPixelCount,
					&pixelIndexBase, &bucketSeed);

			// Transform the bucket index in a pixel index
			pixelIndexBase = pixelIndexBase * SOBOL_OCL_WORK_SIZE;
			pixelIndexOffset = 0;

			sample->pixelIndexBase = pixelIndexBase;

			pixelIndexRandomStart = Floor2UInt(Rnd_FloatValue(seed) * SOBOL_OCL_WORK_SIZE);
			sample->pixelIndexRandomStart = pixelIndexRandomStart;

			Rnd_Init(bucketSeed, &rngGeneratorSeed);
		}

		const uint pixelIndex = pixelIndexBase + (pixelIndexOffset + pixelIndexRandomStart) % SOBOL_OCL_WORK_SIZE;
		if (pixelIndex >= filmRegionPixelCount) {
			// Skip the pixels out of the film sub region
			continue;
		}

		const uint subRegionWidth = filmSubRegion1 - filmSubRegion0 + 1;
		const uint pixelX = filmSubRegion0 + (pixelIndex % subRegionWidth);
		const uint pixelY = filmSubRegion2 + (pixelIndex / subRegionWidth);

#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
		const float adaptiveStrength = samplerSharedData->adaptiveStrength;

		if (adaptiveStrength > 0.f) {
			// Pixels are sampled in accordance with how far from convergence they are
			const float noise = filmNoise[pixelX + pixelY * filmWidth];

			// Factor user driven importance sampling too
			float threshold;
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
			const float userImportance = filmUserImportance[pixelX + pixelY * filmWidth];

			// Noise is initialized to INFINITY at start
			if (isinf(noise))
				threshold = userImportance;
			else
				threshold = (userImportance > 0.f) ? Lerp(samplerSharedData->adaptiveUserImportanceWeight, noise, userImportance) : 0.f;
#else
			threshold = noise;
#endif

			// The floor for the pixel importance is given by the adaptiveness strength
			threshold = fmax(threshold, 1.f - adaptiveStrength);
			
			if (Rnd_FloatValue(seed) > threshold) {
				// Skip this pixel and try the next one
				
				// Workaround for preserving random number distribution behavior
				Rnd_UintValue(&rngGeneratorSeed);
				Rnd_FloatValue(&rngGeneratorSeed);
				Rnd_FloatValue(&rngGeneratorSeed);

				continue;
			}
		}
#endif

		//----------------------------------------------------------------------
		// This code crashes the AMD OpenCL compiler:
		//
		// The array of fields is attached to the SamplerSharedData structure
#if !defined(LUXCORE_AMD_OPENCL)
		__global uint *pixelPasses = &samplerSharedData->pixelPass;
		// Get the pass to do
		sample->pass = atomic_inc(&pixelPasses[pixelIndex]);
#else           //--------------------------------------------------------------
		// This one works:
		//
		// The array of fields is attached to the SamplerSharedData structure
		__global uint *pixelPass = &samplerSharedData->pixelPass + pixelIndex;
		// Get the pass to do
		uint oldVal, newVal;
		do {
				oldVal = *pixelPass;
				newVal = oldVal + 1;
		} while (atomic_cmpxchg(pixelPass, oldVal, newVal) != oldVal);
		sample->pass = oldVal;
#endif
		//----------------------------------------------------------------------

		// Initialize rng0 and rng1

		// Limit the number of pass skipped
		sample->rngPass = Rnd_UintValue(&rngGeneratorSeed);
		sample->rng0 = Rnd_FloatValue(&rngGeneratorSeed);
		sample->rng1 = Rnd_FloatValue(&rngGeneratorSeed);

		// Initialize IDX_SCREEN_X and IDX_SCREEN_Y sample

		sampleDataPathBase[IDX_SCREEN_X] = pixelX + SobolSequence_GetSample(sample, IDX_SCREEN_X);
		sampleDataPathBase[IDX_SCREEN_Y] = pixelY + SobolSequence_GetSample(sample, IDX_SCREEN_Y);
		break;
	}
	
	sample->rngGeneratorSeed = rngGeneratorSeed;

	// Save the new value
	sample->pixelIndexOffset = pixelIndexOffset;
}

OPENCL_FORCE_INLINE __global float *Sampler_GetSampleData(__constant const GPUTaskConfiguration* restrict taskConfig,
		__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * TOTAL_U_SIZE];
}

OPENCL_FORCE_INLINE __global float *Sampler_GetSampleDataPathBase(__constant const GPUTaskConfiguration* restrict taskConfig,
		__global Sample *sample, __global float *sampleData) {
	return sampleData;
}

OPENCL_FORCE_INLINE __global float *Sampler_GetSampleDataPathVertex(__constant const GPUTaskConfiguration* restrict taskConfig,
		__global Sample *sample, __global float *sampleDataPathBase, const uint depth) {
	// This is never used in Sobol sampler
	return &sampleDataPathBase[IDX_BSDF_OFFSET + (depth - 1) * VERTEX_SAMPLE_SIZE];
}

OPENCL_FORCE_INLINE float Sampler_GetSamplePath(__constant const GPUTaskConfiguration* restrict taskConfig,
		Seed *seed, __global Sample *sample,
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

OPENCL_FORCE_INLINE float Sampler_GetSamplePathVertex(__constant const GPUTaskConfiguration* restrict taskConfig,
		Seed *seed, __global Sample *sample,
		__global float *sampleDataPathVertexBase,
		const uint depth, const uint index) {
	// The depth used here is counted from the first hit point of the path
	// vertex (so it is effectively depth - 1)
	if (depth < SOBOL_MAX_DEPTH)
		return SobolSequence_GetSample(sample, IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE + index);
	else
		return Rnd_FloatValue(seed);
}

OPENCL_FORCE_NOT_INLINE void Sampler_SplatSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData
		FILM_PARAM_DECL
		) {
	Film_AddSample(&taskConfig->film, sample->result.pixelX, sample->result.pixelY,
			&sample->result, 1.f
			FILM_PARAM);
}

OPENCL_FORCE_NOT_INLINE void Sampler_NextSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample,
		__global float *sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
		__global float *filmNoise,
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
		__global float *filmUserImportance,
#endif
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	Sampler_InitNewSample(taskConfig, seed, samplerSharedData, sample, sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
			filmNoise,
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
			filmUserImportance,
#endif
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);
}

OPENCL_FORCE_NOT_INLINE bool Sampler_Init(__constant const GPUTaskConfiguration* restrict taskConfig,
		Seed *seed, __global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
		__global float *filmNoise,
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
		__global float *filmUserImportance,
#endif
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	sample->pixelIndexOffset = SOBOL_OCL_WORK_SIZE;

	Sampler_NextSample(taskConfig, seed, samplerSharedData, sample, sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
			filmNoise,
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
			filmUserImportance,
#endif
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);

	return true;
}

#endif
