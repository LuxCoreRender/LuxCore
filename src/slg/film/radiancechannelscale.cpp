
/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include "slg/film/film.h"
#include "luxrays/core/color/spds/blackbodyspd.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RadianceChannelScale
//------------------------------------------------------------------------------

Film::RadianceChannelScale::RadianceChannelScale() : globalScale(1.f), temperature(0.f), rgbScale(1.f),
		enabled(true) {
	Init();
}

void Film::RadianceChannelScale::Init() {
	if (temperature > 0.f) {
		BlackbodySPD spd(temperature);
		XYZColor colorTemp = spd.ToXYZ();
		colorTemp /= colorTemp.Y();

		ColorSystem colorSpace;
		scale = colorSpace.ToRGBConstrained(colorTemp).Clamp(0.f, 1.f) * rgbScale;
	} else
		scale = rgbScale;

	scale *= globalScale;
	scale = scale.Clamp(0.f);
}

void Film::RadianceChannelScale::Scale(float v[3]) const {
	v[0] *= scale.c[0];
	v[1] *= scale.c[1];
	v[2] *= scale.c[2];
}

Spectrum Film::RadianceChannelScale::Scale(const Spectrum &v) const {
	return v * scale;
}
