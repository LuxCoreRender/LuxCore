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

#if (PARAM_SAMPLER_TYPE == 0) || (PARAM_SAMPLER_TYPE == 1)

uint PixelIndexInt(const size_t gid) {
	return gid % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
}

uint NextPixelIndex(const uint i) {
	return (i + PARAM_TASK_COUNT) % (PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT);
}

#elif (PARAM_SAMPLER_TYPE == 2)

uint PixelIndexFloat(const float u) {
	const uint pixelCountPerTask = PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT;
	const uint i = min((uint)floor(pixelCountPerTask * u), pixelCountPerTask - 1);

	return i;
}

#endif

void PixelIndex2XY(const uint index, uint *x, uint *y) {
	*y = index / PARAM_IMAGE_WIDTH;
	*x = index - (*y) * PARAM_IMAGE_WIDTH;
}

uint XY2PixelIndex(const uint x, const uint y) {
	return x + y * PARAM_IMAGE_WIDTH;
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
	return max(0.f, native_exp(-PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA * d * d) - expv);
}

// Gaussian Filter
float ImageFilter_Evaluate(const float x, const float y) {
	return Gaussian(x,
			native_exp(-PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA * PARAM_IMAGE_FILTER_WIDTH_X * PARAM_IMAGE_FILTER_WIDTH_X)) *
		Gaussian(y, 
			native_exp(-PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA * PARAM_IMAGE_FILTER_WIDTH_Y * PARAM_IMAGE_FILTER_WIDTH_Y));
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

#elif (PARAM_IMAGE_FILTER_TYPE == 4)

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

// Mitchell Filter with Super Sampling
float ImageFilter_Evaluate(const float x, const float y) {
	const float distance = native_sqrt(
			x * x * (1.f / (PARAM_IMAGE_FILTER_WIDTH_X * 5.f / 3.f * PARAM_IMAGE_FILTER_WIDTH_X * 5.f / 3.f)) +
			y * y * (1.f / (PARAM_IMAGE_FILTER_WIDTH_Y * 5.f / 3.f * PARAM_IMAGE_FILTER_WIDTH_Y * 5.f / 3.f)));

	const float dist = distance / .6f;
	const float B = PARAM_IMAGE_FILTER_MITCHELL_B;
	const float C = PARAM_IMAGE_FILTER_MITCHELL_C;
	const float a0 = (76.f - 16.f * B + 8.f * C) / 81.f;
	const float a1 = (1.f - a0)/ 2.f;

	return a1 * Mitchell1D(dist - 2.f / 3.f) +
			a0 * Mitchell1D(dist) +
			a1 * Mitchell1D(dist + 2.f / 3.f);
}

#else

Error: unknown image filter !!!

#endif

void Pixel_AddRadiance(__global Pixel *pixel, Spectrum *rad, const float weight) {
	/*if (isnan(rad->r) || isinf(rad->r) ||
			isnan(rad->g) || isinf(rad->g) ||
			isnan(rad->b) || isinf(rad->b) ||
			isnan(weight) || isinf(weight))
		printf(\"(NaN/Inf. error: (%f, %f, %f) [%f]\\n\", rad->r, rad->g, rad->b, weight);*/

	float4 s;
	s.x = rad->r;
	s.y = rad->g;
	s.z = rad->b;
	s.w = 1.f;
	s *= weight;

#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&pixel->c.r, s.x);
	AtomicAdd(&pixel->c.g, s.y);
	AtomicAdd(&pixel->c.b, s.z);
	AtomicAdd(&pixel->count, s.w);

#else
	float4 p = *((__global float4 *)pixel);
	p += s;
	*((__global float4 *)pixel) = p;
#endif
}

#if (PARAM_IMAGE_FILTER_TYPE == 1) || (PARAM_IMAGE_FILTER_TYPE == 2) || (PARAM_IMAGE_FILTER_TYPE == 3) || (PARAM_IMAGE_FILTER_TYPE == 4)
void Pixel_AddFilteredRadiance(__global Pixel *pixel, Spectrum *rad,
	const float distX, const float distY, const float weight) {
	const float filterWeight = ImageFilter_Evaluate(distX, distY);

	Pixel_AddRadiance(pixel, rad, weight * filterWeight)
}
#endif

#if (PARAM_IMAGE_FILTER_TYPE == 0)

void SplatSample(__global Pixel *frameBuffer, const uint pixelIndex, Spectrum *radiance, const float weight) {
		__global Pixel *pixel = &frameBuffer[pixelIndex];

		Pixel_AddRadiance(pixel, radiance, weight);
}

#elif (PARAM_IMAGE_FILTER_TYPE == 1) || (PARAM_IMAGE_FILTER_TYPE == 2) || (PARAM_IMAGE_FILTER_TYPE == 3) || (PARAM_IMAGE_FILTER_TYPE == 4)

void SplatSample(__global Pixel *frameBuffer, const uint pixelIndex, const float sx, const float sy, Spectrum *radiance, const float weight) {
		uint bufferPixelIndex = pixelIndex * 9;

		__global Pixel *pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx + 1.f, sy + 1.f, weight);
		pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx      , sy + 1.f, weight);
		pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx - 1.f, sy + 1.f, weight);

		pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx + 1.f, sy, weight);
		pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx      , sy, weight);
		pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx - 1.f, sy, weight);

		pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx + 1.f, sy - 1.f, weight);
		pixel = &frameBuffer[bufferPixelIndex++];
		Pixel_AddFilteredRadiance(pixel, radiance, sx      , sy - 1.f, weight);
		pixel = &frameBuffer[bufferPixelIndex];
		Pixel_AddFilteredRadiance(pixel, radiance, sx - 1.f, sy - 1.f, weight);
}

#else

Error: unknown image filter !!!

#endif
