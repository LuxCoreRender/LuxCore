#line 2 "tonemap_reinhard02_funcs.cl"

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
// Reinhard02ToneMap_Apply
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Reinhard02ToneMap_Apply(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		const float gamma,
		const float preScale,
		const float postScale,
		const float burn,
		__global float *totalRGB) {
	const size_t gid = get_global_id(0);
	const uint pixelCount = filmWidth * filmHeight;
	if (gid >= pixelCount)
		return;

	// Check if the pixel has received any sample
	if (!isinf(channel_IMAGEPIPELINE[gid * 3])) {
		float Ywa = native_exp(totalRGB[0] / pixelCount);

		// Avoid division by zero
		if (Ywa == 0.f)
			Ywa = 1.f;

		const float alpha = .1f;
		const float invB2 = (burn > 0.f) ? 1.f / (burn * burn) : 1e5f;
		const float scale = alpha / Ywa;
		const float preS = scale / preScale;
		const float postS = scale * postScale;

		__global float *pixel = &channel_IMAGEPIPELINE[gid * 3];
		float3 pixelValue = VLOAD3F(&channel_IMAGEPIPELINE[gid * 3]);

		const float ys = Spectrum_Y(pixelValue) * preS;
		// Note: I don't need to convert to XYZ and back because I'm only
		// scaling the value.
		pixelValue *= postS * (1.f + ys * invB2) / (1.f + ys);

		VSTORE3F(pixelValue, pixel);
	}
}

//------------------------------------------------------------------------------
// REDUCE_OP & ACCUM_OP (used by tonemap_reduce_funcs.cl)
//------------------------------------------------------------------------------

float3 REDUCE_OP(const float3 a, const float3 b) {
	if (Spectrum_IsNanOrInf(b))
		return a;
	else {
		const float y = fmax(Spectrum_Y(b), 1e-6f);
		return a + native_log(y);
	}
}

float3 ACCUM_OP(const float3 a, const float3 b) {
	return a + b;
}
