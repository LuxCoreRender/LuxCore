/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/core/color/swcspectrum.h"

using namespace luxrays;

Scalar SWCSpectrum::Y(const SpectrumWavelengths &sw) const {
	Scalar y = 0.f;

	if (sw.single) {
		const u_int j = sw.single_w;
		SpectrumWavelengths::spd_ciey.Sample(1,
			sw.binsXYZ + j, sw.offsetsXYZ + j, &y);
		y *= c[j] * WAVELENGTH_SAMPLES;
	} else {
		SWCSpectrum ciey;
		SpectrumWavelengths::spd_ciey.Sample(WAVELENGTH_SAMPLES,
			sw.binsXYZ, sw.offsetsXYZ, ciey.c);
		for (u_int j = 0; j < WAVELENGTH_SAMPLES; ++j) {
			y += ciey.c[j] * c[j];
		}
	}

	return y;
}

SWCSpectrum::SWCSpectrum(const SpectrumWavelengths &sw, const SPD &s) {
	s.Sample(WAVELENGTH_SAMPLES, sw.w, c);
}

SWCSpectrum::SWCSpectrum(const SpectrumWavelengths &sw, const RGBColor &s) {
	const float r = s.c[0];
	const float g = s.c[1];
	const float b = s.c[2];
	SWCSpectrum min, med, max;

	SpectrumWavelengths::spd_w.Sample(WAVELENGTH_SAMPLES,
		sw.binsRGB, sw.offsetsRGB, min.c);
	if (r <= g && r <= b) {
		min *= r;

		SpectrumWavelengths::spd_c.Sample(WAVELENGTH_SAMPLES,
			sw.binsRGB, sw.offsetsRGB, med.c);
		if (g <= b) {
			med *= g - r;
			SpectrumWavelengths::spd_b.Sample(WAVELENGTH_SAMPLES,
				sw.binsRGB, sw.offsetsRGB, max.c);
			max *= b - g;
		} else {
			med *= b - r;
			SpectrumWavelengths::spd_g.Sample(WAVELENGTH_SAMPLES,
				sw.binsRGB, sw.offsetsRGB, max.c);
			max *= g - b;
		}
	} else if (g <= r && g <= b) {
		min *= g;

		SpectrumWavelengths::spd_m.Sample(WAVELENGTH_SAMPLES,
			sw.binsRGB, sw.offsetsRGB, med.c);
		if (r <= b) {
			med *= r - g;
			SpectrumWavelengths::spd_b.Sample(WAVELENGTH_SAMPLES,
				sw.binsRGB, sw.offsetsRGB, max.c);
			max *= b - r;
		} else {
			med *= b - g;
			SpectrumWavelengths::spd_r.Sample(WAVELENGTH_SAMPLES,
				sw.binsRGB, sw.offsetsRGB, max.c);
			max *= r - b;
		}
	} else {	// blue <= red && blue <= green
		min *= b;

		SpectrumWavelengths::spd_y.Sample(WAVELENGTH_SAMPLES,
			sw.binsRGB, sw.offsetsRGB, med.c);
		if (r <= g) {
			med *= r - b;
			SpectrumWavelengths::spd_g.Sample(WAVELENGTH_SAMPLES,
				sw.binsRGB, sw.offsetsRGB, max.c);
			max *= g - r;
		} else {
			med *= g - b;
			SpectrumWavelengths::spd_r.Sample(WAVELENGTH_SAMPLES,
				sw.binsRGB, sw.offsetsRGB, max.c);
			max *= r - g;
		}
	}

	for (u_int j = 0; j < WAVELENGTH_SAMPLES; ++j)
		c[j] = min.c[j] + med.c[j] + max.c[j];
}

std::ostream &operator<<(std::ostream &stream, const SWCSpectrum &spectrum) {
	stream << "SWCSpectrum({";
	for(unsigned i = 0; i < WAVELENGTH_SAMPLES; ++i)
	{
		stream << spectrum.c[i];
		if(i + 1 < WAVELENGTH_SAMPLES)
			stream << ", ";
	}

	stream << "})";
	return stream;
}
