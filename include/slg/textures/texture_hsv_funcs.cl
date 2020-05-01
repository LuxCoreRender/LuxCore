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

	cmax = fmax(rgb.x, fmax(rgb.y, rgb.z));
	cmin = fmin(rgb.x, fmin(rgb.y, rgb.z));
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
		c.x = (cmax - rgb.x) * icdelta;
		c.y = (cmax - rgb.y) * icdelta;
		c.z = (cmax - rgb.z) * icdelta;

		if (rgb.x == cmax)
			h = c.z - c.y;
		else if (rgb.y == cmax)
			h = 2.f + c.x - c.z;
		else
			h = 4.f + c.y - c.x;

		h /= 6.f;

		if (h < 0.f)
			h += 1.f;
	} else
		h = 0.f;

	return MAKE_FLOAT3(h, s, v);
}

OPENCL_FORCE_INLINE float3 HsvTexture_HsvToRgb(const float3 hsv) {
	float i, f, p, q, t, h, s, v;

	h = hsv.x;
	s = hsv.y;
	v = hsv.z;

	if (s != 0.f) {
		if (h == 1.f)
			h = 0.f;

		h *= 6.f;
		i = Floor2Int(h);
		f = h - i;

		p = v * (1.f - s);
		q = v * (1.f - (s * f));
		t = v * (1.f - (s * (1.f - f)));

		if (i == 0.f) return MAKE_FLOAT3(v, t, p);
		else if (i == 1.f) return MAKE_FLOAT3(q, v, p);
		else if (i == 2.f) return MAKE_FLOAT3(p, v, t);
		else if (i == 3.f) return MAKE_FLOAT3(p, q, v);
		else if (i == 4.f) return MAKE_FLOAT3(t, p, v);
		else return MAKE_FLOAT3(v, p, q);
	} else
		return TO_FLOAT3(v);
}

//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 HsvTexture_ApplyTransformation(const float3 colorHitpoint,
		const float hueHitpoint, const float satHitpoint,
		const float valHitpoint) {

	const float3 input = Spectrum_Clamp(colorHitpoint);
	
	// colorHitpoint to HSV
	float3 hsv = HsvTexture_RgbToHsv(input);

	// Manipulate HSV
	hsv.x += hueHitpoint + .5f;
	hsv.x = fmod(hsv.x, 1.f);
	hsv.y *= satHitpoint;
	hsv.z *= valHitpoint;

	// Clamp color to prevent negative values caused by over-saturation
	return Spectrum_Clamp(HsvTexture_HsvToRgb(hsv));
}

OPENCL_FORCE_INLINE float HsvTexture_ConstEvaluateFloat(const float3 colorHitpoint,
		const float hueHitpoint, const float satHitpoint,
		const float valHitpoint) {
	return Spectrum_Y(HsvTexture_ApplyTransformation(colorHitpoint, hueHitpoint, satHitpoint, valHitpoint));
}

OPENCL_FORCE_INLINE float3 HsvTexture_ConstEvaluateSpectrum(const float3 colorHitpoint,
		const float hueHitpoint, const float satHitpoint,
		const float valHitpoint) {
	return HsvTexture_ApplyTransformation(colorHitpoint, hueHitpoint, satHitpoint, valHitpoint);
}
