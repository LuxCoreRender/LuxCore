#line 2 "sampler_random_funcs.cl"

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
// Random Sampler Kernel
//------------------------------------------------------------------------------

#define RANDOMSAMPLER_TOTAL_U_SIZE 2

OPENCL_FORCE_INLINE float RandomSampler_GetSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint index
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__global float *samplesData = &samplesDataBuff[gid * RANDOMSAMPLER_TOTAL_U_SIZE];

	switch (index) {
		case IDX_SCREEN_X:
			return samplesData[IDX_SCREEN_X];
		case IDX_SCREEN_Y:
			return samplesData[IDX_SCREEN_Y];
		default:
			return Rnd_FloatValue(seed);
	}
}

OPENCL_FORCE_INLINE void RandomSampler_SplatSample(
		__constant const GPUTaskConfiguration* restrict taskConfig
		SAMPLER_PARAM_DECL
		FILM_PARAM_DECL
		) {
	const size_t gid = get_global_id(0);
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

	Film_AddSample(sampleResult->pixelX, sampleResult->pixelY,
			sampleResult, 1.f
			FILM_PARAM);
}

OPENCL_FORCE_INLINE uint RandomSamplerSharedData_GetNewPixelBucketIndex(__global RandomSamplerSharedData *samplerSharedData) {
	return atomic_inc(&samplerSharedData->pixelBucketIndex);
}

OPENCL_FORCE_INLINE void RandomSampler_InitNewSample(__constant const GPUTaskConfiguration* restrict taskConfig,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__global RandomSamplerSharedData *samplerSharedData = (__global RandomSamplerSharedData *)samplerSharedDataBuff;
	__global RandomSample *samples = (__global RandomSample *)samplesBuff;
	__global RandomSample *sample = &samples[gid];
	__global float *samplesData = &samplesDataBuff[gid * RANDOMSAMPLER_TOTAL_U_SIZE];

	const uint filmRegionPixelCount = (filmSubRegion1 - filmSubRegion0 + 1) * (filmSubRegion3 - filmSubRegion2 + 1);

	// Update pixelIndexOffset

	uint pixelIndexBase  = sample->pixelIndexBase;
	uint pixelIndexOffset = sample->pixelIndexOffset;
	// pixelIndexRandomStart is used to jitter the order of the pixel rendering
	uint pixelIndexRandomStart = sample->pixelIndexRandomStart;

	for (;;) {
		pixelIndexOffset++;
		if (pixelIndexOffset >= RANDOM_OCL_WORK_SIZE) {
			// Ask for a new base

			// Transform the bucket index in a pixel index
			pixelIndexBase = (RandomSamplerSharedData_GetNewPixelBucketIndex(samplerSharedData) %
					((filmRegionPixelCount + RANDOM_OCL_WORK_SIZE - 1) / RANDOM_OCL_WORK_SIZE)) * RANDOM_OCL_WORK_SIZE;
			sample->pixelIndexBase = pixelIndexBase;

			pixelIndexOffset = 0;

			pixelIndexRandomStart = Floor2UInt(Rnd_FloatValue(seed) * RANDOM_OCL_WORK_SIZE);
			sample->pixelIndexRandomStart = pixelIndexRandomStart;
		}

		const uint pixelIndex = pixelIndexBase + (pixelIndexOffset + pixelIndexRandomStart) % RANDOM_OCL_WORK_SIZE;
		if (pixelIndex >= filmRegionPixelCount) {
			// Skip the pixels out of the film sub region
			continue;
		}
		
		const uint subRegionWidth = filmSubRegion1 - filmSubRegion0 + 1;
		const uint pixelX = filmSubRegion0 + (pixelIndex % subRegionWidth);
		const uint pixelY = filmSubRegion2 + (pixelIndex / subRegionWidth);

		if (filmNoise) {
			const float adaptiveStrength = samplerSharedData->adaptiveStrength;

			if (adaptiveStrength > 0.f) {
				// Pixels are sampled in accordance with how far from convergence they are
				const float noise = filmNoise[pixelX + pixelY * filmWidth];

				// Factor user driven importance sampling too
				float threshold;
				if (filmUserImportance) {
					const float userImportance = filmUserImportance[pixelX + pixelY * filmWidth];

					// Noise is initialized to INFINITY at start
					if (isinf(noise))
						threshold = userImportance;
					else
						threshold = (userImportance > 0.f) ? Lerp(samplerSharedData->adaptiveUserImportanceWeight, noise, userImportance) : 0.f;
				} else
					threshold = noise;

				// The floor for the pixel importance is given by the adaptiveness strength
				threshold = fmax(threshold, 1.f - adaptiveStrength);

				if (Rnd_FloatValue(seed) > threshold) {
					// Skip this pixel and try the next one
					continue;
				}
			}
		}

		// Initialize IDX_SCREEN_X and IDX_SCREEN_Y sample

		samplesData[IDX_SCREEN_X] = pixelX + Rnd_FloatValue(seed);
		samplesData[IDX_SCREEN_Y] = pixelY + Rnd_FloatValue(seed);
		break;
	}

	// Save the new value
	sample->pixelIndexOffset = pixelIndexOffset;
}

OPENCL_FORCE_INLINE void RandomSampler_NextSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	RandomSampler_InitNewSample(taskConfig,
			filmNoise,
			filmUserImportance,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3
			SAMPLER_PARAM);
}

OPENCL_FORCE_INLINE bool RandomSampler_Init(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__global RandomSamplerSharedData *samplerSharedData = (__global RandomSamplerSharedData *)samplerSharedDataBuff;
	__global RandomSample *samples = (__global RandomSample *)samplesBuff;
	__global RandomSample *sample = &samples[gid];

	if (gid == 0) {
		__global RandomSamplerSharedData *samplerSharedData = (__global RandomSamplerSharedData *)samplerSharedDataBuff;

		samplerSharedData->pixelBucketIndex = 0;
	}
	sample->pixelIndexOffset = RANDOM_OCL_WORK_SIZE;

	RandomSampler_NextSample(taskConfig,
			filmNoise,
			filmUserImportance,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3
			SAMPLER_PARAM);

	return true;
}
