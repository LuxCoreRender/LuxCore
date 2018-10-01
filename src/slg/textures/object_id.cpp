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

#include "slg/textures/object_id.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ObjectID texture
//------------------------------------------------------------------------------

float ObjectIDTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return static_cast<float>(hitPoint.objectID);
}

Spectrum ObjectIDTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(hitPoint.objectID);
}

Properties ObjectIDTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("objectid"));

	return props;
}

//------------------------------------------------------------------------------
// ObjectIDColor texture
//------------------------------------------------------------------------------

float ObjectIDColorTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum ObjectIDColorTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const u_int objID = hitPoint.objectID;
	return Spectrum((objID & 0x0000ffu) * ( 1.f / 255.f),
	                ((objID & 0x00ff00u) >> 8) * ( 1.f / 255.f),
	                ((objID & 0xff0000u) >> 16) * ( 1.f / 255.f));
}

Properties ObjectIDColorTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("objectidcolor"));

	return props;
}

//------------------------------------------------------------------------------
// ObjectIDNormalized texture
//------------------------------------------------------------------------------

float ObjectIDNormalizedTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return static_cast<float>(hitPoint.objectID) * (1.f / 0xffffffu);
}

Spectrum ObjectIDNormalizedTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties ObjectIDNormalizedTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("objectidnormalized"));

	return props;
}
