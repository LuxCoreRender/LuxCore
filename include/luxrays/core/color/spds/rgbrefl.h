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

#ifndef _LUXRAYS_RGBREFLSPD_H
#define _LUXRAYS_RGBREFLSPD_H

#include "luxrays/core/color/color.h"
#include "luxrays/core/color/spd.h"

namespace luxrays {

// reflectant SPD, from RGB color, using smits conversion, reconstructed using linear interpolation
class RGBReflSPD : public SPD {
public:
	RGBReflSPD() : SPD() { init(RGBColor(1.f)); }

	RGBReflSPD(const RGBColor &s) : SPD() { init(s); }

	virtual ~RGBReflSPD() {}

protected:
	void AddWeighted(float w, float *c) {
		for (u_int i = 0; i < nSamples; ++i) {
			samples[i] += c[i] * w;
		}
	}

	void init(const RGBColor &s);
};

}

#endif // _LUXRAYS_RGBREFLSPD_H
