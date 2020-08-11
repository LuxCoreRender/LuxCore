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

#include "slg/textures/normalmap.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// NormalMap texture
//------------------------------------------------------------------------------

NormalMapTexture::NormalMapTexture(const Texture *t, const float s) : tex(t), scale(s) {
}

NormalMapTexture::~NormalMapTexture() {
}

Normal NormalMapTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
    const Spectrum rgb = tex->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);

	// Normal from normal map
	Vector n(rgb.c);
	n = 2.f * n - Vector(1.f, 1.f, 1.f);
	n.x *= scale;
	n.y *= scale;

	const Normal oldShadeN = hitPoint.shadeN;

	// Transform n from tangent to object space
	Normal shadeN = Normal(Normalize(hitPoint.GetFrame().ToWorld(n)));
	shadeN *= (Dot(oldShadeN, shadeN) < 0.f) ? -1.f : 1.f;

	return shadeN;
}

Properties NormalMapTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;
	
	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("normalmap"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".scale")(scale));

	return props;
}
