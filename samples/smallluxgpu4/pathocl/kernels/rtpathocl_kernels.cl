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
// InitMergedFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitMergedFrameBuffer(
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
		__global Spectrum *dstFrameBuffer) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)
		return;

	float4 srcRGBC = VLOAD4F(&srcFrameBuffer[gid].c.r);

	// Normalize
	float3 srcRGB;
	if (srcRGBC.w > 0.f) {
		const float k = 1.f / (srcRGBC.w * PARAM_DEVICE_COUNT);
		srcRGB = (float3)(srcRGBC.x * k, srcRGBC.y * k, srcRGBC.z * k);
	} else
		srcRGB = 0.f;

	const float3 dstRGB = VLOAD3F(&dstFrameBuffer[gid].r);
	VSTORE3F(srcRGB + dstRGB, &dstFrameBuffer[gid].r);
}

//------------------------------------------------------------------------------
// Image filtering kernels
//------------------------------------------------------------------------------

void ApplyBlurFilterXR1(
		__global Spectrum *src,
		__global Spectrum *dst,
		const float aF,
		const float bF,
		const float cF
		) {
	// Do left edge
	Spectrum a;
	Spectrum b = src[0];
	Spectrum c = src[1];

	const float leftTotF = bF + cF;
	const float bLeftK = bF / leftTotF;
	const float cLeftK = cF / leftTotF;
	dst[0].r = bLeftK  * b.r + cLeftK * c.r;
	dst[0].g = bLeftK  * b.g + cLeftK * c.g;
	dst[0].b = bLeftK  * b.b + cLeftK * c.b;

    // Main loop
	const float totF = aF + bF + cF;
	const float aK = aF / totF;
	const float bK = bF / totF;
	const float cK = cF / totF;

	for (unsigned int x = 1; x < FRAMEBUFFER_WIDTH - 1; ++x) {
		a = b;
		b = c;
		c = src[x + 1];

		dst[x].r = aK * a.r + bK * b.r + cK * c.r;
		dst[x].g = aK * a.g + bK * b.g + cK * c.g;
		dst[x].b = aK * a.b + bK * b.b + cK * c.b;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float aRightK = aF / rightTotF;
	const float bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[FRAMEBUFFER_WIDTH - 1].r = aRightK * a.r + bRightK * b.r;
	dst[FRAMEBUFFER_WIDTH - 1].g = aRightK * a.g + bRightK * b.g;
	dst[FRAMEBUFFER_WIDTH - 1].b = aRightK * a.b + bRightK * b.b;

}

void ApplyBlurFilterYR1(
		__global Spectrum *src,
		__global Spectrum *dst,
		const float aF,
		const float bF,
		const float cF
		) {
	// Do left edge
	Spectrum a;
	Spectrum b = src[0];
	Spectrum c = src[FRAMEBUFFER_WIDTH];

	const float leftTotF = bF + cF;
	const float bLeftK = bF / leftTotF;
	const float cLeftK = cF / leftTotF;
	dst[0].r = bLeftK  * b.r + cLeftK * c.r;
	dst[0].g = bLeftK  * b.g + cLeftK * c.g;
	dst[0].b = bLeftK  * b.b + cLeftK * c.b;

    // Main loop
	const float totF = aF + bF + cF;
	const float aK = aF / totF;
	const float bK = bF / totF;
	const float cK = cF / totF;

    for (unsigned int y = 1; y < FRAMEBUFFER_HEIGHT - 1; ++y) {
		const unsigned index = y * FRAMEBUFFER_WIDTH;

		a = b;
		b = c;
		c = src[index + FRAMEBUFFER_WIDTH];

		dst[index].r = aK * a.r + bK * b.r + cK * c.r;
		dst[index].g = aK * a.g + bK * b.g + cK * c.g;
		dst[index].b = aK * a.b + bK * b.b + cK * c.b;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const float aRightK = aF / rightTotF;
	const float bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[(FRAMEBUFFER_HEIGHT - 1) * FRAMEBUFFER_WIDTH].r = aRightK * a.r + bRightK * b.r;
	dst[(FRAMEBUFFER_HEIGHT - 1) * FRAMEBUFFER_WIDTH].g = aRightK * a.g + bRightK * b.g;
	dst[(FRAMEBUFFER_HEIGHT - 1) * FRAMEBUFFER_WIDTH].b = aRightK * a.b + bRightK * b.b;
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBlurLightFilterXR1(
		__global Spectrum *src,
		__global Spectrum *dst
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
		__global Spectrum *src,
		__global Spectrum *dst
		) {
	const size_t gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH)
		return;

	src += gid;
	dst += gid;

	const float aF = .1f;
	const float bF = 1.f;
	const float cF = .1f;

	ApplyBlurFilterYR1(src, dst, aF, bF, cF);
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ApplyBlurHeavyFilterXR1(
		__global Spectrum *src,
		__global Spectrum *dst
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
		__global Spectrum *src,
		__global Spectrum *dst
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

//------------------------------------------------------------------------------
// Linear Tone Map Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void ToneMapLinear(
		__global Spectrum *src,
		__global Spectrum *dst) {
	const int gid = get_global_id(0);
	if (gid >= FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)
		return;

	const float4 k = (float4)(PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, PARAM_TONEMAP_LINEAR_SCALE, 1.f);
	const float4 sp = VLOAD4F(&src[gid].r);

	VSTORE4F(k * sp, &dst[gid].r);
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
		__global Spectrum *frameBuffer,
		__global Spectrum *pbo) {
	const int gid = get_global_id(0);
	const int x = gid % FRAMEBUFFER_WIDTH - 2;
	const int y = gid / FRAMEBUFFER_WIDTH - 2;
	if ((x < 0) || (y < 0) || (x >= PARAM_IMAGE_WIDTH) || (y >= PARAM_IMAGE_HEIGHT))
		return;

	__global Spectrum *sp = &frameBuffer[gid];

	Spectrum rgb;
	rgb.r = Radiance2PixelFloat(sp->r);
	rgb.g = Radiance2PixelFloat(sp->g);
	rgb.b = Radiance2PixelFloat(sp->b);

	pbo[(x + y * PARAM_IMAGE_WIDTH)] = rgb;
}
