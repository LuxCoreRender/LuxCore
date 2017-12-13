#line 2 "plugin_vignetting_funcs.cl"

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
// VignettingPlugin_Apply
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void VignettingPlugin_Apply(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global uint *channel_FRAMEBUFFER_MASK,
		const float scale) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const uint maskValue = channel_FRAMEBUFFER_MASK[gid];
	if (maskValue) {
		const uint x = gid % filmWidth;
		const uint y = gid / filmWidth;
		const float nx = x / (float)filmWidth;
		const float ny = y / (float)filmHeight;
		const float xOffset = (nx - .5f) * 2.f;
		const float yOffset = (ny - .5f) * 2.f;
		const float tOffset = sqrt(xOffset * xOffset + yOffset * yOffset);

		// Normalize to range [0.f - 1.f]
		const float invOffset = 1.f - (fabs(tOffset) * 1.42f);
		float vWeight = mix(1.f - scale, 1.f, invOffset);

		__global float *pixel = &channel_IMAGEPIPELINE[gid * 3];
		pixel[0] *= vWeight;
		pixel[1] *= vWeight;
		pixel[2] *= vWeight;
	}
}
