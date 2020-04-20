#line 2 "tonemap_reduce_funcs.cl"

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
// Compute the REDUCE_OP of all frame buffer RGB values
//------------------------------------------------------------------------------

__kernel void OpRGBValuesReduce(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global float *accumBuffer) {
	// Workgroup local shared memory
	__local float3 localMemBuffer[64];

	const uint tid = get_local_id(0);
	const uint gid = get_global_id(0);

	const uint localSize = get_local_size(0);
	const uint pixelCount = filmWidth * filmHeight;
	localMemBuffer[tid] = ZERO;

	// Read the first pixel
	const uint stride0 = gid * 2;
	const uint maskValue0 = !isinf(channel_IMAGEPIPELINE[stride0]);
	if (maskValue0 && (stride0 < pixelCount)) {
		const uint stride03 = stride0 * 3;
		localMemBuffer[tid] = REDUCE_OP(localMemBuffer[tid], VLOAD3F(&channel_IMAGEPIPELINE[stride03]));
	}

	// Read the second pixel
	const uint stride1 = stride0 + 1;
	const uint maskValue1 = !isinf(channel_IMAGEPIPELINE[stride1]);
	if (maskValue1 && (stride1 < pixelCount)) {
		const uint stride13 = stride1 * 3;
		localMemBuffer[tid] = REDUCE_OP(localMemBuffer[tid], VLOAD3F(&channel_IMAGEPIPELINE[stride13]));
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	// Do reduction in local memory
	for (uint s = localSize >> 1; s > 0; s >>= 1) {
		if (tid < s) {
			localMemBuffer[tid] = ACCUM_OP(localMemBuffer[tid], localMemBuffer[tid + s]);
		}

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	// Write result for this block to global memory
	if (tid == 0) {
		const uint bid = get_group_id(0) * 3;

		accumBuffer[bid] = localMemBuffer[0].x;
		accumBuffer[bid + 1] = localMemBuffer[0].y;
		accumBuffer[bid + 2] = localMemBuffer[0].z;
	}
}

__kernel void OpRGBValueAccumulate(
		const uint size,
		__global float *accumBuffer) {
	if (get_global_id(0) == 0) {
		float3 totalRGBValue = ZERO;
		for(uint i = 0; i < size; ++i) {
			const uint index = i * 3;
			totalRGBValue = ACCUM_OP(totalRGBValue, VLOAD3F(&accumBuffer[index]));
		}

		// Write the result
		accumBuffer[0] = totalRGBValue.x;
		accumBuffer[1] = totalRGBValue.y;
		accumBuffer[2] = totalRGBValue.z;
	}
}
