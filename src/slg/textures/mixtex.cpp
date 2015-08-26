/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "slg/textures/mix.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

float MixTexture::Y() const {
	return Lerp(amount->Y(), tex1->Y(), tex2->Y());
}

float MixTexture::Filter() const {
	return Lerp(amount->Filter(), tex1->Filter(), tex2->Filter());
}

float MixTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);
	const float value1 = tex1->GetFloatValue(hitPoint);
	const float value2 = tex2->GetFloatValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Spectrum MixTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);
	const Spectrum value1 = tex1->GetSpectrumValue(hitPoint);
	const Spectrum value2 = tex2->GetSpectrumValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Normal MixTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	const float amt = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);
	const Normal tex1ShadeN = tex1->Bump(hitPoint, sampleDistance);
	const Normal tex2ShadeN = tex2->Bump(hitPoint, sampleDistance);

	return Normalize(hitPoint.shadeN +
			Lerp(amt, tex1ShadeN - hitPoint.shadeN, tex2ShadeN - hitPoint.shadeN));
}

Properties MixTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("mix"));
	props.Set(Property("scene.textures." + name + ".amount")(amount->GetName()));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetName()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetName()));

	return props;
}
