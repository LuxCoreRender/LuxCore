#line 2 "texture_hsv_funcs.cl"

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
// Hsv texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HSV)

/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

OPENCL_FORCE_INLINE float3 HsvTexture_RgbToHsv(const float3 rgb) {
	float cmax, cmin, h, s, v, cdelta;

	cmax = fmax(rgb.s0, fmax(rgb.s1, rgb.s2));
	cmin = fmin(rgb.s0, fmin(rgb.s1, rgb.s2));
	cdelta = cmax - cmin;

	v = cmax;

	if (cmax != 0.f)
		s = cdelta / cmax;
	else {
		s = 0.f;
		h = 0.f;
	}

	if (s != 0.0f) {
		float3 c;
		float icdelta = 1.f / cdelta;
		c.s0 = (cmax - rgb.s0) * icdelta;
		c.s1 = (cmax - rgb.s1) * icdelta;
		c.s2 = (cmax - rgb.s2) * icdelta;

		if (rgb.s0 == cmax)
			h = c.s2 - c.s1;
		else if (rgb.s1 == cmax)
			h = 2.f + c.s0 - c.s2;
		else
			h = 4.f + c.s1 - c.s0;

		h /= 6.f;

		if (h < 0.f)
			h += 1.f;
	} else
		h = 0.f;

	return (float3)(h, s, v);
}

OPENCL_FORCE_INLINE float3 HsvTexture_HsvToRgb(const float3 hsv) {
	float i, f, p, q, t, h, s, v;

	h = hsv.s0;
	s = hsv.s1;
	v = hsv.s2;

	if (s != 0.f) {
		if (h == 1.f)
			h = 0.f;

		h *= 6.f;
		i = Floor2Int(h);
		f = h - i;

		p = v * (1.f - s);
		q = v * (1.f - (s * f));
		t = v * (1.f - (s * (1.f - f)));

		if (i == 0.f) return (float3)(v, t, p);
		else if (i == 1.f) return (float3)(q, v, p);
		else if (i == 2.f) return (float3)(p, v, t);
		else if (i == 3.f) return (float3)(p, q, v);
		else if (i == 4.f) return (float3)(t, p, v);
		else return (float3)(v, p, q);
	} else
		return (float3)(v, v, v);
}

//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 HsvTexture_ApplyTransformation(const float3 colorHitpoint,
		const float hueHitpoint, const float satHitpoint,
		const float valHitpoint) {

	const float3 input = Spectrum_Clamp(colorHitpoint);
	
	// colorHitpoint to HSV
	float3 hsv = HsvTexture_RgbToHsv(input);

	// Manipulate HSV
	hsv.s0 += hueHitpoint + .5f;
	hsv.s0 = fmod(hsv.s0, 1.f);
	hsv.s1 *= satHitpoint;
	hsv.s2 *= valHitpoint;

	// Clamp color to prevent negative values caused by over-saturation
	return Spectrum_Clamp(HsvTexture_HsvToRgb(hsv));
}

OPENCL_FORCE_NOT_INLINE float HsvTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float3 colorHitpoint,
		const float hueHitpoint, const float satHitpoint,
		const float valHitpoint) {
	return Spectrum_Y(HsvTexture_ApplyTransformation(colorHitpoint, hueHitpoint, satHitpoint, valHitpoint));
}

OPENCL_FORCE_NOT_INLINE float3 HsvTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 colorHitpoint,
		const float hueHitpoint, const float satHitpoint,
		const float valHitpoint) {
	return HsvTexture_ApplyTransformation(colorHitpoint, hueHitpoint, satHitpoint, valHitpoint);
}

#endif
