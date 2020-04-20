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

#include "slg/textures/hitpoint/hitpointaov.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// HitPointVertexAOV texture
//------------------------------------------------------------------------------

float HitPointVertexAOVTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.GetVertexAOV(dataIndex);
}

Spectrum HitPointVertexAOVTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(hitPoint.GetVertexAOV(dataIndex));
}

Properties HitPointVertexAOVTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointvertexaov"));
	props.Set(Property("scene.textures." + name + ".dataIndex")(dataIndex));

	return props;
}

//------------------------------------------------------------------------------
// HitPointTriangleAOV texture
//------------------------------------------------------------------------------

float HitPointTriangleAOVTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.GetTriAOV(dataIndex);
}

Spectrum HitPointTriangleAOVTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(hitPoint.GetTriAOV(dataIndex));
}

Properties HitPointTriangleAOVTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointtriangleaov"));
	props.Set(Property("scene.textures." + name + ".dataIndex")(dataIndex));

	return props;
}
