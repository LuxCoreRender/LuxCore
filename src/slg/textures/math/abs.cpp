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

#include "slg/textures/math/abs.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Abs texture
//------------------------------------------------------------------------------

float AbsTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return fabsf(GetTexture().GetFloatValue(hitPoint));
}

Spectrum AbsTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return GetTexture().GetSpectrumValue(hitPoint).Abs();
}

PropertiesUPtr AbsTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("abs"));
	props->Set(Property("scene.textures." + name + ".texture")(GetTexture().GetSDLValue()));

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
