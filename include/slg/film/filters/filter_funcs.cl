#line 2 "filter_funcs.cl"

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
// FilterDistribution
//------------------------------------------------------------------------------

void FilterDistribution_SampleContinuous(__constant const Filter *pixelFitler,
		__global float *pixelFilterDistribution,
		const float u0, const float u1, float *su0, float *su1) {
	// Sample according the pixel filter distribution
	float2 uv;
	float distPdf;
	Distribution2D_SampleContinuous(pixelFilterDistribution, u0, u1, &uv, &distPdf);

	*su0 = (uv.x - .5f) * (2.f * pixelFitler->widthX);
	*su1 = (uv.y - .5f) * (2.f * pixelFitler->widthY);
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
