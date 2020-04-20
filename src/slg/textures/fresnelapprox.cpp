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

#include "slg/textures/fresnelapprox.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//------------------------------------------------------------------------------

float FresnelApproxN(const float Fr) {
	const float sqrtReflectance = sqrtf(Clamp(Fr, 0.f, .999f));

	return (1.f + sqrtReflectance) /
		(1.f - sqrtReflectance);
}

Spectrum FresnelApproxN(const Spectrum &Fr) {
	const Spectrum sqrtReflectance = Fr.Clamp(0.f, .999f).Sqrt();

	return (Spectrum(1.f) + sqrtReflectance) /
		(Spectrum(1.f) - sqrtReflectance);
}

float FresnelApproxK(const float Fr) {
	const float reflectance = Clamp(Fr, 0.f, .999f);

	return 2.f * sqrtf(reflectance /
		(1.f - reflectance));
}

Spectrum FresnelApproxK(const Spectrum &Fr) {
	const Spectrum reflectance = Fr.Clamp(0.f, .999f);

	return 2.f * Sqrt(reflectance /
		(Spectrum(1.f) - reflectance));
}

float FresnelApproxNTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetFloatValue(hitPoint));
}

Spectrum FresnelApproxNTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetSpectrumValue(hitPoint));
}

float FresnelApproxNTexture::Y() const {
	return FresnelApproxN(tex->Y());
}

float FresnelApproxNTexture::Filter() const {
	return FresnelApproxN(tex->Filter());
}

float FresnelApproxKTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetFloatValue(hitPoint));
}

Spectrum FresnelApproxKTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetSpectrumValue(hitPoint));
}

float FresnelApproxKTexture::Y() const {
	return FresnelApproxK(tex->Y());
}

float FresnelApproxKTexture::Filter() const {
	return FresnelApproxK(tex->Filter());
}

Properties FresnelApproxNTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("fresnelapproxn"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetSDLValue()));

	return props;
}

Properties FresnelApproxKTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("fresnelapproxk"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetSDLValue()));

	return props;
}
