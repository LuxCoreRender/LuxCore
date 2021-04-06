/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include "slg/scene/colorspaceconverters.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ColorSpaceConverters
//------------------------------------------------------------------------------

ColorSpaceConverters::ColorSpaceConverters() {
}

ColorSpaceConverters::~ColorSpaceConverters() {
}

void ColorSpaceConverters::ConvertFromLuxCore(const float gamma, float &v) {
	Spectrum c(v);

	ConvertFromLuxCore(gamma, c);

	v = c.Y();
}

void ColorSpaceConverters::ConvertFromLuxCore(const float gamma, Spectrum &c) {
	c.c[0] = powf(c.c[0], gamma);
	c.c[1] = powf(c.c[1], gamma);
	c.c[2] = powf(c.c[2], gamma);
}
