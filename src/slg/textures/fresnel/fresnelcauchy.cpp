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

#include <fstream>

#include "luxrays/core/color/spds/irregular.h"
#include "slg/textures/fresnel/fresnelcauchy.h"
#include "slg/textures/fresnel/fresnelconst.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Fresnel cauchy texture
//------------------------------------------------------------------------------

static FresnelTexture *MakeCauchy(float a, float b)
{
	vector<float> wl;
	vector<float> n;

	for (float i = 380.f; i < 720.f; i += 10.f) {
		wl.push_back(i);
		n.push_back(a + b * 1e6f / (i * i));
	}

	IrregularSPD N(&wl[0], &n[0], wl.size());

	ColorSystem colorSpace(.63f, .34f, .31f, .595f, .155f, .07f,
		1.f / 3.f, 1.f / 3.f, 1.f);
	const RGBColor Nrgb = colorSpace.ToRGBConstrained(N.ToNormalizedXYZ());

	return new FresnelConstTexture(Nrgb, Spectrum(0.f));
}

FresnelTexture *slg::AllocFresnelCauchyTex(const luxrays::Properties &props, const std::string &propName)
{
	const float b = props.Get(Property(propName + ".b")(0.f)).Get<float>();
	const float index = props.Get(Property(propName + ".index")(-1.f)).Get<float>();
	float a;
	if (index > 0.f)
		a = props.Get(Property(propName + ".a")(index - b * 1e6f / (720.f * 380.f))).Get<float>();
	else
		a = props.Get(Property(propName + ".a")(1.5f)).Get<float>();

	return MakeCauchy(a, b);
}

FresnelTexture *slg::AllocFresnelAbbeTex(const luxrays::Properties &props, const std::string &propName)
{
	const string mode(props.Get(Property(propName + ".mode")("d")).Get<string>());

	float d = 587.5618f;
	float f = 486.13f;
	float c = 656.28f;
	if (mode == "D") {
		d = 589.29f;
	} else if (mode == "e") {
		d = 546.07f;
		f = 479.99f;
		c = 643.85f;
	} else if (mode == "custom") {
		d = props.Get(Property(propName + ".d")(d)).Get<float>();
		f = props.Get(Property(propName + ".f")(f)).Get<float>();
		c = props.Get(Property(propName + ".c")(c)).Get<float>();
	}
	const float v = props.Get(Property(propName + ".v")(64.17f)).Get<float>();
	const float n = props.Get(Property(propName + ".n")(1.5168f)).Get<float>();

	// Convert to cauchy coefficients
	// convert wavelengths from nm to um
	d *= 1e-3f;
	f *= 1e-3f;
	c *= 1e-3f;

	// square
	d *= d;
	f *= f;
	c *= c;

	const float b = (n - 1.f) * (c * f) / (v * (c - f));
	const float a = n - b / d;

	return MakeCauchy(a, b);
}
