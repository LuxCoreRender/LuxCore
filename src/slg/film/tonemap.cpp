/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/spectrum.h"
#include "slg/film/tonemap.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Tone mapping
//------------------------------------------------------------------------------

string slg::ToneMapType2String(const ToneMapType type) {
	switch (type) {		
		case TONEMAP_LINEAR:
			return "LINEAR";
		case TONEMAP_REINHARD02:
			return "REINHARD02";
		default:
			throw runtime_error("Unknown tone mapping type: " + boost::lexical_cast<string>(type));
	}
}

ToneMapType slg::String2ToneMapType(const std::string &type) {
	if ((type.compare("0") == 0) || (type.compare("LINEAR") == 0))
		return TONEMAP_LINEAR;
	if ((type.compare("1") == 0) || (type.compare("REINHARD02") == 0))
		return TONEMAP_REINHARD02;

	throw runtime_error("Unknown tone mapping type: " + type);
}

//------------------------------------------------------------------------------
// Linear tone mapping
//------------------------------------------------------------------------------

void LinearToneMap::Apply(luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask,
		const u_int width, const u_int height) const {
	const u_int pixelCount = width * height;

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i])
			pixels[i] = scale * pixels[i];
	}
}

//------------------------------------------------------------------------------
// Reinhard02 tone mapping
//------------------------------------------------------------------------------

void Reinhard02ToneMap::Apply(luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask,
		const u_int width, const u_int height) const {
	const float alpha = .1f;
	const u_int pixelCount = width * height;

	// Use the frame buffer as temporary storage and calculate the average luminance
	float Ywa = 0.f;

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i]) {
			// Convert to XYZ color space
			pixels[i] = pixels[i].ToXYZ();

			Ywa += pixels[i].g;
		}
	}
	Ywa /= pixelCount;

	// Avoid division by zero
	if (Ywa == 0.f)
		Ywa = 1.f;

	const float Yw = preScale * alpha * burn;
	const float invY2 = 1.f / (Yw * Yw);
	const float pScale = postScale * preScale * alpha / Ywa;

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i]) {
			Spectrum xyz = pixels[i];

			const float ys = xyz.g;
			xyz *= pScale * (1.f + ys * invY2) / (1.f + ys);

			// Convert back to RGB color space
			pixels[i] = pixels[i].ToRGB();
		}
	}
}
