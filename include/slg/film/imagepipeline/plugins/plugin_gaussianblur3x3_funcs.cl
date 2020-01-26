#line 2 "plugin_gaussianblur3x3_funcs.cl"

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
// GaussianBlur3x3FilterPlugin_FilterX
//------------------------------------------------------------------------------

void GaussianBlur3x3FilterPlugin_ApplyBlurFilterXR1(
		const uint filmWidth, const uint filmHeight,
		__global const float *src,
		__global float *dst,
		const float aF,
		const float bF,
		const float cF) {
	// Do left edge
	float3 a;
	float3 b = VLOAD3F(&src[0 * 3]);
	float3 c = VLOAD3F(&src[1 * 3]);

	const float leftTotF = bF + cF;
	const float3 bLeftK = bF / leftTotF;
	const float3 cLeftK = cF / leftTotF;
	VSTORE3F(bLeftK  * b + cLeftK * c, &dst[0 * 3]);

    // Main loop
	const float totF = aF + bF + cF;
	const float3 aK = aF / totF;
	const float3 bK = bF / totF;
	const float3 cK = cF / totF;

	for (uint x = 1; x < filmWidth - 1; ++x) {
		a = b;
		b = c;
		c = VLOAD3F(&src[(x + 1) * 3]);

		VSTORE3F(aK * a + bK  * b + cK * c, &dst[x * 3]);
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float3 aRightK = aF / rightTotF;
	const float3 bRightK = bF / rightTotF;
	a = b;
	b = c;
	VSTORE3F(aRightK  * a + bRightK * b, &dst[(filmWidth - 1) * 3]);
}

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void GaussianBlur3x3FilterPlugin_FilterX(
		const uint filmWidth, const uint filmHeight,
		__global const float *src,
		__global float *dst,
		const float weight) {
	const size_t gid = get_global_id(0);
	if (gid >= filmHeight)
		return;

	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	const uint index = gid * filmWidth * 3;
	GaussianBlur3x3FilterPlugin_ApplyBlurFilterXR1(filmWidth, filmHeight,
			&src[index], &dst[index], aF, bF, cF);
}

//------------------------------------------------------------------------------
// GaussianBlur3x3FilterPlugin_FilterY
//------------------------------------------------------------------------------

void GaussianBlur3x3FilterPlugin_ApplyBlurFilterYR1(
		const uint filmWidth, const uint filmHeight,
		__global const float *src,
		__global float *dst,
		const float aF,
		const float bF,
		const float cF) {
	// Do left edge
	float3 a;
	float3 b = VLOAD3F(&src[0 * 3]);
	float3 c = VLOAD3F(&src[filmWidth * 3]);

	const float leftTotF = bF + cF;
	const float3 bLeftK = bF / leftTotF;
	const float3 cLeftK = cF / leftTotF;
	VSTORE3F(bLeftK  * b + cLeftK * c, &dst[0 * 3]);

    // Main loop
	const float totF = aF + bF + cF;
	const float3 aK = aF / totF;
	const float3 bK = bF / totF;
	const float3 cK = cF / totF;

    for (uint y = 1; y < filmHeight - 1; ++y) {
		const unsigned index = y * filmWidth;

		a = b;
		b = c;
		c = VLOAD3F(&src[(index + filmWidth) * 3]);

		VSTORE3F(aK * a + bK  * b + cK * c, &dst[index * 3]);
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float3 aRightK = aF / rightTotF;
	const float3 bRightK = bF / rightTotF;
	a = b;
	b = c;
	VSTORE3F(aRightK  * a + bRightK * b, &dst[(filmHeight - 1) * filmWidth * 3]);
}

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void GaussianBlur3x3FilterPlugin_FilterY(
		const uint filmWidth, const uint filmHeight,
		__global const float *src,
		__global float *dst,
		const float weight) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth)
		return;

	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	const uint index = gid * 3;
	GaussianBlur3x3FilterPlugin_ApplyBlurFilterYR1(filmWidth, filmHeight,
			&src[index], &dst[index], aF, bF, cF);
}
