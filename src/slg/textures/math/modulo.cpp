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

#include "slg/textures/math/modulo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Rounding texture
//------------------------------------------------------------------------------

float ModuloTexture::GetFloatValue(const HitPoint &hitPoint) const {
    const float modulus = modulo->GetFloatValue(hitPoint);
    if(modulus == 0) {
        // As the modulus value approaches zero, the texture will grow
        // darker and darker. I chose zero as the default return value
        // to match that trend.
        return 0.f;
    }

    return fmod(texture->GetFloatValue(hitPoint), modulus);
}

Spectrum ModuloTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
    return Spectrum(GetFloatValue(hitPoint));
}

Properties ModuloTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
    Properties props;

    const string name = GetName();
    props.Set(Property("scene.textures." + name + ".type")("modulo"));
    props.Set(Property("scene.textures." + name + ".texture")(texture->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".modulo")(modulo->GetSDLValue()));

    return props;
}
