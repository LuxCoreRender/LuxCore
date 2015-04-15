#line 2 "rtpatchocl_kernels.cl"

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

// List of symbols defined at compile time:
//  PARAM_GHOSTEFFECT_INTENSITY

//------------------------------------------------------------------------------
// ClearFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ClearFrameBuffer(
		const uint filmWidth, const uint filmHeight, 
		__global Pixel *frameBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	VSTORE4F(0.f, frameBuffer[gid].c.c);
}

//------------------------------------------------------------------------------
// ClearScreenBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ClearScreenBuffer(
		const uint filmWidth, const uint filmHeight,
		__global Spectrum *screenBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	VSTORE3F(0.f, screenBuffer[gid].c);
}

//------------------------------------------------------------------------------
// NormalizeFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void NormalizeFrameBuffer(
		const uint filmWidth, const uint filmHeight,
		__global Pixel *frameBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	float4 rgbc = VLOAD4F(frameBuffer[gid].c.c);

	if (rgbc.w > 0.f) {
		const float k = 1.f / rgbc.w;
		rgbc.s0 *= k;
		rgbc.s1 *= k;
		rgbc.s2 *= k;

		VSTORE4F(rgbc, frameBuffer[gid].c.c);
	} else
		VSTORE4F(0.f, frameBuffer[gid].c.c);
}

//------------------------------------------------------------------------------
// MergeFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergeFrameBuffer(
		const uint filmWidth, const uint filmHeight,
		__global Pixel *srcFrameBuffer,
		__global Pixel *dstFrameBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	float4 srcRGBC = VLOAD4F(srcFrameBuffer[gid].c.c);

	if (srcRGBC.w > 0.f) {
		const float4 dstRGBC = VLOAD4F(dstFrameBuffer[gid].c.c);
		VSTORE4F(srcRGBC + dstRGBC, dstFrameBuffer[gid].c.c);
	}
}

//------------------------------------------------------------------------------
// Image filtering kernels
//------------------------------------------------------------------------------

void ApplyBlurFilterXR1(
		const uint filmWidth, const uint filmHeight,
		__global Spectrum *src,
		__global Spectrum *dst,
		const float aF,
		const float bF,
		const float cF
		) {
	// Do left edge
	float3 a;
	float3 b = VLOAD3F(src[0].c);
	float3 c = VLOAD3F(src[1].c);

	const float leftTotF = bF + cF;
	const float3 bLeftK = bF / leftTotF;
	const float3 cLeftK = cF / leftTotF;
	VSTORE3F(bLeftK  * b + cLeftK * c, dst[0].c);

    // Main loop
	const float totF = aF + bF + cF;
	const float3 aK = aF / totF;
	const float3 bK = bF / totF;
	const float3 cK = cF / totF;

	for (unsigned int x = 1; x < filmWidth - 1; ++x) {
		a = b;
		b = c;
		c = VLOAD3F(src[x + 1].c);

		VSTORE3F(aK * a + bK  * b + cK * c, dst[x].c);
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float3 aRightK = aF / rightTotF;
	const float3 bRightK = bF / rightTotF;
	a = b;
	b = c;
	VSTORE3F(aRightK  * a + bRightK * b, dst[filmWidth - 1].c);
}

void ApplyBlurFilterYR1(
		const uint filmWidth, const uint filmHeight,
		__global Spectrum *src,
		__global Spectrum *dst,
		const float aF,
		const float bF,
		const float cF
		) {
	// Do left edge
	float3 a;
	float3 b = VLOAD3F(src[0].c);
	float3 c = VLOAD3F(src[filmWidth].c);

	const float leftTotF = bF + cF;
	const float3 bLeftK = bF / leftTotF;
	const float3 cLeftK = cF / leftTotF;
	VSTORE3F(bLeftK  * b + cLeftK * c, dst[0].c);

    // Main loop
	const float totF = aF + bF + cF;
	const float3 aK = aF / totF;
	const float3 bK = bF / totF;
	const float3 cK = cF / totF;

    for (unsigned int y = 1; y < filmHeight - 1; ++y) {
		const unsigned index = y * filmWidth;

		a = b;
		b = c;
		c = VLOAD3F(src[index + filmWidth].c);

		VSTORE3F(aK * a + bK  * b + cK * c, dst[index].c);
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float3 aRightK = aF / rightTotF;
	const float3 bRightK = bF / rightTotF;
	a = b;
	b = c;
	VSTORE3F(aRightK  * a + bRightK * b, dst[(filmHeight - 1) * filmWidth].c);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyGaussianBlurFilterXR1(
		const uint filmWidth, const uint filmHeight,
		__global Spectrum *src,
		__global Spectrum *dst,
		const float weight
		) {
	const size_t gid = get_global_id(0);
	if (gid >= filmHeight)
		return;

	src += gid * filmWidth;
	dst += gid * filmWidth;

	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	ApplyBlurFilterXR1(filmWidth, filmHeight, src, dst, aF, bF, cF);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyGaussianBlurFilterYR1(
		const uint filmWidth, const uint filmHeight,
		__global Spectrum *src,
		__global Spectrum *dst,
		const float weight
		) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth)
		return;

	src += gid;
	dst += gid;

	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	ApplyBlurFilterYR1(filmWidth, filmHeight, src, dst, aF, bF, cF);
}

//------------------------------------------------------------------------------
// Linear Tone Map Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ToneMapLinear(
		const uint filmWidth, const uint filmHeight,
		__global Pixel *pixels) {
	const int gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const float4 k = (float4)(PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, 1.f);
	const float4 sp = VLOAD4F(pixels[gid].c.c);

	VSTORE4F(k * sp, pixels[gid].c.c);
}

//------------------------------------------------------------------------------
// Compute the sum of all frame buffer RGB values
//------------------------------------------------------------------------------

__attribute__((reqd_work_group_size(64, 1, 1))) __kernel void SumRGBValuesReduce(
		const uint filmWidth, const uint filmHeight,
		__global float4 *src, __global float4 *dst) {
	// Workgroup local shared memory
	__local float4 localMemBuffer[64];

	const uint tid = get_local_id(0);
	const uint gid = get_global_id(0);

	const uint localSize = get_local_size(0);
	const uint stride = gid * 2;
	const uint pixelCount = filmWidth * filmHeight;
	localMemBuffer[tid] = 0.f;
	if (stride < pixelCount)
		localMemBuffer[tid] += src[stride];
	if (stride + 1 < pixelCount)
		localMemBuffer[tid] += src[stride + 1];	

	barrier(CLK_LOCAL_MEM_FENCE);

	// Do reduction in local memory
	for (uint s = localSize >> 1; s > 0; s >>= 1) {
		if (tid < s)
			localMemBuffer[tid] += localMemBuffer[tid + s];

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	// Write result for this block to global memory
	if (tid == 0) {
		const uint bid = get_group_id(0);

		dst[bid] = localMemBuffer[0];
	}
}

__attribute__((reqd_work_group_size(64, 1, 1))) __kernel void SumRGBValueAccumulate(
		const uint size, __global float4 *buff) {
	if (get_local_id(0) == 0) {
		float4 totalRGBValue = 0.f;
		for(uint i = 0; i < size; ++i)
			totalRGBValue += buff[i];
		buff[0] = totalRGBValue;
	}
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ToneMapAutoLinear(
		const uint filmWidth, const uint filmHeight,
		__global Pixel *pixels, const float gamma, __global float4 *totalRGB) {
	const int gid = get_global_id(0);
	const uint pixelCount = filmWidth * filmHeight;
	if (gid >= pixelCount)
		return;

	const float totalLuminance = .212671f * (*totalRGB).x + .715160f * (*totalRGB).y + .072169f * (*totalRGB).z;
	const float avgLuminance = totalLuminance / pixelCount;
	const float scale = (avgLuminance > 0.f) ? (1.25f / avgLuminance * pow(118.f / 255.f, gamma)) : 1.f;

	const float4 sp = VLOAD4F(pixels[gid].c.c);

	VSTORE4F(scale * sp, pixels[gid].c.c);
}

//------------------------------------------------------------------------------
// UpdateScreenBuffer Kernel
//------------------------------------------------------------------------------

float Radiance2PixelFloat(const float x) {
	return pow(clamp(x, 0.f, 1.f), 1.f / PARAM_GAMMA);
}

__kernel void UpdateScreenBuffer(
		const uint filmWidth, const uint filmHeight,
		__global Pixel *srcFrameBuffer,
		__global Spectrum *screenBuffer) {
	const int gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const int x = gid % filmWidth;
	const int y = gid / filmWidth;

	float4 newRgbc = VLOAD4F(srcFrameBuffer[gid].c.c);

	if (newRgbc.s3 > 0.f) {
		float3 newRgb = (float3)(
				newRgb.s0 = Radiance2PixelFloat(newRgbc.s0),
				newRgb.s1 = Radiance2PixelFloat(newRgbc.s1),
				newRgb.s2 = Radiance2PixelFloat(newRgbc.s2));

		const float3 oldRgb = VLOAD3F(screenBuffer[(x + y * filmWidth)].c);

		// Blend old and new RGB value in for ghost effect
		VSTORE3F(mix(oldRgb, newRgb, PARAM_GHOSTEFFECT_INTENSITY), screenBuffer[(x + y * filmWidth)].c);
	}
}
