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

#ifndef _LUXRAYS_REGULARSPD_H
#define _LUXRAYS_REGULARSPD_H

#include "luxrays/core/color/spd.h"

namespace luxrays {

// regularly sampled SPD, reconstructed using linear interpolation
class RegularSPD : public SPD {
public:
	RegularSPD() : SPD() {}

	//  creates a regularly sampled SPD
	//  samples    array of sample values
	//  lambdaMin  wavelength (nm) of first sample
	//  lambdaMax  wavelength (nm) of last sample
	//  n          number of samples
	RegularSPD(const float* const s, float lMin, float lMax, u_int n) : SPD() {
		init(lMin, lMax, s, n);
	}
	RegularSPD(const float* const s, float lMin, float lMax, u_int n, float scale) : SPD() {
		init(lMin, lMax, s, n);
		Scale(scale);
	}

	virtual ~RegularSPD() {}

protected:
	void init(float lMin, float lMax, const float* const s, u_int n);
};

}

#endif // _LUXRAYS_REGULARSPD_H
