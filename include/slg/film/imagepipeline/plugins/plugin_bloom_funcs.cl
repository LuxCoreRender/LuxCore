#line 2 "plugin_bloom_funcs.cl"

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
// BloomFilterPlugin_FilterX
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void BloomFilterPlugin_FilterX(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global float *bloomBuffer,
		__global float *bloomBufferTmp,
		__global float *bloomFilter,
		const uint bloomWidth) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const uint x = gid % filmWidth;
	const uint y = gid / filmWidth;

	// Check if the pixel has received any sample
	if (!isinf(channel_IMAGEPIPELINE[gid * 3])) {
		// Compute bloom for pixel (x, y)
		// Compute extent of pixels contributing bloom
		const uint x0 = max(x, bloomWidth) - bloomWidth;
		const uint x1 = min(x + bloomWidth, filmWidth - 1);

		float sumWt = 0.f;
		const uint by = y;
		float3 pixel = 0.f;
		for (uint bx = x0; bx <= x1; ++bx) {
			const uint bloomOffset = bx + by * filmWidth;

			// Check if the pixel has received any sample
			if (!isinf(channel_IMAGEPIPELINE[bloomOffset])) {
				// Accumulate bloom from pixel (bx, by)
				const uint dist2 = (x - bx) * (x - bx) + (y - by) * (y - by);
				const float wt = bloomFilter[dist2];
				if (wt == 0.f)
					continue;

				sumWt += wt;
				const uint bloomOffset3 = bloomOffset * 3;
				pixel.s0 += wt * channel_IMAGEPIPELINE[bloomOffset3];
				pixel.s1 += wt * channel_IMAGEPIPELINE[bloomOffset3 + 1];
				pixel.s2 += wt * channel_IMAGEPIPELINE[bloomOffset3 + 2];
			}
		}
		if (sumWt > 0.f)
			pixel /= sumWt;
		
		__global float *dst = &bloomBufferTmp[(x + y * filmWidth) * 3];
		dst[0] = pixel.s0;
		dst[1] = pixel.s1;
		dst[2] = pixel.s2;
	}
}

//------------------------------------------------------------------------------
// BloomFilterPlugin_FilterY
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void BloomFilterPlugin_FilterY(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global float *bloomBuffer,
		__global float *bloomBufferTmp,
		__global float *bloomFilter,
		const uint bloomWidth) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const uint x = gid % filmWidth;
	const uint y = gid / filmWidth;

	if (!isinf(channel_IMAGEPIPELINE[gid * 3])) {
		// Compute bloom for pixel (x, y)
		// Compute extent of pixels contributing bloom
		const uint y0 = max(y, bloomWidth) - bloomWidth;
		const uint y1 = min(y + bloomWidth, filmHeight - 1);

		float sumWt = 0.f;
		const uint bx = x;
		float3 pixel = 0.f;
		for (uint by = y0; by <= y1; ++by) {
			const uint bloomOffset = bx + by * filmWidth;

			if (!isinf(channel_IMAGEPIPELINE[bloomOffset * 3])) {
				// Accumulate bloom from pixel (bx, by)
				const uint dist2 = (x - bx) * (x - bx) + (y - by) * (y - by);
				const float wt = bloomFilter[dist2];
				if (wt == 0.f)
					continue;

				const uint bloomOffset = bx + by * filmWidth;
				sumWt += wt;
				const uint bloomOffset3 = bloomOffset * 3;
				pixel.s0 += wt * bloomBufferTmp[bloomOffset3];
				pixel.s1 += wt * bloomBufferTmp[bloomOffset3 + 1];
				pixel.s2 += wt * bloomBufferTmp[bloomOffset3 + 2];
			}
		}

		if (sumWt > 0.f)
			pixel /= sumWt;

		__global float *dst = &bloomBuffer[(x + y * filmWidth) * 3];
		dst[0] = pixel.s0;
		dst[1] = pixel.s1;
		dst[2] = pixel.s2;
	}
}

//------------------------------------------------------------------------------
// BloomFilterPlugin_Merge
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void BloomFilterPlugin_Merge(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global float *bloomBuffer,
		const float bloomWeight) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	if (!isinf(channel_IMAGEPIPELINE[gid * 3])) {
		__global float *src = &channel_IMAGEPIPELINE[gid * 3];
		__global float *dst = &bloomBuffer[gid * 3];
		
		src[0] = mix(src[0], dst[0], bloomWeight);
		src[1] = mix(src[1], dst[1], bloomWeight);
		src[2] = mix(src[2], dst[2], bloomWeight);
	}
}
