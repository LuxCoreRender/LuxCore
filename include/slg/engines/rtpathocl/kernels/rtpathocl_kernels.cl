#line 2 "rtpatchocl_kernels.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

//------------------------------------------------------------------------------
// ClearFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ClearFrameBuffer(
		const uint filmWidth, const uint filmHeight, 
		__global Pixel *frameBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	VSTORE4F(0.f, &frameBuffer[gid].c.r);
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

	VSTORE3F(0.f, &screenBuffer[gid].r);
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

	float4 rgbc = VLOAD4F(&frameBuffer[gid].c.r);

	if (rgbc.w > 0.f) {
		const float k = 1.f / rgbc.w;
		rgbc.s0 *= k;
		rgbc.s1 *= k;
		rgbc.s2 *= k;

		VSTORE4F(rgbc, &frameBuffer[gid].c.r);
	} else
		VSTORE4F(0.f, &frameBuffer[gid].c.r);
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

	float4 srcRGBC = VLOAD4F(&srcFrameBuffer[gid].c.r);

	if (srcRGBC.w > 0.f) {
		const float4 dstRGBC = VLOAD4F(&dstFrameBuffer[gid].c.r);
		VSTORE4F(srcRGBC + dstRGBC, &dstFrameBuffer[gid].c.r);
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
	float3 b = VLOAD3F(&src[0].r);
	float3 c = VLOAD3F(&src[1].r);

	const float leftTotF = bF + cF;
	const float3 bLeftK = bF / leftTotF;
	const float3 cLeftK = cF / leftTotF;
	VSTORE3F(bLeftK  * b + cLeftK * c, &dst[0].r);

    // Main loop
	const float totF = aF + bF + cF;
	const float3 aK = aF / totF;
	const float3 bK = bF / totF;
	const float3 cK = cF / totF;

	for (unsigned int x = 1; x < filmWidth - 1; ++x) {
		a = b;
		b = c;
		c = VLOAD3F(&src[x + 1].r);

		VSTORE3F(aK * a + bK  * b + cK * c, &dst[x].r);
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float3 aRightK = aF / rightTotF;
	const float3 bRightK = bF / rightTotF;
	a = b;
	b = c;
	VSTORE3F(aRightK  * a + bRightK * b, &dst[filmWidth - 1].r);
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
	float3 b = VLOAD3F(&src[0].r);
	float3 c = VLOAD3F(&src[filmWidth].r);

	const float leftTotF = bF + cF;
	const float3 bLeftK = bF / leftTotF;
	const float3 cLeftK = cF / leftTotF;
	VSTORE3F(bLeftK  * b + cLeftK * c, &dst[0].r);

    // Main loop
	const float totF = aF + bF + cF;
	const float3 aK = aF / totF;
	const float3 bK = bF / totF;
	const float3 cK = cF / totF;

    for (unsigned int y = 1; y < filmHeight - 1; ++y) {
		const unsigned index = y * filmWidth;

		a = b;
		b = c;
		c = VLOAD3F(&src[index + filmWidth].r);

		VSTORE3F(aK * a + bK  * b + cK * c, &dst[index].r);
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float3 aRightK = aF / rightTotF;
	const float3 bRightK = bF / rightTotF;
	a = b;
	b = c;
	VSTORE3F(aRightK  * a + bRightK * b, &dst[(filmHeight - 1) * filmWidth].r);
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

	const float aF = .15f;
	const float bF = 1.f;
	const float cF = .15f;

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
		__global Pixel *src,
		__global Pixel *dst) {
	const int gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	const float4 k = (float4)(PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, 1.f);
	const float4 sp = VLOAD4F(&src[gid].c.r);

	VSTORE4F(k * sp, &dst[gid].c.r);
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

	float4 newRgbc = VLOAD4F(&srcFrameBuffer[gid].c.r);

	if (newRgbc.s3 > 0.f) {
		float3 newRgb = (float3)(
				newRgb.s0 = Radiance2PixelFloat(newRgbc.s0),
				newRgb.s1 = Radiance2PixelFloat(newRgbc.s1),
				newRgb.s2 = Radiance2PixelFloat(newRgbc.s2));

		const float3 oldRgb = VLOAD3F(&screenBuffer[(x + y * filmWidth)].r);

		// Blend old and new RGB value in for ghost effect
		VSTORE3F(mix(oldRgb, newRgb, .85f), &screenBuffer[(x + y * filmWidth)].r);
	}
}
