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

#include "slg/materials/thinfilmcoating.h"
#include "luxrays/core/color/spds/data/xyzbasis.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//---------------------------------------------------------------------------------
// Thin film coating (utility functions, to be used in materials where appropriate)
//---------------------------------------------------------------------------------

// Original LuxRender code
// void PhaseDifference(const SpectrumWavelengths &sw, const Vector &wo,
// 	float film, float filmindex, SWCSpectrum *const Pd)
// {
// 	const float swo = SinTheta(wo);
// 	const float s = sqrtf(max(0.f, filmindex * filmindex - /*1.f * 1.f * */ swo * swo));
// 	for(int i = 0; i < WAVELENGTH_SAMPLES; ++i) {
// 		const float pd = (4.f * M_PI * film / sw.w[i]) * s + M_PI;
// 		const float cpd = cosf(pd);
// 		Pd->c[i] *= cpd*cpd;
// 	}
// }

// 24 wavelengths seem to do the job. Any less and artifacs begin to appear at thickness around 2000 nm
const u_int NUM_WAVELENGTHS = 24;
const float MIN_WAVELENGTH = 380.f;
const float MAX_WAVELENGTH = 720.f;
const float WAVELENGTH_STEP = (MAX_WAVELENGTH - MIN_WAVELENGTH) / float(NUM_WAVELENGTHS - 1);

static float sampleSPD(const float lambda, const float *samples) {
	const float delta = (MAX_WAVELENGTH - MIN_WAVELENGTH) / (NUM_WAVELENGTHS - 1);
	const float invDelta = 1.f / delta;
	
	// Sample the SPD, interpolating the two closest samples linearly
	if (lambda < MIN_WAVELENGTH || lambda > MAX_WAVELENGTH)
		return 0.f;
	
	const float x = (lambda - MIN_WAVELENGTH) * invDelta;
	const u_int b0 = Floor2UInt(x);
	const u_int b1 = Min(b0 + 1, NUM_WAVELENGTHS - 1);
	const float dx = x - b0;
	const float s = Lerp(dx, samples[b0], samples[b1]);
	return s;
}

static XYZColor spdToNormalizedXYZ(const float *samples) {
	XYZColor c(0.f);
	float yint = 0.f;
	
	for (u_int i = 0; i < nCIE; ++i) {
		yint += CIE_Y[i];

		const u_int lambda = i + CIEstart;
		const float s = sampleSPD(lambda, samples);
		
		c.c[0] += s * CIE_X[i];
		c.c[1] += s * CIE_Y[i];
		c.c[2] += s * CIE_Z[i];
	}
	return c / yint;
}

Spectrum slg::CalcFilmColor(const Vector &localFixedDir, const float filmThickness, const float filmIOR, const float exteriorIOR) {
	// Prevent wrong values if the ratio between IOR and thickness is too high
	if (filmThickness * (filmIOR - .4f) > 2000.f)
		return Spectrum(.5f);
	
	float intensities[NUM_WAVELENGTHS];
	
	const float sinTheta = SinTheta(localFixedDir);
	const float s = sqrtf(Max(0.f, Sqr(filmIOR) - Sqr(exteriorIOR) * Sqr(sinTheta)));

	for (int i = 0; i < NUM_WAVELENGTHS; ++i) {
		const float waveLength = WAVELENGTH_STEP * float(i) + MIN_WAVELENGTH;
		
		const float pd = (4.f * M_PI * filmThickness / waveLength) * s + M_PI;
		intensities[i] = Sqr(cosf(pd));
	}
	
	const XYZColor normalizedXYZ = spdToNormalizedXYZ(intensities);
	const ColorSystem colorSpace(.63f, .34f, .31f, .595f, .155f, .07f, 1.f / 3.f, 1.f / 3.f, 1.f);
	const RGBColor rgb = colorSpace.ToRGBConstrained(normalizedXYZ);
	return static_cast<Spectrum>(rgb);
}
