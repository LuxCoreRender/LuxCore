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

#include "slg/textures/math/divide.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Divide texture
//------------------------------------------------------------------------------

float DivideTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const float value2 = tex2->GetFloatValue(hitPoint);
	if (value2 == 0.f)
		return 0.f;
		
	const float value1 = tex1->GetFloatValue(hitPoint);
	return value1 / value2;
}

Spectrum DivideTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Spectrum value2 = tex2->GetSpectrumValue(hitPoint);
	if (value2.Black())
		return Spectrum(0.f);
	
	const Spectrum value1 = tex1->GetSpectrumValue(hitPoint);
	return value1 / value2;
}

float DivideTexture::Y() const {
	const float Y2 = tex2->Y();
	if (Y2 == 0.f)
		return 0.f;
		
	const float Y1 = tex1->Y();
	return Y1 / Y2;
}

float DivideTexture::Filter() const {
	const float filter2 = tex2->Filter();
	if (filter2 == 0.f)
		return 0.f;
		
	const float filter1 = tex1->Filter();
	return filter1 / filter2;
}

Properties DivideTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("divide"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetSDLValue()));

	return props;
}
