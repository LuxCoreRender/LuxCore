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
	
	const Spectrum input = colorHitpoint.Clamp(0.f, 1.f);
	
	// colorHitpoint to HSV
	float color[3] = {0.f, 0.f, 0.f};
	RgbToHsv(input, color);
	
	// Manipulate HSV
	color[0] += hueHitpoint + 0.5f;
	color[0] = fmodf(color[0], 1.0f);
	color[1] *= satHitpoint;
	color[2] *= valHitpoint;
	
	Spectrum result = HsvToRgb(color);
	
	// Clamp color to prevent negative values caused by oversaturation
	//result = result.Clamp(0.f, 1.f); // this crashes, no idea why
	
	return result;
}

void HsvTexture::RgbToHsv(const Spectrum &rgb, float *result) const {
	float cmax, cmin, h, s, v, cdelta;
	float c[3];

	cmax = max(rgb.c[0], max(rgb.c[1], rgb.c[2]));
	cmin = min(rgb.c[0], min(rgb.c[1], rgb.c[2]));
	cdelta = cmax - cmin;
	
	v = cmax;

	if(cmax != 0.0f) {
		s = cdelta / cmax;
	}
	else {
		s = 0.0f;
		h = 0.0f;
	}

	if(s != 0.0f) {
		for(unsigned short i = 0; i < 3; i++) {
			c[i] = (cmax - rgb.c[i]) / cdelta;
		}

		if(rgb.c[0] == cmax) 
			h = c[2] - c[1];
		else if(rgb.c[1] == cmax) 
			h = 2.0f + c[0] - c[2];
		else 
			h = 4.0f + c[1] - c[0];

		h /= 6.0f;

		if(h < 0.0f)
			h += 1.0f;
	}
	else {
		h = 0.0f;
	}
	
	result[0] = h;
	result[1] = s;
	result[2] = v;
}

Spectrum HsvTexture::HsvToRgb(const float *hsv) const {
	float i, f, p, q, t, h, s, v;

	h = hsv[0];
	s = hsv[1];
	v = hsv[2];

	if(s != 0.0f) {
		if(h == 1.0f)
			h = 0.0f;

		h *= 6.0f;
		i = Floor2Int(h);
		f = h - i;
		
		p = v*(1.0f-s);
		q = v*(1.0f-(s*f));
		t = v*(1.0f-(s*(1.0f-f)));

		if(i == 0.0f) return Spectrum(v, t, p);
		else if(i == 1.0f) return Spectrum(q, v, p);
		else if(i == 2.0f) return Spectrum(p, v, t);
		else if(i == 3.0f) return Spectrum(p, q, v);
		else if(i == 4.0f) return Spectrum(t, p, v);
		else               return Spectrum(v, p, q);
	}
	else {
		return Spectrum(v, v, v);
	}
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
