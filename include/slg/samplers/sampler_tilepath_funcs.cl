#line 2 "sampler_tilepath_funcs.cl"

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
// Morton related functions
//------------------------------------------------------------------------------

// Morton decode from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

// Inverse of Part1By1 - "delete" all odd-indexed bits

OPENCL_FORCE_INLINE uint Compact1By1(uint x) {
	x &= 0x55555555;					// x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
	x = (x ^ (x >> 1)) & 0x33333333;	// x = --fe --dc --ba --98 --76 --54 --32 --10
	x = (x ^ (x >> 2)) & 0x0f0f0f0f;	// x = ---- fedc ---- ba98 ---- 7654 ---- 3210
	x = (x ^ (x >> 4)) & 0x00ff00ff;	// x = ---- ---- fedc ba98 ---- ---- 7654 3210
	x = (x ^ (x >> 8)) & 0x0000ffff;	// x = ---- ---- ---- ---- fedc ba98 7654 3210
	return x;
}

// Inverse of Part1By2 - "delete" all bits not at positions divisible by 3

OPENCL_FORCE_INLINE uint Compact1By2(uint x) {
	x &= 0x09249249;					// x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
	x = (x ^ (x >> 2)) & 0x030c30c3;	// x = ---- --98 ---- 76-- --54 ---- 32-- --10
	x = (x ^ (x >> 4)) & 0x0300f00f;	// x = ---- --98 ---- ---- 7654 ---- ---- 3210
	x = (x ^ (x >> 8)) & 0xff0000ff;	// x = ---- --98 ---- ---- ---- ---- 7654 3210
	x = (x ^ (x >> 16)) & 0x000003ff;	// x = ---- ---- ---- ---- ---- --98 7654 3210
	return x;
}

OPENCL_FORCE_INLINE uint DecodeMorton2X(const uint code) {
	return Compact1By1(code >> 0);
}

OPENCL_FORCE_INLINE uint DecodeMorton2Y(const uint code) {
	return Compact1By1(code >> 1);
}

//------------------------------------------------------------------------------
// TilePath Sampler Kernel
//------------------------------------------------------------------------------

#define TILEPATHSAMPLER_TOTAL_U_SIZE 2

OPENCL_FORCE_INLINE float TilePathSampler_GetSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint index
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__global float *samplesData = &samplesDataBuff[gid * TILEPATHSAMPLER_TOTAL_U_SIZE];

	switch (index) {
		case IDX_SCREEN_X:
			return samplesData[IDX_SCREEN_X];
		case IDX_SCREEN_Y:
			return samplesData[IDX_SCREEN_Y];
		default: {
#if defined(RENDER_ENGINE_RTPATHOCL)
			return Rnd_FloatValue(seed);
#else
			__global const uint* restrict sobolDirections = (__global const uint* restrict)samplerSharedDataBuff;

			__global TilePathSample *samples = (__global TilePathSample *)samplesBuff;
			__global TilePathSample *sample = &samples[gid];

			return SobolSequence_GetSample(sobolDirections, sample->pass,
					sample->rngPass, sample->rng0, sample->rng1, index);	
#endif
		}
	}
}

OPENCL_FORCE_INLINE void TilePathSampler_SplatSample(
		__constant const GPUTaskConfiguration* restrict taskConfig
		SAMPLER_PARAM_DECL
		FILM_PARAM_DECL
		) {
	const size_t gid = get_global_id(0);
	__global TilePathSample *samples = (__global TilePathSample *)samplesBuff;
	__global TilePathSample *sample = &samples[gid];
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

#if defined(RENDER_ENGINE_RTPATHOCL)
	// Check if I'm in preview phase
	if (sample->pass < taskConfig->renderEngine.rtpathocl.previewResolutionReductionStep) {
		// I have to copy the current pixel to fill the assigned square
		for (uint y = 0; y < taskConfig->renderEngine.rtpathocl.previewResolutionReduction; ++y) {
			for (uint x = 0; x < taskConfig->renderEngine.rtpathocl.previewResolutionReduction; ++x) {
				// The sample weight is very low so this value is rapidly replaced
				// during normal rendering
				const uint px = sampleResult->pixelX + x;
				const uint py = sampleResult->pixelY + y;
				// px and py are unsigned so there is no need to check if they are >= 0
				if ((px < filmWidth) && (py < filmHeight)) {
					Film_AddSample(px, py,
							sampleResult, .001f
							FILM_PARAM);
				}
			}
		}
	} else
#endif
		Film_AddSample(sampleResult->pixelX, sampleResult->pixelY,
				sampleResult, 1.f
				FILM_PARAM);
}

OPENCL_FORCE_INLINE void TilePathSampler_NextSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	// TilePathSampler_NextSample() is not used in TILEPATHSAMPLER
}

OPENCL_FORCE_INLINE bool TilePathSampler_Init(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	const size_t gid = get_global_id(0);
	__global TilePathSamplerSharedData *samplerSharedData = (__global TilePathSamplerSharedData *)samplerSharedDataBuff;
	__global TilePathSample *samples = (__global TilePathSample *)samplesBuff;
	__global TilePathSample *sample = &samples[gid];
	__global float *samplesData = &samplesDataBuff[gid * TILEPATHSAMPLER_TOTAL_U_SIZE];

#if defined(RENDER_ENGINE_RTPATHOCL)
	// 1 thread for each pixel

	uint pixelX, pixelY;
	if (samplerSharedData->tilePass < taskConfig->renderEngine.rtpathocl.previewResolutionReductionStep) {
		const uint samplesPerRow = filmWidth / taskConfig->renderEngine.rtpathocl.previewResolutionReduction;
		const uint subPixelX = gid % samplesPerRow;
		const uint subPixelY = gid / samplesPerRow;

		pixelX = subPixelX * taskConfig->renderEngine.rtpathocl.previewResolutionReduction;
		pixelY = subPixelY * taskConfig->renderEngine.rtpathocl.previewResolutionReduction;
	} else {
		const uint samplesPerRow = filmWidth / taskConfig->renderEngine.rtpathocl.resolutionReduction;
		const uint subPixelX = gid % samplesPerRow;
		const uint subPixelY = gid / samplesPerRow;

		pixelX = subPixelX * taskConfig->renderEngine.rtpathocl.resolutionReduction;
		pixelY = subPixelY * taskConfig->renderEngine.rtpathocl.resolutionReduction;

		const uint pixelsCount = taskConfig->renderEngine.rtpathocl.resolutionReduction;
		const uint pixelsCount2 = pixelsCount * pixelsCount;

		// Rendering according a Morton curve
		const uint pixelIndex = samplerSharedData->tilePass % pixelsCount2;
		const uint mortonX = DecodeMorton2X(pixelIndex);
		const uint mortonY = DecodeMorton2Y(pixelIndex);

		pixelX += mortonX;
		pixelY += mortonY;
	}

	if ((pixelX >= samplerSharedData->tileWidth) || (pixelY >= samplerSharedData->tileHeight))
		return false;

	sample->pass = samplerSharedData->tilePass;

	samplesData[IDX_SCREEN_X] = pixelX + Rnd_FloatValue(seed);
	samplesData[IDX_SCREEN_Y] = pixelY + Rnd_FloatValue(seed);
#else
	// aaSamples * aaSamples threads for each pixel

	const uint aaSamples2 = Sqr(samplerSharedData->aaSamples);

	if (gid >= filmWidth * filmHeight * aaSamples2)
		return false;

	const uint pixelIndex = gid / aaSamples2;

	const uint pixelX = pixelIndex % filmWidth;
	const uint pixelY = pixelIndex / filmWidth;
	if ((pixelX >= samplerSharedData->tileWidth) || (pixelY >= samplerSharedData->tileHeight))
		return false;

	// Note: sample->rngPass, sample->rng0 and sample->rng1 are initialize by the CPU

	sample->pass = samplerSharedData->tilePass * aaSamples2 + gid % aaSamples2;

	__global const uint* restrict sobolDirections = (__global const uint* restrict)samplerSharedDataBuff;
	samplesData[IDX_SCREEN_X] = pixelX + SobolSequence_GetSample(sobolDirections, sample->pass,
			sample->rngPass, sample->rng0, sample->rng1, IDX_SCREEN_X);
	samplesData[IDX_SCREEN_Y] = pixelY + SobolSequence_GetSample(sobolDirections, sample->pass,
			sample->rngPass, sample->rng0, sample->rng1, IDX_SCREEN_Y);
#endif

	return true;
}
