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

#include "slg/textures/clamp.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Clamp texture
//------------------------------------------------------------------------------

float ClampTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return Clamp(tex->GetFloatValue(hitPoint), minVal, maxVal);
}

Spectrum ClampTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return tex->GetSpectrumValue(hitPoint).Clamp(minVal, maxVal);
}

Properties ClampTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("clamp"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetName()));
	props.Set(Property("scene.textures." + name + ".min")(minVal));
	props.Set(Property("scene.textures." + name + ".max")(maxVal));

	return props;
}
