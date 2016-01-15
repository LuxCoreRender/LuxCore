#line 2 "plugin_cameraresponse_funcs.cl"

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
// CameraResponsePlugin_Apply
//------------------------------------------------------------------------------

// Implementation of std::upper_bound()
__global const float *std_upper_bound(__global const float *first,
		__global const float *last, const float val) {
	__global const float *it;
	uint count = last - first;
	uint step;

	while (count > 0) {
		it = first;
		step = count / 2;
		it += step;

		if (!(val < *it)) {
			first = ++it;
			count -= step + 1;
		} else
			count = step;
	}

	return first;
}

float ApplyCrf(const float point, __global const float *from, __global const float *to,
		const uint size) {
	if (point <= from[0])
		return to[0];
	if (point >= from[size - 1])
		return to[size - 1];

	const int index = std_upper_bound(from, from + size, point) - from;
	const float x1 = from[index - 1];
	const float x2 = from[index];
	const float y1 = to[index - 1];
	const float y2 = to[index];
	return mix(y1, y2, (point - x1) / (x2 - x1));
}

float3 Map(__global const float *redI, __global const float *redB, const uint redSize
#if defined(PARAM_CAMERARESPONSE_COLOR)
		, __global const float *greenI, __global const float *greenB, const uint greenSize
		, __global const float *blueI, __global const float *blueB, const uint blueSize
#endif
		, const float3 rgb) {
	float3 result;

#if defined(PARAM_CAMERARESPONSE_COLOR)
	result.s0 = ApplyCrf(rgb.s0, redI, redB, redSize);
	result.s1 = ApplyCrf(rgb.s1, greenI, greenB, greenSize);
	result.s2 = ApplyCrf(rgb.s2, blueI, blueB, blueSize);
#else
	const float y = Spectrum_Y(rgb);
	result = ApplyCrf(y, redI, redB, redSize);
#endif

	return result;
}

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void CameraResponsePlugin_Apply(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_RGB_TONEMAPPED,
		__global uint *channel_FRAMEBUFFER_MASK,
		__global const float *redI, __global const float *redB, const uint redSize
#if defined(PARAM_CAMERARESPONSE_COLOR)
		, __global const float *greenI, __global const float *greenB, const uint greenSize
		, __global const float *blueI, __global const float *blueB, const uint blueSize
#endif
		) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	const uint maskValue = channel_FRAMEBUFFER_MASK[gid];
	if (maskValue) {
		__global float *pixel = &channel_RGB_TONEMAPPED[gid * 3];
		const float3 pixelValue = (float3)(pixel[0], pixel[1], pixel[2]);
		
		const float3 newPixelValue = Map(
				redI, redB, redSize
#if defined(PARAM_CAMERARESPONSE_COLOR)
				, greenI, greenB, greenSize
				, blueI, blueB, blueSize
#endif
				, pixelValue);

		pixel[0] = newPixelValue.s0;
		pixel[1] = newPixelValue.s1;
		pixel[2] = newPixelValue.s2;
	}
}
