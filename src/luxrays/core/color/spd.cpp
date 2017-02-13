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

#include "luxrays/core/color/color.h"
#include "luxrays/core/color/spd.h"
#include "luxrays/core/color/spds/data/xyzbasis.h"
#include "luxrays/utils/memory.h"

using namespace std;
using namespace luxrays;

void SPD::AllocateSamples(u_int n) {
	 // Allocate memory for samples
	samples = AllocAligned<float>(n);
}

void SPD::FreeSamples() {
	 // Free Allocate memory for samples
	if (samples)
		FreeAligned(samples);
}

void SPD::Normalize() {
	float max = 0.f;

	for (u_int i = 0; i < nSamples; ++i)
		if(samples[i] > max)
			max = samples[i];

	const float scale = 1.f / max;

	for (u_int i = 0; i < nSamples; ++i)
		samples[i] *= scale;
}

void SPD::Clamp() {
	for (u_int i = 0; i < nSamples; ++i) {
		if (!(samples[i] > 0.f))
			samples[i] = 0.f;
	}
}

void SPD::Scale(float s) {
	for (u_int i = 0; i < nSamples; ++i)
		samples[i] *= s;
}

void SPD::Whitepoint(float temp) {
	vector<float> bbvals;

	// Fill bbvals with BB curve
	float w = lambdaMin * 1e-9f;
	for (u_int i = 0; i < nSamples; ++i) {
		// Compute blackbody power for wavelength w and temperature temp
		bbvals.push_back(4e-9f * (3.74183e-16f * powf(w, -5.f))
				/ (expf(1.4388e-2f / (w * temp)) - 1.f));
		w += 1e-9f * delta;
	}

	// Normalize
	float max = 0.f;
	for (u_int i = 0; i < nSamples; ++i)
		if (bbvals[i] > max)
			max = bbvals[i];
	const float scale = 1.f / max;
	// Multiply
	for (u_int i = 0; i < nSamples; ++i)
		samples[i] *= bbvals[i] * scale;
}

float SPD::Y() const
{
	float y = 0.f;
	for (u_int i = 0; i < nCIE; ++i)
		y += Sample(i + CIEstart) * CIE_Y[i];
	return y * 683.f;
}
float SPD::Filter() const
{
	float y = 0.f;
	for (u_int i = 0; i < nSamples; ++i)
		y += samples[i];
	return y / nSamples;
}

XYZColor SPD::ToXYZ() const {
	XYZColor c(0.f);
	for (u_int i = 0; i < nCIE; ++i) {
		const float s = Sample(i + CIEstart);
		c.c[0] += s * CIE_X[i];
		c.c[1] += s * CIE_Y[i];
		c.c[2] += s * CIE_Z[i];
	}
	return c * 683.f;
}

XYZColor SPD::ToNormalizedXYZ() const {
	XYZColor c(0.f);
	float yint  = 0.f;
	for (u_int i = 0; i < nCIE; ++i) {
		yint += CIE_Y[i];

		const float s = Sample(i + CIEstart);
		c.c[0] += s * CIE_X[i];
		c.c[1] += s * CIE_Y[i];
		c.c[2] += s * CIE_Z[i];
	}
	return c / yint;
}
