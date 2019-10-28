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

#include "luxrays/core/epsilon.h"
#include "slg/textures/triplanar.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Triplanar mapping texture
//------------------------------------------------------------------------------

float TriplanarTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
    return (texX1->GetFloatValue(hitPoint) + texY1->GetFloatValue(hitPoint) + texZ1->GetFloatValue(hitPoint))/3;
}

Spectrum TriplanarTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
    return (texX1->GetSpectrumValue(hitPoint) + texY1->GetSpectrumValue(hitPoint) + texZ1->GetSpectrumValue(hitPoint))/3;
}

Properties TriplanarTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("triplanarmapping"));
	props.Set(Property("scene.textures." + name + ".texture1")(texX1->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(texY1->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".texture3")(texZ1->GetSDLValue()));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
