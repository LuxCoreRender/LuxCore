#line 2 "sampler_random_funcs.cl"

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

OPENCL_FORCE_INLINE void RandomSamplerSharedData_GetNewBucket(__global RandomSamplerSharedData *samplerSharedData,
		const uint bucketCount, uint *newBucketIndex) {
	*newBucketIndex = atomic_inc(&samplerSharedData->bucketIndex) % bucketCount;
}

OPENCL_FORCE_INLINE void RandomSampler_InitNewSample(__constant const GPUTaskConfiguration* restrict taskConfig,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__constant const Sampler *sampler = &taskConfig->sampler;
	__global RandomSamplerSharedData *samplerSharedData = (__global RandomSamplerSharedData *)samplerSharedDataBuff;
	__global RandomSample *samples = (__global RandomSample *)samplesBuff;
	__global RandomSample *sample = &samples[gid];
	__global float *samplesData = &samplesDataBuff[gid * RANDOMSAMPLER_TOTAL_U_SIZE];

	const uint bucketSize = sampler->sobol.bucketSize;
	const uint tileSize = sampler->sobol.tileSize;
	const uint superSampling = sampler->sobol.superSampling;
	const uint overlapping = sampler->sobol.overlapping;
	
	const uint subRegionWidth = filmSubRegion1 - filmSubRegion0 + 1;
	const uint subRegionHeight = filmSubRegion3 - filmSubRegion2 + 1;

	const uint tiletWidthCount = (subRegionWidth + tileSize - 1) / tileSize;
	const uint tileHeightCount = (subRegionHeight + tileSize - 1) / tileSize;

	const uint bucketCount = overlapping * (tiletWidthCount * tileSize * tileHeightCount * tileSize + bucketSize - 1) / bucketSize;

	// Pick the pixel to render

	uint bucketIndex = sample->bucketIndex;
	uint pixelOffset = sample->pixelOffset;
	uint passOffset = sample->passOffset;

	for (;;) {
		passOffset++;
		if (passOffset >= superSampling) {
			pixelOffset++;
			passOffset = 0;

			if (pixelOffset >= bucketSize) {
				// Ask for a new bucket
				RandomSamplerSharedData_GetNewBucket(samplerSharedData, bucketCount,
						&bucketIndex);

				sample->bucketIndex = bucketIndex;
				pixelOffset = 0;
				passOffset = 0;
			}
		}

		// Transform the bucket index in a pixel coordinate

		const uint pixelBucketIndex = (bucketIndex / overlapping) * bucketSize + pixelOffset;
		const uint mortonCurveOffset = pixelBucketIndex % (tileSize * tileSize);
		const uint pixelTileIndex = pixelBucketIndex / (tileSize * tileSize);

		const uint subRegionPixelX = (pixelTileIndex % tiletWidthCount) * tileSize + DecodeMorton2X(mortonCurveOffset);
		const uint subRegionPixelY = (pixelTileIndex / tiletWidthCount) * tileSize + DecodeMorton2Y(mortonCurveOffset);
		if ((subRegionPixelX >= subRegionWidth) || (subRegionPixelY >= subRegionHeight)) {
			// Skip the pixels out of the film sub region
			continue;
		}

		const uint pixelX = filmSubRegion0 + subRegionPixelX;
		const uint pixelY = filmSubRegion2 + subRegionPixelY;

		if (filmNoise) {
			const float adaptiveStrength = sampler->sobol.adaptiveStrength;

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
						threshold = (userImportance > 0.f) ? Lerp(sampler->sobol.adaptiveUserImportanceWeight, noise, userImportance) : 0.f;
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
	
	// Save the new values
	sample->pixelOffset = pixelOffset;
	sample->passOffset = passOffset;
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
	__constant const Sampler *sampler = &taskConfig->sampler;
	__global RandomSample *samples = (__global RandomSample *)samplesBuff;
	__global RandomSample *sample = &samples[gid];

	const uint bucketSize = sampler->random.bucketSize;
	sample->pixelOffset = bucketSize * bucketSize;
	sample->passOffset = sampler->sobol.superSampling;

	RandomSampler_NextSample(taskConfig,
			filmNoise,
			filmUserImportance,
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3
			SAMPLER_PARAM);

	return true;
}
