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
// TilePath Sampler Kernel
//------------------------------------------------------------------------------

#define TILEPATHSAMPLER_TOTAL_U_SIZE 2

OPENCL_FORCE_INLINE __global const uint* restrict TilePathSampler_GetSobolDirectionsPtr(
		__global TilePathSamplerSharedData *samplerSharedData) {
	// Sobol directions array is appended at the end of slg::ocl::TilePathSamplerSharedData
	return (__global uint *)(
			(__global char *)samplerSharedData +
			sizeof(TilePathSamplerSharedData));
}

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
			__global TilePathSamplerSharedData *samplerSharedData = (__global TilePathSamplerSharedData *)samplerSharedDataBuff;
			__global const uint* restrict sobolDirections = TilePathSampler_GetSobolDirectionsPtr(samplerSharedData);

			__global TilePathSample *samples = (__global TilePathSample *)samplesBuff;
			__global TilePathSample *sample = &samples[gid];

			return SobolSequence_GetSample(sobolDirections, sample->pass + SOBOL_STARTOFFSET,
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
	__global SampleResult *sampleResult = &sampleResultsBuff[gid];

#if defined(RENDER_ENGINE_RTPATHOCL)
	__global TilePathSample *samples = (__global TilePathSample *)samplesBuff;
	__global TilePathSample *sample = &samples[gid];

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

	// Initialize rng0, rng1 and rngPass
	const uint pixelRngGenSeed = (samplerSharedData->tileStartX + pixelX + (samplerSharedData->tileStartY + pixelY) * samplerSharedData->cameraFilmWidth + 1) *
			(samplerSharedData->multipassIndexToRender + 1);
	Seed pixelSeed;
	Rnd_Init(pixelRngGenSeed, &pixelSeed);

	sample->rngPass = Rnd_UintValue(&pixelSeed);
	sample->rng0 = Rnd_FloatValue(&pixelSeed);
	sample->rng1 = Rnd_FloatValue(&pixelSeed);

	sample->pass = samplerSharedData->tilePass * aaSamples2 + gid % aaSamples2;

	__global const uint* restrict sobolDirections = TilePathSampler_GetSobolDirectionsPtr(samplerSharedData);

	samplesData[IDX_SCREEN_X] = pixelX + SobolSequence_GetSample(sobolDirections, sample->pass + SOBOL_STARTOFFSET,
			sample->rngPass, sample->rng0, sample->rng1, IDX_SCREEN_X);
	samplesData[IDX_SCREEN_Y] = pixelY + SobolSequence_GetSample(sobolDirections, sample->pass + SOBOL_STARTOFFSET,
			sample->rngPass, sample->rng0, sample->rng1, IDX_SCREEN_Y);
#endif

	return true;
}
