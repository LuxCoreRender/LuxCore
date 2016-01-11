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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/contourlines.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ContourLinesPlugin plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ContourLinesPlugin)

// Scale should be set to 179.0: the value of 179 lm/w is the standard luminous
// efficacy of equal energy white light that is defined and used by Radiance. It
// can be used here to produce the same output of Radiance.
//
// More information here: http://www.radiance-online.org/pipermail/radiance-general/2006-April/003616.html

ContourLinesPlugin::ContourLinesPlugin(const float scl, const float r, const u_int s,
		const int gridSize) : scale(scl), range(r), steps(s), zeroGridSize(gridSize) {
}

ImagePipelinePlugin *ContourLinesPlugin::Copy() const {
	return new ContourLinesPlugin(scale, range, steps, zeroGridSize);
}

float ContourLinesPlugin::GetLuminance(const Film &film,
		const u_int x, const u_int y) const {
	Spectrum v;
	film.channel_IRRADIANCE->GetWeightedPixel(x, y, v.c);

	return scale * v.Y();
}

int ContourLinesPlugin::GetStep(const Film &film, const int x, const int y,
		const int defaultValue, float *normalizedValue) const {
	if ((x < 0) || (x >= (int)film.GetWidth()) ||
			(y < 0) || (y >= (int)film.GetHeight()) ||
			!(*(film.channel_FRAMEBUFFER_MASK->GetPixel(x, y))))
		return defaultValue;

	const float l = GetLuminance(film, x, y);
	if (l == 0.f)
		return -1;

	const float normVal = Clamp(l / range, 0.f, 1.f);
	if (normalizedValue)
		*normalizedValue = normVal;

	return Floor2UInt((steps - 1) * normVal);
}

static Spectrum FalseColor(const float v) {
	static Spectrum falseColors[] = {
		Spectrum(.5f, 0.f, .5f),
		Spectrum(0.f, 0.f, .5f),
		Spectrum(0.f, .75f, 0.f),
		Spectrum(1.f, 1.f, 0.f),
		Spectrum(1.f, 0.f, 0.f)
	};
	static const u_int falseColorsCount = 5;

	if (v <= 0.f)
		return falseColors[0];
	if (v >= 1.f)
		return falseColors[falseColorsCount - 1];

	const int index = Floor2UInt(v * (falseColorsCount - 1));
	const float vAtoB = v * (falseColorsCount - 1) - index;
	const Spectrum &colorA = falseColors[index];
	const Spectrum &colorB = falseColors[index + 1];

	return Lerp(vAtoB, colorA, colorB);
}

void ContourLinesPlugin::Apply(Film &film) {
	// Do nothing if the Film is missing the IRRADIANCE channel
	if (!film.HasChannel(Film::IRRADIANCE))
		return;

	// Draw the contour lines
	Spectrum *pixels = (Spectrum *)film.channel_RGB_TONEMAPPED->GetPixels();
	
	#pragma omp parallel for
	for (int s = 0; s < (int)steps; ++s) {
		for (int y = 0; y < (int)film.GetHeight(); ++y) {
			for (int x = 0; x < (int)film.GetWidth(); ++x) {
				const u_int pixelIndex = x + y * film.GetWidth();
				if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(pixelIndex))) {
					bool isBorder = false;

					float normalizedValue;
					const int myStep = GetStep(film, x, y, 0, &normalizedValue);
					if (myStep == -1) {
						// No irradiance information available or black
						if (zeroGridSize == 0.f)
							pixels[pixelIndex] = Spectrum();
						else if ((zeroGridSize > 0.f) && (
								(x % zeroGridSize == 0) || (y % zeroGridSize == 0)))
							pixels[pixelIndex] = Spectrum();
					} else {
						// Irradiance information available
						if ((GetStep(film, x - 1, y, myStep) != myStep) ||
								(GetStep(film, x + 1, y, myStep) != myStep) ||
								(GetStep(film, x, y - 1, myStep) != myStep) ||
								(GetStep(film, x, y + 1, myStep) != myStep))
							isBorder = true;

						if (isBorder) {
							const Spectrum c = FalseColor(normalizedValue);

							pixels[pixelIndex] = c;
						}
					}
				}
			}
		}
	}
}
