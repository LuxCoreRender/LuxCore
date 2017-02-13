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

#include "slg/textures/wrinkled.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

float WrinkledTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p(mapping->Map(hitPoint));
	return Turbulence(p, omega, octaves);
}

Spectrum WrinkledTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties WrinkledTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("wrinkled"));
	props.Set(Property("scene.textures." + name + ".octaves")(octaves));
	props.Set(Property("scene.textures." + name + ".roughness")(omega));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
