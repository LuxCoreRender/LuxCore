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

#include "slg/textures/math/scale.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

float ScaleTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetTexture1().GetFloatValue(hitPoint) * GetTexture2().GetFloatValue(hitPoint);
}

Spectrum ScaleTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return GetTexture1().GetSpectrumValue(hitPoint) * GetTexture2().GetSpectrumValue(hitPoint);
}

Normal ScaleTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	const Vector u = Normalize(hitPoint.dpdu);
	const Vector v = Normalize(Cross(Vector(hitPoint.shadeN), hitPoint.dpdu));

	const Normal n1 = GetTexture1().Bump(hitPoint, sampleDistance);
	const float nn1 = Dot(n1, hitPoint.shadeN);
	float du1, dv1;
	if (nn1 != 0.f) {
		du1 = Dot(n1, u) / nn1;
		dv1 = Dot(n1, v) / nn1;
	} else {
		du1 = 0.f;
		dv1 = 0.f;
	}

	const Normal n2 = GetTexture2().Bump(hitPoint, sampleDistance);
	const float nn2 = Dot(n2, hitPoint.shadeN);
	float du2, dv2;
	if (nn2 != 0.f) {
		du2 = Dot(n2, u) / nn2;
		dv2 = Dot(n2, v) / nn2;
	} else {
		du2 = 0.f;
		dv2 = 0.f;
	}

	const float t1 = GetTexture1().GetFloatValue(hitPoint);
	const float t2 = GetTexture2().GetFloatValue(hitPoint);

	const float du = du1 * t2 + t1 * du2;
	const float dv = dv1 * t2 + t1 * dv2;

	return Normal(Normalize(Vector(hitPoint.shadeN) + du * u + dv * v));
}

PropertiesUPtr ScaleTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("scale"));
	props->Set(Property("scene.textures." + name + ".texture1")(GetTexture1().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture2")(GetTexture2().GetSDLValue()));

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
