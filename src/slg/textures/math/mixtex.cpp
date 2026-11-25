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

#include "slg/textures/math/mix.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

float MixTexture::Y() const {
	return Lerp(GetAmountTexture().Y(), GetTexture1().Y(), GetTexture2().Y());
}

float MixTexture::Filter() const {
	return Lerp(GetAmountTexture().Filter(), GetTexture1().Filter(), GetTexture2().Filter());
}

float MixTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(GetAmountTexture().GetFloatValue(hitPoint), 0.f, 1.f);
	const float value1 = GetTexture1().GetFloatValue(hitPoint);
	const float value2 = GetTexture2().GetFloatValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Spectrum MixTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(GetAmountTexture().GetFloatValue(hitPoint), 0.f, 1.f);
	const Spectrum value1 = GetTexture1().GetSpectrumValue(hitPoint);
	const Spectrum value2 = GetTexture2().GetSpectrumValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Normal MixTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	const Vector u = Normalize(hitPoint.dpdu);
	const Vector v = Normalize(Cross(Vector(hitPoint.shadeN), hitPoint.dpdu));
	Normal n = GetTexture1().Bump(hitPoint, sampleDistance);
	float nn = Dot(n, hitPoint.shadeN);
	const float du1 = Dot(n, u) / nn;
	const float dv1 = Dot(n, v) / nn;

	n = GetTexture2().Bump(hitPoint, sampleDistance);
	nn = Dot(n, hitPoint.shadeN);
	const float du2 = Dot(n, u) / nn;
	const float dv2 = Dot(n, v) / nn;

	n = GetAmountTexture().Bump(hitPoint, sampleDistance);
	nn = Dot(n, hitPoint.shadeN);
	const float dua = Dot(n, u) / nn;
	const float dva = Dot(n, v) / nn;

	const float t1 = GetTexture1().GetFloatValue(hitPoint);
	const float t2 = GetTexture2().GetFloatValue(hitPoint);
	const float amt = Clamp(GetAmountTexture().GetFloatValue(hitPoint), 0.f, 1.f);

	const float du = Lerp(amt, du1, du2) + dua * (t2 - t1);
	const float dv = Lerp(amt, dv1, dv2) + dva * (t2 - t1);

	return Normal(Normalize(Vector(hitPoint.shadeN) + du * u + dv * v));
}

PropertiesUPtr MixTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("mix"));
	props->Set(Property("scene.textures." + name + ".amount")(GetAmountTexture().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture1")(GetTexture1().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture2")(GetTexture2().GetSDLValue()));

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
