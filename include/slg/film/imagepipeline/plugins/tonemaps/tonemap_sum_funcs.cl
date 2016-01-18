#line 2 "tonemap_sum_funcs.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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
// Compute the sum of all frame buffer RGB values
//------------------------------------------------------------------------------

__attribute__((reqd_work_group_size(64, 1, 1))) __kernel void SumRGBValuesReduce(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_RGB_TONEMAPPED,
		__global uint *channel_FRAMEBUFFER_MASK,
		__global float *accumBuffer) {
	// Workgroup local shared memory
	__local float3 localMemBuffer[64];

	const uint tid = get_local_id(0);
	const uint gid = get_global_id(0);

	const uint localSize = get_local_size(0);
	const uint pixelCount = filmWidth * filmHeight;
	localMemBuffer[tid] = 0.f;

	// Read the first pixel
	const uint stride0 = gid * 2;
	const uint maskValue0 = channel_FRAMEBUFFER_MASK[stride0];
	if (maskValue0 && (stride0 < pixelCount)) {
		const uint stride03 = stride0 * 3;
		localMemBuffer[tid].s0 += channel_RGB_TONEMAPPED[stride03];
		localMemBuffer[tid].s1 += channel_RGB_TONEMAPPED[stride03 + 1];
		localMemBuffer[tid].s2 += channel_RGB_TONEMAPPED[stride03 + 2];
	}

	// Read the second pixel
	const uint stride1 = stride0 + 1;
	const uint maskValue1 = channel_FRAMEBUFFER_MASK[stride1];
	if (maskValue1 && (stride1 < pixelCount)) {
		const uint stride13 = stride1 * 3;
		localMemBuffer[tid].s0 += channel_RGB_TONEMAPPED[stride13];
		localMemBuffer[tid].s1 += channel_RGB_TONEMAPPED[stride13 + 1];
		localMemBuffer[tid].s2 += channel_RGB_TONEMAPPED[stride13 + 2];
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	// Do reduction in local memory
	for (uint s = localSize >> 1; s > 0; s >>= 1) {
		if (tid < s)
			localMemBuffer[tid] += localMemBuffer[tid + s];

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	// Write result for this block to global memory
	if (tid == 0) {
		const uint bid = get_group_id(0) * 3;

		accumBuffer[bid] = localMemBuffer[0].s0;
		accumBuffer[bid + 1] = localMemBuffer[0].s1;
		accumBuffer[bid + 2] = localMemBuffer[0].s2;
	}
}

__attribute__((reqd_work_group_size(64, 1, 1))) __kernel void SumRGBValueAccumulate(
		const uint size,
		__global float *accumBuffer) {
	if (get_global_id(0) == 0) {
		float3 totalRGBValue = 0.f;
		for(uint i = 0; i < size; ++i) {
			const uint index = i * 3;
			totalRGBValue.s0 += accumBuffer[index];
			totalRGBValue.s1 += accumBuffer[index + 1];
			totalRGBValue.s2 += accumBuffer[index + 2];
		}

		// Write the result
		accumBuffer[0] = totalRGBValue.s0;
		accumBuffer[1] = totalRGBValue.s1;
		accumBuffer[2] = totalRGBValue.s2;
	}
}
