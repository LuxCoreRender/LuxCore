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

#if (PARAM_SAMPLER_TYPE == 0)

uint SamplerSharedData_GetNewPixelBucketIndex(__global SamplerSharedData *samplerSharedData) {
	return atomic_inc(&samplerSharedData->pixelBucketIndex);
}

void Sampler_InitNewSample(Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleDataPathBase,
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
		__global float *filmConvergence,
#endif
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	const uint filmRegionPixelCount = (filmSubRegion1 - filmSubRegion0 + 1) * (filmSubRegion3 - filmSubRegion2 + 1);

#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
	const float adaptiveStrength = samplerSharedData->adaptiveStrength;
#endif

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
			pixelIndexBase = (SamplerSharedData_GetNewPixelBucketIndex(samplerSharedData) %
					(filmRegionPixelCount / RANDOM_OCL_WORK_SIZE)) * RANDOM_OCL_WORK_SIZE;
			sample->pixelIndexBase = pixelIndexBase;

			pixelIndexOffset = 0;

			pixelIndexRandomStart = Floor2UInt(Rnd_FloatValue(seed) * RANDOM_OCL_WORK_SIZE);
			sample->pixelIndexRandomStart = pixelIndexRandomStart;
		}

		const uint pixelIndex = (pixelIndexBase + pixelIndexOffset + pixelIndexRandomStart) % filmRegionPixelCount;
		const uint subRegionWidth = filmSubRegion1 - filmSubRegion0 + 1;
		const uint pixelX = filmSubRegion0 + (pixelIndex % subRegionWidth);
		const uint pixelY = filmSubRegion2 + (pixelIndex / subRegionWidth);

#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
		if ((adaptiveStrength > 0.f) && (filmConvergence[pixelX + pixelY * filmWidth] == 0.f)) {
			// This pixel is already under the convergence threshold. Check if to
			// render or not
			if (Rnd_FloatValue(seed) < adaptiveStrength) {
				// Skip this pixel and try the next one
				continue;
			}
		}
#endif

		// Initialize IDX_SCREEN_X and IDX_SCREEN_Y sample

		sampleDataPathBase[IDX_SCREEN_X] = pixelX + Rnd_FloatValue(seed);
		sampleDataPathBase[IDX_SCREEN_Y] = pixelY + Rnd_FloatValue(seed);
		break;
	}

	// Save the new value
	sample->pixelIndexOffset = pixelIndexOffset;
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
	// This is never used in Random sampler
	//
	// The depth used here is counted from the first hit point of the path
	// vertex (so it is effectively depth - 1)
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
			return Rnd_FloatValue(seed);
	}
}

float Sampler_GetSamplePathVertex(Seed *seed, __global Sample *sample,
		__global float *sampleDataPathVertexBase,
		const uint depth, const uint index) {
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
		__global Sample *sample, __global float *sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
		__global float *filmConvergence,
#endif
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	Sampler_InitNewSample(seed, samplerSharedData, sample, sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
			filmConvergence,
#endif
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);
}

bool Sampler_Init(Seed *seed, __global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
		__global float *filmConvergence,
#endif
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	if (get_global_id(0) == 0)
		samplerSharedData->pixelBucketIndex = 0;
	sample->pixelIndexOffset = RANDOM_OCL_WORK_SIZE;

	Sampler_NextSample(seed, samplerSharedData, sample, sampleData,
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
			filmConvergence,
#endif
			filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);

	return true;
}

#endif
