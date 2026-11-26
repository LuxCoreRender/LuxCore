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

#include "slg/textures/fresnel/fresnelcolor.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Fresnel color texture
//------------------------------------------------------------------------------

float FresnelColorTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetKr().GetFloatValue(hitPoint);
}

Spectrum FresnelColorTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return GetKr().GetSpectrumValue(hitPoint);
}

float FresnelColorTexture::Y() const {
	return GetKr().Y();
}

float FresnelColorTexture::Filter() const {
	return GetKr().Filter();
}

Spectrum FresnelColorTexture::Evaluate(const HitPoint &hitPoint, const float cosi) const {
	const Spectrum c = GetKr().GetSpectrumValue(hitPoint);

	const Spectrum n = ApproxN(c);
	const Spectrum k = ApproxK(c);

	return GeneralEvaluate(n, k, cosi);
}

PropertiesUPtr FresnelColorTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("fresnelcolor"));
	props->Set(Property("scene.textures." + name + ".kr")(GetKr().GetSDLValue()));

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
