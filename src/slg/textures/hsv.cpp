/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "slg/textures/hsv.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Hue saturation value texture
//------------------------------------------------------------------------------

float HsvTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum HsvTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Spectrum colorHitpoint = tex->GetSpectrumValue(hitPoint);
	const float hueHitpoint = hue->GetFloatValue(hitPoint);
	const float satHitpoint = sat->GetFloatValue(hitPoint);
	const float valHitpoint = val->GetFloatValue(hitPoint);
	
	return ApplyTransformation(colorHitpoint, hueHitpoint, satHitpoint, valHitpoint);
}

Spectrum HsvTexture::ApplyTransformation(const Spectrum &colorHitpoint,
		const float &hueHitpoint, const float &satHitpoint,
		const float &valHitpoint) const {

	const Spectrum input = colorHitpoint.Clamp(0.f, 1.f);
	
	// colorHitpoint to HSV
	Spectrum hsv = RgbToHsv(input);

	// Manipulate HSV
	hsv.c[0] += hueHitpoint + .5f;
	hsv.c[0] = fmodf(hsv.c[0], 1.f);
	hsv.c[1] *= satHitpoint;
	hsv.c[2] *= valHitpoint;
	
	// Clamp color to prevent negative values caused by over-saturation
	return HsvToRgb(hsv).Clamp(0.f, 1.f);
}

float HsvTexture::Y() const {
	return ApplyTransformation(Spectrum(tex->Filter()),
			hue->Filter(), sat->Filter(), val->Filter()).Y();
}

float HsvTexture::Filter() const {
	return ApplyTransformation(Spectrum(tex->Filter()),
			hue->Filter(), sat->Filter(), val->Filter()).Filter();	
}

Properties HsvTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hsv"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetName()));
	props.Set(Property("scene.textures." + name + ".hue")(hue->GetName()));
	props.Set(Property("scene.textures." + name + ".saturation")(sat->GetName()));
	props.Set(Property("scene.textures." + name + ".value")(val->GetName()));

	return props;
}

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

Spectrum HsvTexture::RgbToHsv(const Spectrum &rgb) const {
	float cmax, cmin, h, s, v, cdelta;

	cmax = Max(rgb.c[0], Max(rgb.c[1], rgb.c[2]));
	cmin = Min(rgb.c[0], Min(rgb.c[1], rgb.c[2]));
	cdelta = cmax - cmin;

	v = cmax;

	if (cmax != 0.f)
		s = cdelta / cmax;
	else {
		s = 0.f;
		h = 0.f;
	}

	if (s != 0.0f) {
		Spectrum c;
		float icdelta = 1.f / cdelta;
		c.c[0] = (cmax - rgb.c[0]) * icdelta;
		c.c[1] = (cmax - rgb.c[1]) * icdelta;
		c.c[2] = (cmax - rgb.c[2]) * icdelta;

		if (rgb.c[0] == cmax)
			h = c.c[2] - c.c[1];
		else if (rgb.c[1] == cmax)
			h = 2.f + c.c[0] - c.c[2];
		else
			h = 4.f + c.c[1] - c.c[0];

		h /= 6.f;

		if (h < 0.f)
			h += 1.f;
	} else
		h = 0.f;

	return Spectrum(h, s, v);
}

Spectrum HsvTexture::HsvToRgb(const Spectrum &hsv) const {
	float i, f, p, q, t, h, s, v;

	h = hsv.c[0];
	s = hsv.c[1];
	v = hsv.c[2];

	if (s != 0.f) {
		if (h == 1.f)
			h = 0.f;

		h *= 6.f;
		i = Floor2Int(h);
		f = h - i;

		p = v * (1.f - s);
		q = v * (1.f - (s * f));
		t = v * (1.f - (s * (1.f - f)));

		if (i == 0.f) return Spectrum(v, t, p);
		else if (i == 1.f) return Spectrum(q, v, p);
		else if (i == 2.f) return Spectrum(p, v, t);
		else if (i == 3.f) return Spectrum(p, q, v);
		else if (i == 4.f) return Spectrum(t, p, v);
		else return Spectrum(v, p, q);
	} else
		return Spectrum(v, v, v);
}
