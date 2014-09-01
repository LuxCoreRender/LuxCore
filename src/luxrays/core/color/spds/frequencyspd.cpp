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

#include "luxrays/core/color/spds/frequencyspd.h"

using namespace luxrays;

void FrequencySPD::init(float freq, float phase, float refl) {
	fq = freq;
    ph = phase;
	r0 = refl;

	lambdaMin = FREQ_CACHE_START;
	lambdaMax = FREQ_CACHE_END;
	delta = (FREQ_CACHE_END - FREQ_CACHE_START) / (FREQ_CACHE_SAMPLES-1);
    invDelta = 1.f / delta;
	nSamples = FREQ_CACHE_SAMPLES;

	AllocateSamples(FREQ_CACHE_SAMPLES);

	// Fill samples with Frequency curve
	for(int i=0; i<FREQ_CACHE_SAMPLES; i++) {
		const float w = (FREQ_CACHE_START + (delta*i));
		samples[i] = (sinf(w * freq + phase) + 1.0f) * 0.5f * refl;
	}

	Clamp();
}
