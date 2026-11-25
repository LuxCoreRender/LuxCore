/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "slg/textures/vectormath/dotproduct.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Dot product texture
//------------------------------------------------------------------------------

float DotProductTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return Dot(GetTexture1().GetSpectrumValue(hitPoint), GetTexture2().GetSpectrumValue(hitPoint));
}

Spectrum DotProductTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

PropertiesUPtr DotProductTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("dotproduct"));
	props->Set(Property("scene.textures." + name + ".texture1")(GetTexture1().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture2")(GetTexture2().GetSDLValue()));

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
