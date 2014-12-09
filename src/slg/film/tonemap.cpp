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
#include <boost/serialization/export.hpp>

#include "luxrays/core/color/color.h"
#include "slg/film/tonemap.h"
#include "slg/film/imagepipelineplugins.h"
#include "slg/film/film.h"

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
		case TONEMAP_AUTOLINEAR:
			return "AUTOLINEAR";
		case TONEMAP_LUXLINEAR:
			return "LUXLINEAR";
		default:
			throw runtime_error("Unknown tone mapping type: " + boost::lexical_cast<string>(type));
	}
}

ToneMapType slg::String2ToneMapType(const std::string &type) {
	if ((type.compare("0") == 0) || (type.compare("LINEAR") == 0))
		return TONEMAP_LINEAR;
	if ((type.compare("1") == 0) || (type.compare("REINHARD02") == 0))
		return TONEMAP_REINHARD02;
	if ((type.compare("2") == 0) || (type.compare("AUTOLINEAR") == 0))
		return TONEMAP_AUTOLINEAR;
	if ((type.compare("3") == 0) || (type.compare("LUXLINEAR") == 0))
		return TONEMAP_LUXLINEAR;

	throw runtime_error("Unknown tone mapping type: " + type);
}

//------------------------------------------------------------------------------
// Auto-linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::AutoLinearToneMap)

void AutoLinearToneMap::Apply(const Film &film, Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	float Y = 0.f;
	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i]) {
			const float y = pixels[i].Y();
			if ((y <= 0.f) || isinf(y))
				continue;

			Y += y;
		}
	}
	Y /= pixelCount;

	if (Y <= 0.f)
		return;

	float gamma = 2.2f;
	const ImagePipeline *ip = film.GetImagePipeline();
	if (ip) {
		const GammaCorrectionPlugin *gc = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
		if (gc)
			gamma = gc->gamma;
	}
	
	// Substitute exposure, fstop and sensitivity cancel out; collect constants
	const float scale = (1.25f / Y * powf(118.f / 255.f, gamma));

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i])
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			pixels[i] = scale * pixels[i];
	}
}

//------------------------------------------------------------------------------
// Linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::LinearToneMap)

void LinearToneMap::Apply(const Film &film, Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i])
			pixels[i] = scale * pixels[i];
	}
}

//------------------------------------------------------------------------------
// LuxRender Linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::LuxLinearToneMap)

void LuxLinearToneMap::Apply(const Film &film, Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	float gamma = 2.2f;
	const ImagePipeline *ip = film.GetImagePipeline();
	if (ip) {
		const GammaCorrectionPlugin *gc = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
		if (gc)
			gamma = gc->gamma;
	}

	const float scale = exposure / (fstop * fstop) * sensitivity * 0.65f / 10.f * powf(118.f / 255.f, gamma);
	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i])
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			pixels[i] = scale * pixels[i];
	}
}

//------------------------------------------------------------------------------
// Reinhard02 tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Reinhard02ToneMap)

void Reinhard02ToneMap::Apply(const Film &film, Spectrum *pxls, std::vector<bool> &pixelsMask) const {
	RGBColor *rgbPixels = (RGBColor *)pxls;

	const float alpha = .1f;
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	float Ywa = 0.f;
	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i] && !rgbPixels[i].IsInf())
			Ywa += logf(max(rgbPixels[i].Y(), 1e-6f));
	}
	if (pixelCount > 0)
		Ywa = expf(Ywa / pixelCount);

	// Avoid division by zero
	if (Ywa == 0.f)
		Ywa = 1.f;

	const float invB2 = burn > 0.f ? 1.f / (burn * burn) : 1e5f;
	const float scale = alpha / Ywa;
	const float preS = scale / preScale;
	const float postS = scale * postScale;

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i]) {
			const float ys = rgbPixels[i].Y() * preS;
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			rgbPixels[i] *= postS * (1.f + ys * invB2) / (1.f + ys);
		}
	}
}
