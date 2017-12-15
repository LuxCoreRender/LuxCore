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

#include "slg/textures/marble.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

Spectrum MarbleTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));
	P *= scale;

	float marble = P.y + variation * FBm(P, omega, octaves);
	float t = .5f + .5f * sinf(marble);
	// Evaluate marble spline at _t_
	static const float c[9][3] = {
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{ .5f, .5f, .5f},
		{ .6f, .59f, .58f},
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{.2f, .2f, .33f},
		{ .58f, .58f, .6f}
	};
#define NC  sizeof(c) / sizeof(c[0])
#define NSEG (NC-3)
	int first = Floor2Int(t * NSEG);
	t = (t * NSEG - first);
#undef NC
#undef NSEG
	Spectrum c0(c[first]), c1(c[first + 1]), c2(c[first + 2]), c3(c[first + 3]);
	// Bezier spline evaluated with de Castilejau's algorithm
	Spectrum s0(Lerp(t, c0, c1));
	Spectrum s1(Lerp(t, c1, c2));
	Spectrum s2(Lerp(t, c2, c3));
	s0 = Lerp(t, s0, s1);
	s1 = Lerp(t, s1, s2);
	// Extra scale of 1.5 to increase variation among colors
	return 1.5f * Lerp(t, s0, s1);
}

float MarbleTexture::Y() const {
	static float c[][3] = { { .58f, .58f, .6f }, { .58f, .58f, .6f }, { .58f, .58f, .6f },
		{ .5f, .5f, .5f }, { .6f, .59f, .58f }, { .58f, .58f, .6f },
		{ .58f, .58f, .6f }, {.2f, .2f, .33f }, { .58f, .58f, .6f }, };
	Spectrum cs;
#define NC  sizeof(c) / sizeof(c[0])
	for (u_int i = 0; i < NC; ++i)
		cs += Spectrum(c[i]);
	return cs.Y() / NC;
#undef NC
}

float MarbleTexture::Filter() const {
	static float c[][3] = { { .58f, .58f, .6f }, { .58f, .58f, .6f }, { .58f, .58f, .6f },
		{ .5f, .5f, .5f }, { .6f, .59f, .58f }, { .58f, .58f, .6f },
		{ .58f, .58f, .6f }, {.2f, .2f, .33f }, { .58f, .58f, .6f }, };
	Spectrum cs;
#define NC  sizeof(c) / sizeof(c[0])
	for (u_int i = 0; i < NC; ++i)
		cs += Spectrum(c[i]);
	return cs.Filter() / NC;
#undef NC
}

float MarbleTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Properties MarbleTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("marble"));
	props.Set(Property("scene.textures." + name + ".octaves")(octaves));
	props.Set(Property("scene.textures." + name + ".roughness")(omega));
	props.Set(Property("scene.textures." + name + ".scale")(scale));
	props.Set(Property("scene.textures." + name + ".variation")(variation));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
