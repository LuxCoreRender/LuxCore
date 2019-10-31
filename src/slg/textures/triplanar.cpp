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

#include "luxrays/core/epsilon.h"
#include "slg/textures/triplanar.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Triplanar mapping texture
//------------------------------------------------------------------------------

float TriplanarTexture::GetFloatValue(const HitPoint &hitPoint) const {
    return (texX->GetFloatValue(hitPoint) + texY->GetFloatValue(hitPoint) + texZ->GetFloatValue(hitPoint))/3;
}

Spectrum TriplanarTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
    HitPoint hitPointX = hitPoint;
    HitPoint hitPointY = hitPoint;
    HitPoint hitPointZ = hitPoint;

    hitPointX.uv.u = hitPoint.p.y;
    hitPointX.uv.v = hitPoint.p.z;

    hitPointY.uv.u = hitPoint.p.x;
    hitPointY.uv.v = hitPoint.p.z;

    hitPointZ.uv.u = hitPoint.p.x;
    hitPointZ.uv.v = hitPoint.p.y;

    float weights[3] = {pow(fabsf(hitPoint.shadeN.x),4), pow(fabsf(hitPoint.shadeN.y),4), pow(fabsf(hitPoint.shadeN.z),4)};
    const float sum = weights[0] + weights[1] + weights[2];
    weights[0] = weights[0]/sum;
    weights[1] = weights[1]/sum;
    weights[2] = weights[2]/sum;

    return (texX->GetSpectrumValue(hitPointX)*weights[0] + texY->GetSpectrumValue(hitPointY)*weights[1] + texZ->GetSpectrumValue(hitPointZ)*weights[2]);
}

Properties TriplanarTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("triplanar"));
	props.Set(Property("scene.textures." + name + ".texture1")(texX->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(texY->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".texture3")(texZ->GetSDLValue()));

	return props;
}
