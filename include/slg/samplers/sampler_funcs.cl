#line 2 "sampler_funcs.cl"

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
// Sampler functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float Sampler_GetSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint taskIndex,
		const uint index
		SAMPLER_PARAM_DECL) {
	switch (taskConfig->sampler.type) {
		case RANDOM:
			return RandomSampler_GetSample(taskConfig, taskIndex, index SAMPLER_PARAM);
		case SOBOL:
			return SobolSampler_GetSample(taskConfig, taskIndex, index SAMPLER_PARAM);
		case METROPOLIS:
			return MetropolisSampler_GetSample(taskConfig, taskIndex, index SAMPLER_PARAM);
		case TILEPATHSAMPLER:
			return TilePathSampler_GetSample(taskConfig, taskIndex, index SAMPLER_PARAM);
		default:
			// Something has gone very wrong here
			return 0.f;
	}
}

// Cuda reports large argument size, so overrides noinline attribute anyway
OPENCL_FORCE_INLINE void Sampler_SplatSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint taskIndex
		SAMPLER_PARAM_DECL
		FILM_PARAM_DECL
		) {
	switch (taskConfig->sampler.type) {
		case RANDOM:
			return RandomSampler_SplatSample(taskConfig,
					taskIndex
					SAMPLER_PARAM
					FILM_PARAM);
		case SOBOL:
			return SobolSampler_SplatSample(taskConfig,
					taskIndex
					SAMPLER_PARAM
					FILM_PARAM);
		case METROPOLIS:
			return MetropolisSampler_SplatSample(taskConfig,
					taskIndex
					SAMPLER_PARAM
					FILM_PARAM);
		case TILEPATHSAMPLER:
			return TilePathSampler_SplatSample(taskConfig,
					taskIndex
					SAMPLER_PARAM
					FILM_PARAM);
		default:
			// Something has gone very wrong here
			return;
	}
}

OPENCL_FORCE_NOT_INLINE void Sampler_NextSample(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint taskIndex,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	switch (taskConfig->sampler.type) {
		case RANDOM:
			return RandomSampler_NextSample(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		case SOBOL:
			return SobolSampler_NextSample(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		case METROPOLIS:
			return MetropolisSampler_NextSample(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		case TILEPATHSAMPLER:
			return TilePathSampler_NextSample(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		default:
			// Something has gone very wrong here
			return;
	}
}

OPENCL_FORCE_NOT_INLINE bool Sampler_Init(
		__constant const GPUTaskConfiguration* restrict taskConfig,
		const uint taskIndex,
		__global float *filmNoise,
		__global float *filmUserImportance,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3
		SAMPLER_PARAM_DECL) {
	switch (taskConfig->sampler.type) {
		case RANDOM:
			return RandomSampler_Init(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		case SOBOL:
			return SobolSampler_Init(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		case METROPOLIS:
			return MetropolisSampler_Init(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		case TILEPATHSAMPLER:
			return TilePathSampler_Init(taskConfig,
					taskIndex,
					filmNoise,
					filmUserImportance,
					filmWidth, filmHeight,
					filmSubRegion0, filmSubRegion1,
					filmSubRegion2, filmSubRegion3
					SAMPLER_PARAM);
		default:
			// Something has gone very wrong here
			return true;
	}
}
