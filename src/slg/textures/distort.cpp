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

void DistortTexture::GetTmpHitPoint(const HitPoint &hitPoint, HitPoint &tmpHitPoint) const {
	const Spectrum offsetColor = offset->GetSpectrumValue(hitPoint);
	const Vector offset = Vector(offsetColor.c[0], offsetColor.c[1], offsetColor.c[2]) * strength;
	
	tmpHitPoint = hitPoint;
	tmpHitPoint.p += offset;
	tmpHitPoint.defaultUV.u += offset.x;
	tmpHitPoint.defaultUV.v += offset.y;
}

Spectrum DistortTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	HitPoint tmpHitPoint;
	GetTmpHitPoint(hitPoint, tmpHitPoint);
	
	return tex->GetSpectrumValue(tmpHitPoint);
}

float DistortTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
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
