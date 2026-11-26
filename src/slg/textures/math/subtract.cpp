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

#include "slg/textures/math/subtract.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Subtract texture
//------------------------------------------------------------------------------

float SubtractTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetTexture1().GetFloatValue(hitPoint) - GetTexture2().GetFloatValue(hitPoint);
}

Spectrum SubtractTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return GetTexture1().GetSpectrumValue(hitPoint) - GetTexture2().GetSpectrumValue(hitPoint);
}

Normal SubtractTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	const Normal tex1ShadeN = GetTexture1().Bump(hitPoint, sampleDistance);
	const Normal tex2ShadeN = GetTexture2().Bump(hitPoint, sampleDistance);

	// Same of Normalize(hitPoint.shadeN + (tex1ShadeN - hitPoint.shadeN) - (tex2ShadeN - hitPoint.shadeN))
	return Normalize(tex1ShadeN - tex2ShadeN + hitPoint.shadeN);
}

PropertiesUPtr SubtractTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();
	
	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("subtract"));
	props->Set(Property("scene.textures." + name + ".texture1")(GetTexture1().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture2")(GetTexture2().GetSDLValue()));
	
	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
