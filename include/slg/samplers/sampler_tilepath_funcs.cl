#line 2 "sampler_tilepath_funcs.cl"

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
// Morton related functions
//------------------------------------------------------------------------------

#if defined(RENDER_ENGINE_RTPATHOCL)

// Morton decode from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

// Inverse of Part1By1 - "delete" all odd-indexed bits

uint Compact1By1(uint x) {
	x &= 0x55555555;					// x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
	x = (x ^ (x >> 1)) & 0x33333333;	// x = --fe --dc --ba --98 --76 --54 --32 --10
	x = (x ^ (x >> 2)) & 0x0f0f0f0f;	// x = ---- fedc ---- ba98 ---- 7654 ---- 3210
	x = (x ^ (x >> 4)) & 0x00ff00ff;	// x = ---- ---- fedc ba98 ---- ---- 7654 3210
	x = (x ^ (x >> 8)) & 0x0000ffff;	// x = ---- ---- ---- ---- fedc ba98 7654 3210
	return x;
}

// Inverse of Part1By2 - "delete" all bits not at positions divisible by 3

uint Compact1By2(uint x) {
	x &= 0x09249249;					// x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
	x = (x ^ (x >> 2)) & 0x030c30c3;	// x = ---- --98 ---- 76-- --54 ---- 32-- --10
	x = (x ^ (x >> 4)) & 0x0300f00f;	// x = ---- --98 ---- ---- 7654 ---- ---- 3210
	x = (x ^ (x >> 8)) & 0xff0000ff;	// x = ---- --98 ---- ---- ---- ---- 7654 3210
	x = (x ^ (x >> 16)) & 0x000003ff;	// x = ---- ---- ---- ---- ---- --98 7654 3210
	return x;
}

uint DecodeMorton2X(const uint code) {
	return Compact1By1(code >> 0);
}

uint DecodeMorton2Y(const uint code) {
	return Compact1By1(code >> 1);
}

#endif

//------------------------------------------------------------------------------
// TilePath Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 3)

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
#if defined(RENDER_ENGINE_RTPATHOCL)
			return Rnd_FloatValue(seed);
#else
			return SobolSequence_GetSample(sample, index);
#endif
	}
}

float Sampler_GetSamplePathVertex(Seed *seed, __global Sample *sample,
		__global float *sampleDataPathVertexBase,
		const uint depth, const uint index) {
#if defined(RENDER_ENGINE_RTPATHOCL)
	return Rnd_FloatValue(seed);
#else
	if (depth < SOBOL_MAX_DEPTH)
		return SobolSequence_GetSample(sample, IDX_BSDF_OFFSET + (depth - 1) * VERTEX_SAMPLE_SIZE + index);
	else
		return Rnd_FloatValue(seed);
#endif
}

void Sampler_SplatSample(
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData
		FILM_PARAM_DECL
		) {
	const size_t gid = get_global_id(0);

#if defined(RENDER_ENGINE_RTPATHOCL)
	// Check if I'm in preview phase
	if (sample->pass < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP) {
		// I have to copy the current pixel to fill the assigned square
		for (uint y = 0; y < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION; ++y) {
			for (uint x = 0; x < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION; ++x) {
				// The sample weight is very low so this value is rapidly replaced
				// during normal rendering
				const uint px = sample->result.pixelX + x;
				const uint py = sample->result.pixelY + y;
				// px and py are unsigned so there is no need to check if they are >= 0
				if ((px < filmWidth) && (py < filmHeight)) {
					Film_AddSample(px, py,
							&sample->result, .001f
							FILM_PARAM);
				}
			}
		}
	} else
#endif
		Film_AddSample(sample->result.pixelX, sample->result.pixelY,
				&sample->result, 1.f
				FILM_PARAM);
}

void Sampler_NextSample(
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	// Sampler_NextSample() is not used in TILEPATHSAMPLER
}

bool Sampler_Init(Seed *seed, __global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3,
		// cameraFilmWidth/cameraFilmHeight and filmWidth/filmHeight are usually
		// the same. They are different when doing tile rendering
		const uint cameraFilmWidth, const uint cameraFilmHeight,
		const uint tileStartX, const uint tileStartY,
		const uint tileWidth, const uint tileHeight,
		const uint tilePass, const uint aaSamples) {
	const size_t gid = get_global_id(0);

#if defined(RENDER_ENGINE_RTPATHOCL)
	// 1 thread for each pixel

	uint pixelX, pixelY;
	if (tilePass < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP) {
		const uint samplesPerRow = filmWidth / PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION;
		const uint subPixelX = gid % samplesPerRow;
		const uint subPixelY = gid / samplesPerRow;

		pixelX = subPixelX * PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION;
		pixelY = subPixelY * PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION;
	} else {
		const uint samplesPerRow = filmWidth / PARAM_RTPATHOCL_RESOLUTION_REDUCTION;
		const uint subPixelX = gid % samplesPerRow;
		const uint subPixelY = gid / samplesPerRow;

		pixelX = subPixelX * PARAM_RTPATHOCL_RESOLUTION_REDUCTION;
		pixelY = subPixelY * PARAM_RTPATHOCL_RESOLUTION_REDUCTION;

		const uint pixelsCount = PARAM_RTPATHOCL_RESOLUTION_REDUCTION;
		const uint pixelsCount2 = pixelsCount * pixelsCount;

		// Rendering according a Morton curve
		const uint pixelIndex = tilePass % pixelsCount2;
		const uint mortonX = DecodeMorton2X(pixelIndex);
		const uint mortonY = DecodeMorton2Y(pixelIndex);

		pixelX += mortonX;
		pixelY += mortonY;
	}

	if ((pixelX >= tileWidth) || (pixelY >= tileHeight))
		return false;

	sample->pass = tilePass;

	sampleData[IDX_SCREEN_X] = (pixelX + Rnd_FloatValue(seed)) / filmWidth;
	sampleData[IDX_SCREEN_Y] = (pixelY + Rnd_FloatValue(seed)) / filmHeight;
#else
	// aaSamples * aaSamples threads for each pixel

	const uint aaSamples2 = aaSamples * aaSamples;

	if (gid >= filmWidth * filmHeight * aaSamples2)
		return false;

	const uint pixelIndex = gid / aaSamples2;

	const uint pixelX = pixelIndex % filmWidth;
	const uint pixelY = pixelIndex / filmWidth;
	if ((pixelX >= tileWidth) || (pixelY >= tileHeight))
		return false;

	const uint sharedDataIndex = (tileStartX + pixelX) + (tileStartY + pixelY) * cameraFilmWidth;
	// Limit the number of pass skipped
	sample->rngPass = samplerSharedData[sharedDataIndex].rngPass % 512;
	sample->rng0 = samplerSharedData[sharedDataIndex].rng0;
	sample->rng1 = samplerSharedData[sharedDataIndex].rng1;

	sample->pass = tilePass * aaSamples2 + gid % aaSamples2;
	
	sampleData[IDX_SCREEN_X] = (pixelX + SobolSequence_GetSample(sample, IDX_SCREEN_X)) / filmWidth;
	sampleData[IDX_SCREEN_Y] = (pixelY + SobolSequence_GetSample(sample, IDX_SCREEN_Y)) / filmHeight;
#endif

	return true;
}

#endif
