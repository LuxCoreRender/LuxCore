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

#include "slg/textures/brightcontrast.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Brightness/Contrast texture
//------------------------------------------------------------------------------

float BrightContrastTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const float value = tex->GetSpectrumValue(hitPoint).Y();
	const float contrast = contrastTex->GetFloatValue(hitPoint);
	const float brightness = brightnessTex->GetFloatValue(hitPoint);

	const float a = 1.f + contrast;
	const float b = brightness - contrast * 0.5f;

	return Clamp(value * a + b, 0.f, INFINITY);
}

Spectrum BrightContrastTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float contrast = contrastTex->GetFloatValue(hitPoint);
	const float brightness = brightnessTex->GetFloatValue(hitPoint);

	const float a = 1.f + contrast;
	const float b = brightness - contrast * 0.5f;

	return (tex->GetSpectrumValue(hitPoint) * a + Spectrum(b)).Clamp();
}

Properties BrightContrastTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("brightcontrast"));
	props.Set(Property("scene.textures." + name + ".texture")(tex->GetName()));
	props.Set(Property("scene.textures." + name + ".brightness")(brightnessTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".contrast")(contrastTex->GetSDLValue()));

	return props;
}
