#line 2 "plugin_gammacorrection_funcs.cl"

/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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
// GammaCorrectionPlugin_Apply
//------------------------------------------------------------------------------

float Radiance2PixelFloat(__global float *gammaTable, const uint tableSize,
		const float x) {
	const int index = clamp(Floor2UInt(tableSize * clamp(x, 0.f, 1.f)), 0u, tableSize - 1u);
	return gammaTable[index];
}

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void GammaCorrectionPlugin_Apply(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global uint *channel_FRAMEBUFFER_MASK,
		__global float *gammaTable,
		const uint tableSize) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const uint maskValue = channel_FRAMEBUFFER_MASK[gid];
	if (maskValue) {
		__global float *pixel = &channel_IMAGEPIPELINE[gid * 3];

		pixel[0] = Radiance2PixelFloat(gammaTable, tableSize, pixel[0]);
		pixel[1] = Radiance2PixelFloat(gammaTable, tableSize, pixel[1]);
		pixel[2] = Radiance2PixelFloat(gammaTable, tableSize, pixel[2]);
	}
}
