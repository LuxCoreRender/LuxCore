#line 2 "rtpatchocl_kernels.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#define FRAMEBUFFER_WIDTH (PARAM_IMAGE_WIDTH + 2)
#define FRAMEBUFFER_HEIGHT (PARAM_IMAGE_HEIGHT + 2)

//------------------------------------------------------------------------------
// InitDisplayFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitDisplayFrameBuffer(
		__global Pixel *frameBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)
		return;

	VSTORE4F(0.f, &frameBuffer[gid].c.r);
}

//------------------------------------------------------------------------------
// MergeFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergeFrameBuffer(
		__global Pixel *srcFrameBuffer,
		__global Pixel *dstFrameBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)
		return;

	const float4 srcRGBC = VLOAD4F(&srcFrameBuffer[gid].c.r);
	const float4 dstRGBC = VLOAD4F(&dstFrameBuffer[gid].c.r);
	VSTORE4F(srcRGBC + dstRGBC, &dstFrameBuffer[gid].c.r);
}

//------------------------------------------------------------------------------
// Image filtering kernels
//------------------------------------------------------------------------------

void ApplyBlurFilterXR1(
		__global Pixel *src,
		__global Pixel *dst,
		const float aF,
		const float bF,
		const float cF
		) {
	// Do left edge
	Pixel a;
	Pixel b = src[0];
	Pixel c = src[1];
	float invACount, invBCount, invCCount;

	const float leftTotF = bF + cF;
	const float bLeftK = bF / leftTotF;
	const float cLeftK = cF / leftTotF;
	invBCount = 1.f / b.count;
	invCCount = 1.f / c.count;
	dst[0].c.r = bLeftK * b.c.r * invBCount + cLeftK * c.c.r * invCCount;
	dst[0].c.g = bLeftK * b.c.g * invBCount + cLeftK * c.c.g * invCCount;
	dst[0].c.b = bLeftK * b.c.b * invBCount + cLeftK * c.c.b * invCCount;

    // Main loop
	const float totF = aF + bF + cF;
	const float aK = aF / totF;
	const float bK = bF / totF;
	const float cK = cF / totF;

	for (unsigned int x = 1; x < FRAMEBUFFER_WIDTH - 1; ++x) {
		a = b;
		invACount = invBCount;
		b = c;
		invBCount = invCCount;
		c = src[x + 1];
		invCCount = 1.f / c.count;

		dst[x].c.r = aK * a.c.r * invACount + bK * b.c.r * invBCount + cK * c.c.r * invCCount;
		dst[x].c.g = aK * a.c.g * invACount + bK * b.c.g * invBCount + cK * c.c.g * invCCount;
		dst[x].c.b = aK * a.c.b * invACount + bK * b.c.b * invBCount + cK * c.c.b * invCCount;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float aRightK = aF / rightTotF;
	const float bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[FRAMEBUFFER_WIDTH - 1].c.r = aRightK * a.c.r + bRightK * b.c.r;
	dst[FRAMEBUFFER_WIDTH - 1].c.g = aRightK * a.c.g + bRightK * b.c.g;
	dst[FRAMEBUFFER_WIDTH - 1].c.b = aRightK * a.c.b + bRightK * b.c.b;

}

void ApplyBlurFilterYR1(
		__global Pixel *src,
		__global Pixel *dst,
		const float aF,
		const float bF,
		const float cF
		) {
	// Do left edge
	Pixel a;
	Pixel b = src[0];
	Pixel c = src[FRAMEBUFFER_WIDTH];
	float invACount, invBCount, invCCount;

	const float leftTotF = bF + cF;
	const float bLeftK = bF / leftTotF;
	const float cLeftK = cF / leftTotF;
	invBCount = 1.f / b.count;
	invCCount = 1.f / c.count;
	dst[0].c.r = bLeftK * b.c.r * invBCount + cLeftK * c.c.r * invCCount;
	dst[0].c.g = bLeftK * b.c.g * invBCount + cLeftK * c.c.g * invCCount;
	dst[0].c.b = bLeftK * b.c.b * invBCount + cLeftK * c.c.b * invCCount;

    // Main loop
	const float totF = aF + bF + cF;
	const float aK = aF / totF;
	const float bK = bF / totF;
	const float cK = cF / totF;

    for (unsigned int y = 1; y < FRAMEBUFFER_HEIGHT - 1; ++y) {
		const unsigned index = y * FRAMEBUFFER_WIDTH;

		a = b;
		invACount = invBCount;
		b = c;
		invBCount = invCCount;
		c = src[index + FRAMEBUFFER_WIDTH];
		invCCount = 1.f / c.count;

		dst[index].c.r = aK * a.c.r * invACount + bK * b.c.r * invBCount + cK * c.c.r * invCCount;
		dst[index].c.g = aK * a.c.g * invACount + bK * b.c.g * invBCount + cK * c.c.g * invCCount;
		dst[index].c.b = aK * a.c.b * invACount + bK * b.c.b * invBCount + cK * c.c.b * invCCount;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float aRightK = aF / rightTotF;
	const float bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[(FRAMEBUFFER_HEIGHT - 1) * FRAMEBUFFER_WIDTH].c.r = aRightK * a.c.r + bRightK * b.c.r;
	dst[(FRAMEBUFFER_HEIGHT - 1) * FRAMEBUFFER_WIDTH].c.g = aRightK * a.c.g + bRightK * b.c.g;
	dst[(FRAMEBUFFER_HEIGHT - 1) * FRAMEBUFFER_WIDTH].c.b = aRightK * a.c.b + bRightK * b.c.b;
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBlurLightFilterXR1(
		__global Pixel *src,
		__global Pixel *dst
		) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_HEIGHT)
		return;

	src += gid * FRAMEBUFFER_WIDTH;
	dst += gid * FRAMEBUFFER_WIDTH;

	const float aF = .15f;
	const float bF = 1.f;
	const float cF = .15f;

	ApplyBlurFilterXR1(src, dst, aF, bF, cF);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBlurLightFilterYR1(
		__global Pixel *src,
		__global Pixel *dst
		) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH)
		return;

	src += gid;
	dst += gid;

	const float aF = .15f;
	const float bF = 1.f;
	const float cF = .15f;

	ApplyBlurFilterYR1(src, dst, aF, bF, cF);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBlurHeavyFilterXR1(
		__global Pixel *src,
		__global Pixel *dst
		) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_HEIGHT)
		return;

	src += gid * FRAMEBUFFER_WIDTH;
	dst += gid * FRAMEBUFFER_WIDTH;

	const float aF = .35f;
	const float bF = 1.f;
	const float cF = .35f;

	ApplyBlurFilterXR1(src, dst, aF, bF, cF);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBlurHeavyFilterYR1(
		__global Pixel *src,
		__global Pixel *dst
		) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH)
		return;

	src += gid;
	dst += gid;

	const float aF = .35f;
	const float bF = 1.f;
	const float cF = .35f;

	ApplyBlurFilterYR1(src, dst, aF, bF, cF);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBoxFilterXR1(
		__global Pixel *src,
		__global Pixel *dst
		) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_HEIGHT)
		return;

	src += gid * FRAMEBUFFER_WIDTH;
	dst += gid * FRAMEBUFFER_WIDTH;

	const float aF = .35f;
	const float bF = 1.f;
	const float cF = .35f;

	ApplyBlurFilterXR1(src, dst, aF, bF, cF);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBoxFilterYR1(
		__global Pixel *src,
		__global Pixel *dst
		) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH)
		return;

	src += gid;
	dst += gid;

	const float aF = 1.f / 3.f;
	const float bF = 1.f / 3.f;
	const float cF = 1.f / 3.f;

	ApplyBlurFilterYR1(src, dst, aF, bF, cF);
}

//------------------------------------------------------------------------------
// Linear Tone Map Kernel
//------------------------------------------------------------------------------

__kernel void ToneMapLinear(
		__global Pixel *src,
		__global Pixel *dst) {
	const int gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)
		return;

	const float4 k = (float4)(PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, 1.f);
	const float4 sp = VLOAD4F(&src[gid].c.r);

	VSTORE4F(k * sp, &dst[gid].c.r);
}

//------------------------------------------------------------------------------
// UpdateScreenBuffer Kernel
//------------------------------------------------------------------------------

//uint Radiance2PixelUInt(const float x) {
//	return (uint)(pow(clamp(x, 0.f, 1.f), 1.f / PARAM_GAMMA) * 255.f + .5f);
//}

float Radiance2PixelFloat(const float x) {
	return pow(clamp(x, 0.f, 1.f), 1.f / PARAM_GAMMA);
}

__kernel void UpdateScreenBuffer(
		__global Pixel *frameBuffer,
		__global Spectrum *pbo) {
	const int gid = get_global_id(0);
	const int x = gid % FRAMEBUFFER_WIDTH - 2;
	const int y = gid / FRAMEBUFFER_WIDTH - 2;
	if ((x < 0) || (y < 0) || (x >= PARAM_IMAGE_WIDTH) || (y >= PARAM_IMAGE_HEIGHT))
		return;

	__global Pixel *sp = &frameBuffer[gid];

	Spectrum rgb;
	const float count = sp->count;
	if (count > 0.f) {
		const float invCount = 1.f / count;
		rgb.r = Radiance2PixelFloat(sp->c.r * invCount);
		rgb.g = Radiance2PixelFloat(sp->c.g * invCount);
		rgb.b = Radiance2PixelFloat(sp->c.b * invCount);
	} else {
		rgb.r = 0.f;
		rgb.g = 0.f;
		rgb.b = 0.f;
	}

	pbo[(x + y * PARAM_IMAGE_WIDTH)] = rgb;
}
