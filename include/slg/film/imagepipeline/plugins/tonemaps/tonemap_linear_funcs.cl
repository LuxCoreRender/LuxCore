#line 2 "linear_funcs.cl"

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
// LinearToneMap_Apply
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void LinearToneMap_Apply(const uint filmWidth, const uint filmHeight,
		__global float *channel_RGB_TONEMAPPED,
		__global uint *channel_FRAMEBUFFER_MASK,
		const float scale) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	const uint maskValue = channel_FRAMEBUFFER_MASK[gid];
	if (gid) {
		const uint index = gid * 3;
		channel_FRAMEBUFFER_MASK[index] *= scale;
		channel_FRAMEBUFFER_MASK[index + 1] *= scale;
		channel_FRAMEBUFFER_MASK[index + 2] *= scale;
	}
}