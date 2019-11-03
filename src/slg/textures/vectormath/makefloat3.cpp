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

#include "slg/textures/vectormath/makefloat3.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Make float3 texture
//------------------------------------------------------------------------------

float MakeFloat3Texture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum MakeFloat3Texture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float v1 = tex1->GetFloatValue(hitPoint);
	const float v2 = tex2->GetFloatValue(hitPoint);
	const float v3 = tex3->GetFloatValue(hitPoint);
	return Spectrum(v1, v2, v3);
}

Properties MakeFloat3Texture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("makefloat3"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture3")(tex3->GetSDLValue()));

	return props;
}
