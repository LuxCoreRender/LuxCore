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
	return Rnd_FloatValue(seed);
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
#if defined(RENDER_ENGINE_RTPATHOCL)
	// Check if I'm in preview phase
	if (sample->currentTilePass < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP) {
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
		Film_AddSample(sample->result.pixelX, sample->result.pixelY,
			&sample->result, 1.f
			FILM_PARAM);
#else
	Film_AddSample(sample->result.pixelX, sample->result.pixelY,
			&sample->result, 1.f
			FILM_PARAM);
#endif
}

void Sampler_NextSample(
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	// sampleData[] is not used at all in random sampler
}

void Sampler_Init(Seed *seed, __global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	Sampler_NextSample(seed, samplerSharedData, sample, sampleData, filmWidth, filmHeight,
			filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3);
}

#endif
