/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

Properties RoundingTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
    Properties props;

    const string name = GetName();
    props.Set(Property("scene.textures." + name + ".type")("divide"));
    props.Set(Property("scene.textures." + name + ".texture")(texture->GetName()));
    props.Set(Property("scene.textures." + name + ".increment")(increment->GetName()));

    return props;
}

// This is where the rounding logic happens
float RoundingTexture::round(float value, float increment) const {
    if(value == increment) {
        return value;
    }

    float ret;
    float lowerBound = 0.f;
    if(value < 0)  {
        while(lowerBound > value) {
            value -= increment;
        }

        float upperBound = lowerBound - increment;
        fabsf(upperBound - value) < fabsf(lowerBound - value) ?
            ret = upperBound : ret = lowerBound;
    } else if(value > 0) {
        while(lowerBound < value) {
            value += increment;
        }

        float upperBound = lowerBound + increment;
        fabsf(upperBound - value) < fabsf(lowerBound - value) ?
            ret = upperBound : ret = lowerBound;
    } else {
        // value == 0
        return 0.f;
    }
    return ret;
}
