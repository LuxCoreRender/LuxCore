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

#include "luxrays/core/color/spds/rgbillum.h"
#include "luxrays/core/color/spds/data/rgbD65_32.h"

using namespace luxrays;

void RGBIllumSPD::init(const RGBColor &s) {
	lambdaMin = illumrgb2spect_start;
	lambdaMax = illumrgb2spect_end;
	const u_int n = illumrgb2spect_bins;
	delta = (lambdaMax - lambdaMin) / (n - 1);
	invDelta = 1.f / delta;
	nSamples = n;

	AllocateSamples(n);

	// Zero out
	for (u_int i = 0; i < n; ++i)
		samples[i] = 0.f;

	float r = s.c[0];
	float g = s.c[1];
	float b = s.c[2];

	if (r <= g && r <= b) {
		AddWeighted(r, illumrgb2spect_white);

		if (g <= b) {
			AddWeighted(g - r, illumrgb2spect_cyan);
			AddWeighted(b - g, illumrgb2spect_blue);
		} else {
			AddWeighted(b - r, illumrgb2spect_cyan);
			AddWeighted(g - b, illumrgb2spect_green);
		}
	} else if (g <= r && g <= b) {
		AddWeighted(g, illumrgb2spect_white);

		if (r <= b) {
			AddWeighted(r - g, illumrgb2spect_magenta);
			AddWeighted(b - r, illumrgb2spect_blue);
		} else {
			AddWeighted(b - g, illumrgb2spect_magenta);
			AddWeighted(r - b, illumrgb2spect_red);
		}
	} else { // blue <= red && blue <= green
		AddWeighted(b, illumrgb2spect_white);

		if (r <= g) {
			AddWeighted(r - b, illumrgb2spect_yellow);
			AddWeighted(g - r, illumrgb2spect_green);
		} else {
			AddWeighted(g - b, illumrgb2spect_yellow);
			AddWeighted(r - g, illumrgb2spect_red);
		}
	}

	Scale(illumrgb2spect_scale);
	Clamp();
}
