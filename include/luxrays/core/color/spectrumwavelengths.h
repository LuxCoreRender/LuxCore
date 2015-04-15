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

#ifndef _LUXRAYS_SPECTRUMWAVELENGTHS_H
#define _LUXRAYS_SPECTRUMWAVELENGTHS_H

#include "luxrays/core/color/spds/regular.h"

namespace luxrays {

#define WAVELENGTH_SAMPLES 4
#define WAVELENGTH_START 380.f
#define WAVELENGTH_END   720.f
static const float inv_WAVELENGTH_SAMPLES = 1.f / WAVELENGTH_SAMPLES;

class SpectrumWavelengths {
public:

	// SpectrumWavelengths Public Methods
	SpectrumWavelengths() : single_w(0), single(false) { }
	~SpectrumWavelengths() { }

	inline void Sample(float u1) {
		single = false;
		u1 *= WAVELENGTH_SAMPLES;
		single_w = Floor2UInt(u1);
		u1 -= single_w;

		// Sample new stratified wavelengths and precompute RGB/XYZ data
		const float offset = float(WAVELENGTH_END - WAVELENGTH_START) * inv_WAVELENGTH_SAMPLES;
		float waveln = WAVELENGTH_START + u1 * offset;
		for (u_int i = 0; i < WAVELENGTH_SAMPLES; ++i) {
			w[i] = waveln;
			waveln += offset;
		}
		spd_w.Offsets(WAVELENGTH_SAMPLES, w, binsRGB, offsetsRGB);
		spd_ciex.Offsets(WAVELENGTH_SAMPLES, w, binsXYZ, offsetsXYZ);
	}

	inline float SampleSingle() const {
		single = true;
		return w[single_w];
	}

	float w[WAVELENGTH_SAMPLES]; // Wavelengths in nm

	u_int  single_w; // Chosen single wavelength bin
	mutable bool single; // Split to single

	int binsRGB[WAVELENGTH_SAMPLES], binsXYZ[WAVELENGTH_SAMPLES];
	float offsetsRGB[WAVELENGTH_SAMPLES], offsetsXYZ[WAVELENGTH_SAMPLES];

	static const RegularSPD spd_w, spd_c, spd_m, spd_y,
		spd_r, spd_g, spd_b, spd_ciex, spd_ciey, spd_ciez;
};


}

#endif // _LUXRAYS_SPECTRUMWAVELENGTHS_H
