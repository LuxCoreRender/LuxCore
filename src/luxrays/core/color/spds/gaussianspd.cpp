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

#include "luxrays/core/color/spds/gaussianspd.h"

using namespace luxrays;

void GaussianSPD::init(float mean, float width, float refl) {
	mu = mean;
    wd = width;
	r0 = refl;

	const float scale2 = float(-0.5 / (width * width));

	lambdaMin = GAUSS_CACHE_START;
	lambdaMax = GAUSS_CACHE_END;
	delta = (GAUSS_CACHE_END - GAUSS_CACHE_START) / (GAUSS_CACHE_SAMPLES-1);
    invDelta = 1.f / delta;
	nSamples = GAUSS_CACHE_SAMPLES;

	AllocateSamples(GAUSS_CACHE_SAMPLES);

	// Fill samples with Gaussian curve
	for(int i=0; i<GAUSS_CACHE_SAMPLES; i++) {
		const float w = (GAUSS_CACHE_START + (delta*i));
		const float x = w - mu;
		samples[i] = refl * expf(x * x * scale2);
	}

	Clamp();
}
