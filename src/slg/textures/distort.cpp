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

#include "slg/textures/distort.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Distort texture
//------------------------------------------------------------------------------

HitPoint DistortTexture::GetTmpHitPoint(const HitPoint &hitPoint) const {
	// TODO strength parameter?
	
	const Spectrum offsetColor = offset->GetSpectrumValue(hitPoint);
	const Vector offset = Vector(offsetColor.c[0], offsetColor.c[1], offsetColor.c[2]) * strength;
	
	HitPoint hitPointTmp = hitPoint;
	hitPointTmp.p += offset;
	hitPointTmp.defaultUV.u += offset.x;
	hitPointTmp.defaultUV.v += offset.y;
	return hitPointTmp;
}

float DistortTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return tex->GetFloatValue(GetTmpHitPoint(hitPoint));
}

Spectrum DistortTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return tex->GetSpectrumValue(GetTmpHitPoint(hitPoint));
}

Properties DistortTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("distort"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".offset")(offset->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".strength")(strength));

	return props;
}
