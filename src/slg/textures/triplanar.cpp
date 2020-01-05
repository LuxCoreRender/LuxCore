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
    return GetSpectrumValue(hitPoint).Y();
}

Spectrum TriplanarTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	Normal localShadeN;
	const Point localPoint = mapping->Map(hitPoint, &localShadeN);
	
    float weights[3] = {
		Sqr(Sqr(fabsf(localShadeN.x))),
		Sqr(Sqr(fabsf(localShadeN.y))),
		Sqr(Sqr(fabsf(localShadeN.z)))
	};
    
    const float sum = weights[0] + weights[1] + weights[2];
    weights[0] = weights[0] / sum;
    weights[1] = weights[1] / sum;
    weights[2] = weights[2] / sum;

	HitPoint hitPointTmp = hitPoint;
	hitPointTmp.uv[uvIndex].u = localPoint.y;
	hitPointTmp.uv[uvIndex].v = localPoint.z;
	Spectrum result = texX->GetSpectrumValue(hitPointTmp) * weights[0];

	hitPointTmp.uv[uvIndex].u = localPoint.x;
	hitPointTmp.uv[uvIndex].v = localPoint.z;
	result += texY->GetSpectrumValue(hitPointTmp) * weights[1];

	hitPointTmp.uv[uvIndex].u = localPoint.x;
	hitPointTmp.uv[uvIndex].v = localPoint.y;
	result += texZ->GetSpectrumValue(hitPointTmp) * weights[2];

	return result;
}

Properties TriplanarTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("triplanar"));
	props.Set(Property("scene.textures." + name + ".texture1")(texX->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(texY->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".texture3")(texZ->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".uvindex")(uvIndex));
    props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
