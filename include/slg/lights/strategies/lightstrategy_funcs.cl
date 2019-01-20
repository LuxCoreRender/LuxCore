#line 2 "lightstrategy_funcs.cl"

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

OPENCL_FORCE_INLINE uint LightStrategy_SampleLights(
		__global const float* restrict lightsDistribution1D,
		__global const DLSCacheEntry* restrict dlscAllEntries,
		__global const uint* restrict dlscDistributionIndexToLightIndex,
		__global const float* restrict dlscDistributions,
		__global const IndexBVHArrayNode* restrict dlscBVHNodes,
		const float dlscRadius2, const float dlscNormalCosAngle,
		const float3 p, const float3 n,
#if defined(PARAM_HAS_VOLUMES)
		const bool isVolume,
#endif
		const float u, float *pdf) {
#if !defined(RENDER_ENGINE_RTPATHOCL)
	if (dlscAllEntries) {
		// DLSC strategy

		// Check if a cache entry is available for this point
		__global const DLSCacheEntry* restrict cacheEntry = DLSCache_GetEntry(
				dlscAllEntries, dlscDistributionIndexToLightIndex,
				dlscDistributions, dlscBVHNodes,
				dlscRadius2, dlscNormalCosAngle,
				p, n
#if defined(PARAM_HAS_VOLUMES)
				, isVolume
#endif
		);

		if (cacheEntry) {
			if (DLSCacheEntry_IsDirectLightSamplingDisabled(cacheEntry))
				return NULL_INDEX;
			else {
				const uint distributionLightIndex = Distribution1D_SampleDiscrete(
						&dlscDistributions[cacheEntry->lightsDistributionOffset], u, pdf);

				if (*pdf > 0.f)
					return dlscDistributionIndexToLightIndex[cacheEntry->distributionIndexToLightIndexOffset + distributionLightIndex];
				else
					return NULL_INDEX;
			}
		} else
			return Distribution1D_SampleDiscrete(lightsDistribution1D, u, pdf);
	} else
#endif
		// All other strategies
		return Distribution1D_SampleDiscrete(lightsDistribution1D, u, pdf);
}

OPENCL_FORCE_INLINE float LightStrategy_SampleLightPdf(
		__global const float* restrict lightsDistribution1D,
		__global const DLSCacheEntry* restrict dlscAllEntries,
		__global const uint* restrict dlscDistributionIndexToLightIndex,
		__global const float* restrict dlscDistributions,
		__global const IndexBVHArrayNode* restrict dlscBVHNodes,
		const float dlscRadius2, const float dlscNormalCosAngle,
		const float3 p, const float3 n,
#if defined(PARAM_HAS_VOLUMES)
		const bool isVolume,
#endif
		const uint lightIndex) {
#if !defined(RENDER_ENGINE_RTPATHOCL)
	if (dlscAllEntries) {
		// DLSC strategy
		
		// Check if a cache entry is available for this point
		__global const DLSCacheEntry* restrict cacheEntry = DLSCache_GetEntry(
				dlscAllEntries, dlscDistributionIndexToLightIndex,
				dlscDistributions, dlscBVHNodes,
				dlscRadius2, dlscNormalCosAngle,
				p, n
#if defined(PARAM_HAS_VOLUMES)
				, isVolume
#endif
		);

		if (cacheEntry) {
			if (DLSCacheEntry_IsDirectLightSamplingDisabled(cacheEntry))
				return 0.f;
			else {
				// Look for the distribution index
				// TODO: optimize the lookup
				const uint offset = cacheEntry->distributionIndexToLightIndexOffset;
				const uint size = cacheEntry->distributionIndexToLightIndexSize;
				for (uint i = 0; i < size; ++i) {
					if (dlscDistributionIndexToLightIndex[offset + i] == lightIndex)
						return Distribution1D_Pdf_UINT(&dlscDistributions[cacheEntry->lightsDistributionOffset], i);
				}
				
				return 0.f;
			}
		} else
			return Distribution1D_Pdf_UINT(lightsDistribution1D, lightIndex);
	} else
#endif
		// All other strategies
		return Distribution1D_Pdf_UINT(lightsDistribution1D, lightIndex);
}
