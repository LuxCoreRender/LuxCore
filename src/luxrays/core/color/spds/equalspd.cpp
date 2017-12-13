/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/core/color/spds/equalspd.h"

using namespace luxrays;

void EqualSPD::init(float p) {
	power = p;

	lambdaMin = EQ_CACHE_START;
	lambdaMax = EQ_CACHE_END;
	delta = (EQ_CACHE_END - EQ_CACHE_START) / (EQ_CACHE_SAMPLES-1);
    invDelta = 1.f / delta;
	nSamples = EQ_CACHE_SAMPLES;

	AllocateSamples(EQ_CACHE_SAMPLES);

	// Fill sample with power value
	for(int i=0; i<EQ_CACHE_SAMPLES; i++) {
		samples[i] = power;
	}

	Clamp();
}
