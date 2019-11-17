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

#include "slg/textures/hitpoint/hitpointcolor.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

float HitPointColorTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.color[dataIndex].Y();
}

Spectrum HitPointColorTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return hitPoint.color[dataIndex];
}

Properties HitPointColorTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointcolor"));
	props.Set(Property("scene.textures." + name + ".index")(dataIndex));

	return props;
}

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

float HitPointAlphaTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.alpha[dataIndex];
}

Spectrum HitPointAlphaTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(hitPoint.alpha[dataIndex]);
}

Properties HitPointAlphaTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointalpha"));
	props.Set(Property("scene.textures." + name + ".index")(dataIndex));

	return props;
}

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

float HitPointGreyTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return (channel > 2) ? hitPoint.color[dataIndex].Y() : hitPoint.color[dataIndex].c[channel];
}

Spectrum HitPointGreyTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float v = (channel > 2) ? hitPoint.color[dataIndex].Y() : hitPoint.color[dataIndex].c[channel];
	return Spectrum(v);
}

Properties HitPointGreyTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("hitpointgrey"));
	props.Set(Property("scene.textures." + name + ".index")(dataIndex));
	props.Set(Property("scene.textures." + name + ".channel")(
		((channel != 0) && (channel != 1) && (channel != 2)) ? -1 : ((int)channel)));

	return props;
}
