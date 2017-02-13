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

#include "slg/textures/fresnel/fresnelconst.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Fresnel const texture
//------------------------------------------------------------------------------

float FresnelConstTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return 0.f;
}

Spectrum FresnelConstTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return GeneralEvaluate(n, k, .5f);
}

float FresnelConstTexture::Y() const {
	return 0.f;
}

float FresnelConstTexture::Filter() const {
	return 0.f;
}

Spectrum FresnelConstTexture::Evaluate(const HitPoint &hitPoint, const float cosi) const {
	return GeneralEvaluate(n, k, cosi);
}

Properties FresnelConstTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("fresnelconst"));
	props.Set(Property("scene.textures." + name + ".n")(n));
	props.Set(Property("scene.textures." + name + ".k")(k));

	return props;
}
