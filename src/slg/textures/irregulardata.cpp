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

#include "luxrays/core/color/spds/irregular.h"

#include "slg/textures/irregulardata.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Irregular data texture
//------------------------------------------------------------------------------

IrregularDataTexture::IrregularDataTexture(const u_int n,
		const float *wl, const float *dt,
		const float resolution, bool em) :
	waveLengths(n), data(n), emission(em)
{
	copy(wl, wl + n, waveLengths.begin());
	copy(dt, dt + n, data.begin());

	IrregularSPD spd(&waveLengths[0], &data[0], n, resolution);

	if (emission) {
		ColorSystem colorSpace;
		rgb = colorSpace.ToRGBConstrained(spd.ToXYZ()).Clamp(0.f);
	} else {
		ColorSystem colorSpace(.63f, .34f, .31f, .595f, .155f, .07f,
			1.f / 3.f, 1.f / 3.f, 1.f);
		rgb = colorSpace.ToRGBConstrained(spd.ToNormalizedXYZ()).Clamp(0.f);
	}
}

Properties IrregularDataTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("irregulardata"));
	props.Set(Property("scene.textures." + name + ".wavelengths")(waveLengths));
	props.Set(Property("scene.textures." + name + ".data")(data));
	props.Set(Property("scene.textures." + name + ".emission")(emission));

	return props;
}
