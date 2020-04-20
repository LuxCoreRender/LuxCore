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

#include "slg/textures/math/rounding.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Rounding texture
//------------------------------------------------------------------------------

float RoundingTexture::GetFloatValue(const HitPoint &hitPoint) const {
    return round(texture->GetFloatValue(hitPoint), increment->GetFloatValue(hitPoint));
}

Spectrum RoundingTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
    return Spectrum(GetFloatValue(hitPoint));
}

Properties RoundingTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
    Properties props;

    const string name = GetName();
    props.Set(Property("scene.textures." + name + ".type")("rounding"));
    props.Set(Property("scene.textures." + name + ".texture")(texture->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".increment")(increment->GetSDLValue()));

    return props;
}

// This is where the rounding logic happens
float RoundingTexture::round(float value, float increment) const {
    if(value == increment || increment == 0) {
        return value;
    } else {
        const float innerBound = increment * static_cast<int> (value / increment);
        const float outerBound = (value > 0 ? innerBound + increment : innerBound - increment);
        return (fabsf(innerBound - value) < fabsf(outerBound - value) ?
                innerBound : outerBound);
    }
}
