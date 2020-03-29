#line 2 "tonemap_autolinear_funcs.cl"

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
// AutoLinearToneMap_Apply
//------------------------------------------------------------------------------

__kernel void AutoLinearToneMap_Apply(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		const float gamma, __global float *totalRGB) {
	const size_t gid = get_global_id(0);
	const uint pixelCount = filmWidth * filmHeight;
	if (gid >= pixelCount)
		return;

	// Check if the pixel has received any sample
	if (!isinf(channel_IMAGEPIPELINE[gid * 3])) {
		const float totalLuminance = .212671f * totalRGB[0] + .715160f * totalRGB[1] + .072169f * totalRGB[2];
		const float avgLuminance = totalLuminance / pixelCount;
		const float scale = (avgLuminance > 0.f) ? (1.25f / avgLuminance * native_powr(118.f / 255.f, gamma)) : 1.f;
		
		__global float *pixel = &channel_IMAGEPIPELINE[gid * 3];
		pixel[0] *= scale;
		pixel[1] *= scale;
		pixel[2] *= scale;
	}
}

//------------------------------------------------------------------------------
// REDUCE_OP & ACCUM_OP (used by tonemap_reduce_funcs.cl)
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 REDUCE_OP(const float3 a, const float3 b) {
	if (Spectrum_IsNanOrInf(b))
		return a;
	else
		return a + b;
}

OPENCL_FORCE_INLINE float3 ACCUM_OP(const float3 a, const float3 b) {
	return a + b;
}
