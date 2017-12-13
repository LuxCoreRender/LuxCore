#line 2 "filter_funcs.cl"

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
// FilterDistribution
//------------------------------------------------------------------------------

void FilterDistribution_SampleContinuous(__global float *pixelFilterDistribution,
		const float u0, const float u1, float *su0, float *su1) {
	// Sample according the pixel filter distribution
	float2 uv;
	float distPdf;
	Distribution2D_SampleContinuous(pixelFilterDistribution, u0, u1, &uv, &distPdf);

	*su0 = (uv.x - .5f) * (2.f * PARAM_IMAGE_FILTER_WIDTH_X);
	*su1 = (uv.y - .5f) * (2.f * PARAM_IMAGE_FILTER_WIDTH_Y);
}

//------------------------------------------------------------------------------
// Pixel related functions
//------------------------------------------------------------------------------

void PixelIndex2XY(const uint filmWidth, const uint index, uint *x, uint *y) {
	*y = index / filmWidth;
	*x = index - (*y) * filmWidth;
}

uint XY2PixelIndex(const uint filmWidth, const uint x, const uint y) {
	return x + y * filmWidth;
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

// Mitchell Filter
float ImageFilter_Evaluate(const float x, const float y) {
	const float distance = native_sqrt(
			x * x * (1.f / (PARAM_IMAGE_FILTER_WIDTH_X * PARAM_IMAGE_FILTER_WIDTH_X)) +
			y * y * (1.f / (PARAM_IMAGE_FILTER_WIDTH_Y * PARAM_IMAGE_FILTER_WIDTH_Y)));

	const float dist = distance / .6f;
	return PARAM_IMAGE_FILTER_MITCHELL_A1 * Mitchell1D(dist - 2.f / 3.f) +
		PARAM_IMAGE_FILTER_MITCHELL_A0 * Mitchell1D(dist) +
		PARAM_IMAGE_FILTER_MITCHELL_A1 * Mitchell1D(dist + 2.f / 3.f);
}

#elif (PARAM_IMAGE_FILTER_TYPE == 5)

float BlackmanHarris1D(float x) {
	if (x < -1.f || x > 1.f)
		return 0.f;
	x = (x + 1.f) * .5f;
	x *= M_PI_F;
	const float A0 =  0.35875f;
	const float A1 = -0.48829f;
	const float A2 =  0.14128f;
	const float A3 = -0.01168f;
	return A0 + A1 * cos(2.f * x) + A2 * cos(4.f * x) + A3 * cos(6.f * x);
}

// Blackman-Harris Filter
float ImageFilter_Evaluate(const float x, const float y) {
	return BlackmanHarris1D(x * (1.f / PARAM_IMAGE_FILTER_WIDTH_X)) *
			BlackmanHarris1D(y *  (1.f / PARAM_IMAGE_FILTER_WIDTH_Y));
}

#else

Error: unknown image filter !!!

#endif
