/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/core/color/color.h"
#include "luxrays/core/color/spd.h"
#include "luxrays/core/color/spds/data/xyzbasis.h"
#include <numeric>

using namespace std;
using namespace luxrays;

void SPD::AllocateSamples(u_int n) {
	 // Allocate memory for samples
	_samples.resize(n);
}


void SPD::Normalize() {
	float max = std::ranges::max(samples());

	const float scale = 1.f / max;

	Scale(scale);
}

void SPD::Clamp() {
	std::ranges::for_each(samples(), [](float& x) { if (x <= 0.f) x = 0.f; });
}

void SPD::Scale(float scale) {
	std::ranges::for_each(samples(), [scale](float& x) { x*= scale; });
}

void SPD::Whitepoint(float temp) {

	std::vector<float> bbvals(nSamples);

	const float w0 = lambdaMin * 1e-9f;  // starting wavelength, captured before the loop

	// Fill bbvals with BB curve
	for (unsigned i = 0; i < nSamples; ++i) {
		const float wi = w0 + static_cast<float>(i) * (1e-9f * delta);
		bbvals[i] = 4e-9f * (3.74183e-16f * std::pow(wi, -5.f))
				  / (std::exp(1.4388e-2f / (wi * temp)) - 1.f);
	}

	// Get scale
	float max = std::ranges::max(bbvals);
	const float scale = 1.f / max;

	// Apply bbval to this
	std::ranges::transform(samples(), bbvals, samples().begin(), std::multiplies<>{});

	Scale(scale);
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
    const float sum = std::accumulate(samples().begin(), samples().end(), 0.f);
    return sum / static_cast<float>(nSamples);
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



void SPD::AddWeighted(float w, const float *c) {
	std::span<float> samps = samples();
	std::span<const float> cs{c, nSamples};

	std::ranges::transform(samps, cs, samps.begin(),
		[w](float s, float ci) { return s + ci * w; });
}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
