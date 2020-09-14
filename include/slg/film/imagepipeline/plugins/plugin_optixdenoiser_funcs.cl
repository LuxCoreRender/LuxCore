#line 2 "plugin_optixdenoiser_funcs.cl"

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
// OptixDenoiserPlugin_BufferSetUp
//------------------------------------------------------------------------------

__kernel void OptixDenoiserPlugin_BufferSetUp(
		const uint filmWidth, const uint filmHeight,
		__global float *src,
		__global float *dst) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const uint index4 = gid * 4;
	const float x = src[index4];
	const float y = src[index4 + 1];
	const float z = src[index4 + 2];
	const float w = 1.f / src[index4 + 3];

	const uint index3 = gid * 3;
	dst[index3] = x * w;
	dst[index3 + 1] = y * w;
	dst[index3 + 2] = z * w;
}
