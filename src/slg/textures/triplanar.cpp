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
    return (texX1->GetFloatValue(hitPoint) + texY1->GetFloatValue(hitPoint) + texZ1->GetFloatValue(hitPoint))/3;
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

    // SLG_LOG("GetSpectrum: Hitpoint " << hitPoint.geometryN << " " << hitPoint.localToWorld);
    float weights[3] = {pow(fabsf(hitPoint.geometryN.x),4), pow(fabsf(hitPoint.geometryN.y),4), pow(fabsf(hitPoint.geometryN.z),4)};
    const float sum = weights[0] + weights[1] + weights[2];
    weights[0] = weights[0]/sum;
    weights[1] = weights[1]/sum;
    weights[2] = weights[2]/sum;
    return (texX1->GetSpectrumValue(hitPointX)*weights[0] + texY1->GetSpectrumValue(hitPointY)*weights[1] + texZ1->GetSpectrumValue(hitPointZ)*weights[2]);
}

Properties TriplanarTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("triplanarmapping"));
	props.Set(Property("scene.textures." + name + ".texture1")(texX1->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(texY1->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".texture3")(texZ1->GetSDLValue()));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
