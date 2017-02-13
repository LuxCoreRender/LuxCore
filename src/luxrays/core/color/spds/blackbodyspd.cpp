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

#include "luxrays/core/color/spds/blackbodyspd.h"

using namespace luxrays;

void BlackbodySPD::init(float t) {
	temp = t;

	lambdaMin = BB_CACHE_START;
	lambdaMax = BB_CACHE_END;
	delta = (BB_CACHE_END - BB_CACHE_START) / (BB_CACHE_SAMPLES-1);
	invDelta = 1.f / delta;
	nSamples = BB_CACHE_SAMPLES;

	AllocateSamples(BB_CACHE_SAMPLES);

	// Fill samples with BB curve
	for(int i=0; i<BB_CACHE_SAMPLES; i++) {
		const float w = 1e-9f * (BB_CACHE_START + (delta*i));
		// Compute blackbody power for wavelength w and temperature temp
		samples[i] = 0.4e-9f * (3.74183e-16f * powf(w, -5.f))
				/ (expf(1.4388e-2f / (w * temp)) - 1.f);
	}

	Normalize();
	Clamp();
}
