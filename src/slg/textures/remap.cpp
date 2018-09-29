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

#include "slg/textures/remap.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Remap texture
//------------------------------------------------------------------------------

float RemapTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const float value = valueTex->GetFloatValue(hitPoint);
	const float sourceMin = sourceMinTex->GetFloatValue(hitPoint);
	const float sourceMax = sourceMaxTex->GetFloatValue(hitPoint);
	const float targetMin = targetMinTex->GetFloatValue(hitPoint);
	const float targetMax = targetMaxTex->GetFloatValue(hitPoint);
	
	return Remap(value, sourceMin, sourceMax, targetMin, targetMax);
}

Spectrum RemapTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Spectrum value = valueTex->GetFloatValue(hitPoint);
	const Spectrum sourceMin = sourceMinTex->GetFloatValue(hitPoint);
	const Spectrum sourceMax = sourceMaxTex->GetFloatValue(hitPoint);
	const Spectrum targetMin = targetMinTex->GetFloatValue(hitPoint);
	const Spectrum targetMax = targetMaxTex->GetFloatValue(hitPoint);
	
	return Remap(value, sourceMin, sourceMax, targetMin, targetMax);
}

float RemapTexture::Y() const {
	const float valueY = valueTex->Y();
	const float sourceMinY = sourceMinTex->Y();
	const float sourceMaxY = sourceMaxTex->Y();
	const float targetMinY = targetMinTex->Y();
	const float targetMaxY = targetMaxTex->Y();
	
	return Remap(valueY, sourceMinY, sourceMaxY, targetMinY, targetMaxY);
}

float RemapTexture::Filter() const {
	const float valueFilter = valueTex->Filter();
	const float sourceMinFilter = sourceMinTex->Filter();
	const float sourceMaxFilter = sourceMaxTex->Filter();
	const float targetMinFilter = targetMinTex->Filter();
	const float targetMaxFilter = targetMaxTex->Filter();
	
	return Remap(valueFilter, sourceMinFilter, sourceMaxFilter, targetMinFilter, targetMaxFilter);
}

Properties RemapTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("remap"));
	props.Set(Property("scene.textures." + name + ".value")(valueTex->GetName()));
	props.Set(Property("scene.textures." + name + ".sourcemin")(sourceMinTex->GetName()));
	props.Set(Property("scene.textures." + name + ".sourcemax")(sourceMaxTex->GetName()));
	props.Set(Property("scene.textures." + name + ".targetmin")(targetMinTex->GetName()));
	props.Set(Property("scene.textures." + name + ".targetmax")(targetMaxTex->GetName()));

	return props;
}
