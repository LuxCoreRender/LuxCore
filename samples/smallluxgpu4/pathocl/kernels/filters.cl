#line 2 "filters.cl"

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

//------------------------------------------------------------------------------
// Pixel related functions
//------------------------------------------------------------------------------

void PixelIndex2XY(const uint index, uint *x, uint *y) {
	*y = index / PARAM_IMAGE_WIDTH;
	*x = index - (*y) * PARAM_IMAGE_WIDTH;
}

uint XY2PixelIndex(const uint x, const uint y) {
	return x + y * PARAM_IMAGE_WIDTH;
}

uint XY2FrameBufferIndex(const int x, const int y) {
	return x + 1 + (y + 1) * (PARAM_IMAGE_WIDTH + 2);
}

bool IsValidPixelXY(const int x, const int y) {
	return (x >= 0) && (x < PARAM_IMAGE_WIDTH) && (y >= 0) && (y < PARAM_IMAGE_HEIGHT);
}

//------------------------------------------------------------------------------
// Image filtering related functions
//------------------------------------------------------------------------------

#if (PARAM_IMAGE_FILTER_TYPE == 0)

// Nothing

#elif (PARAM_IMAGE_FILTER_TYPE == 1)

// Box Filter
float ImageFilter_Evaluate(const float x, const float y) {
	return 1.f;
}

#elif (PARAM_IMAGE_FILTER_TYPE == 2)

float Gaussian(const float d, const float expv) {
	return max(0.f, exp(-PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA * d * d) - expv);
}

// Gaussian Filter
float ImageFilter_Evaluate(const float x, const float y) {
	return Gaussian(x,
			exp(-PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA * PARAM_IMAGE_FILTER_WIDTH_X * PARAM_IMAGE_FILTER_WIDTH_X)) *
		Gaussian(y, 
			exp(-PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA * PARAM_IMAGE_FILTER_WIDTH_Y * PARAM_IMAGE_FILTER_WIDTH_Y));
}

#elif (PARAM_IMAGE_FILTER_TYPE == 3)

float Mitchell1D(float x) {
	const float B = PARAM_IMAGE_FILTER_MITCHELL_B;
	const float C = PARAM_IMAGE_FILTER_MITCHELL_C;

	if (x >= 1.f)
		return 0.f;
	x = fabs(2.f * x);

	if (x > 1.f)
		return (((-B / 6.f - C) * x + (B + 5.f * C)) * x +
			(-2.f * B - 8.f * C)) * x + (4.f / 3.f * B + 4.f * C);
	else
		return ((2.f - 1.5f * B - C) * x +
			(-3.f + 2.f * B + C)) * x * x +
			(1.f - B / 3.f);
}

// Mitchell Filter
float ImageFilter_Evaluate(const float x, const float y) {
	const float distance = native_sqrt(
			x * x * (1.f / (PARAM_IMAGE_FILTER_WIDTH_X * PARAM_IMAGE_FILTER_WIDTH_X)) +
			y * y * (1.f / (PARAM_IMAGE_FILTER_WIDTH_Y * PARAM_IMAGE_FILTER_WIDTH_Y)));

	return Mitchell1D(distance);
}

#else

Error: unknown image filter !!!

#endif

#if defined(PARAM_USE_PIXEL_ATOMICS)
void AtomicAdd(__global float *val, const float delta) {
	union {
		float f;
		unsigned int i;
	} oldVal;
	union {
		float f;
		unsigned int i;
	} newVal;

	do {
		oldVal.f = *val;
		newVal.f = oldVal.f + delta;
	} while (atom_cmpxchg((__global unsigned int *)val, oldVal.i, newVal.i) != oldVal.i);
}
#endif

void Pixel_AddRadiance(__global Pixel *pixel, const float3 rad, const float weight) {
	/*if (isnan(rad->r) || isinf(rad->r) ||
			isnan(rad->g) || isinf(rad->g) ||
			isnan(rad->b) || isinf(rad->b) ||
			isnan(weight) || isinf(weight))
		printf(\"NaN/Inf. error: (%f, %f, %f) [%f]\\n\", rad->r, rad->g, rad->b, weight);*/

	float4 s;
	s.xyz = rad;
	s.w = 1.f;
	s *= weight;

#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&pixel->c.r, s.x);
	AtomicAdd(&pixel->c.g, s.y);
	AtomicAdd(&pixel->c.b, s.z);
	AtomicAdd(&pixel->count, s.w);
#else
	float4 p = VLOAD4F(&(pixel->c.r));
	p += s;
	VSTORE4F(p, &(pixel->c.r));
#endif
}

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
void Pixel_AddAlpha(__global AlphaPixel *apixel, const float alpha, const float weight) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&apixel->alpha, weight * alpha);
#else
	apixel->alpha += weight * alpha;
#endif
}
#endif

#if (PARAM_IMAGE_FILTER_TYPE == 1) || (PARAM_IMAGE_FILTER_TYPE == 2) || (PARAM_IMAGE_FILTER_TYPE == 3)
void Pixel_AddFilteredRadiance(__global Pixel *pixel, const float3 rad,
	const float distX, const float distY, const float weight) {
	const float filterWeight = ImageFilter_Evaluate(distX, distY);

	Pixel_AddRadiance(pixel, rad, weight * filterWeight);
}

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
void Pixel_AddFilteredAlpha(__global AlphaPixel *apixel, const float alpha,
	const float distX, const float distY, const float weight) {
	const float filterWeight = ImageFilter_Evaluate(distX, distY);

	Pixel_AddAlpha(apixel, alpha, weight * filterWeight);
}
#endif

#endif

#if (PARAM_IMAGE_FILTER_TYPE == 0)

void SplatSample(__global Pixel *frameBuffer,
		const float scrX, const float scrY, const float3 radiance,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		__global AlphaPixel *alphaFrameBuffer,
		const float alpha,
#endif
		const float weight) {
	const uint x = min((uint)floor(PARAM_IMAGE_WIDTH * scrX + .5f), (uint)(PARAM_IMAGE_WIDTH - 1));
	const uint y = min((uint)floor(PARAM_IMAGE_HEIGHT * scrY + .5f), (uint)(PARAM_IMAGE_HEIGHT - 1));
	
	__global Pixel *pixel = &frameBuffer[XY2FrameBufferIndex(x, y)];
	Pixel_AddRadiance(pixel, radiance, weight);

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	__global AlphaPixel *apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x, y)];
	Pixel_AddAlpha(apixel, alpha, weight);
#endif
}

#elif (PARAM_IMAGE_FILTER_TYPE == 1) || (PARAM_IMAGE_FILTER_TYPE == 2) || (PARAM_IMAGE_FILTER_TYPE == 3)

void SplatSample(__global Pixel *frameBuffer,
		const float scrX, const float scrY, const float3 radiance,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		__global AlphaPixel *alphaFrameBuffer,
		const float alpha,
#endif
		const float weight) {
	const float px = PARAM_IMAGE_WIDTH * scrX + .5f;
	const float py = PARAM_IMAGE_HEIGHT * scrY + .5f;

	const uint x = min((uint)floor(px), (uint)(PARAM_IMAGE_WIDTH - 1));
	const uint y = min((uint)floor(py), (uint)(PARAM_IMAGE_HEIGHT - 1));

	const float sx = px - (float)x;
	const float sy = py - (float)y;

	{
		__global Pixel *pixel = &frameBuffer[XY2FrameBufferIndex(x - 1, y - 1)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx + 1.f, sy + 1.f, weight);
		pixel = &frameBuffer[XY2FrameBufferIndex(x, y - 1)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx, sy + 1.f, weight);
		pixel = &frameBuffer[XY2FrameBufferIndex(x + 1, y - 1)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx - 1.f, sy + 1.f, weight);

		pixel = &frameBuffer[XY2FrameBufferIndex(x - 1, y)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx + 1.f, sy, weight);
		pixel = &frameBuffer[XY2FrameBufferIndex(x, y)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx, sy, weight);
		pixel = &frameBuffer[XY2FrameBufferIndex(x + 1, y)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx - 1.f, sy, weight);

		pixel = &frameBuffer[XY2FrameBufferIndex(x - 1, y + 1)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx + 1.f, sy - 1.f, weight);
		pixel = &frameBuffer[XY2FrameBufferIndex(x, y + 1)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx, sy - 1.f, weight);
		pixel = &frameBuffer[XY2FrameBufferIndex(x + 1, y + 1)];
		Pixel_AddFilteredRadiance(pixel, radiance, sx - 1.f, sy - 1.f, weight);
	}

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	{
		__global AlphaPixel *apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x - 1, y - 1)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx + 1.f, sy + 1.f, weight);
		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x, y - 1)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx, sy + 1.f, weight);
		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x + 1, y - 1)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx - 1.f, sy + 1.f, weight);

		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x - 1, y)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx + 1.f, sy, weight);
		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x, y)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx, sy, weight);
		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x + 1, y)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx - 1.f, sy, weight);

		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x - 1, y + 1)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx + 1.f, sy - 1.f, weight);
		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x, y + 1)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx, sy - 1.f, weight);
		apixel = &alphaFrameBuffer[XY2FrameBufferIndex(x + 1, y + 1)];
		Pixel_AddFilteredAlpha(apixel, alpha, sx - 1.f, sy - 1.f, weight);
	}
#endif
}

#else

Error: unknown image filter !!!

#endif
