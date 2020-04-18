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
		Sqr(Sqr(localShadeN.x)),
		Sqr(Sqr(localShadeN.y)),
		Sqr(Sqr(localShadeN.z))
	};
    
    const float sum = weights[0] + weights[1] + weights[2];
    weights[0] = weights[0] / sum;
    weights[1] = weights[1] / sum;
    weights[2] = weights[2] / sum;

	HitPoint hitPointTmp = hitPoint;
	hitPointTmp.defaultUV.u = localPoint.y;
	hitPointTmp.defaultUV.v = localPoint.z;
	Spectrum result = texX->GetSpectrumValue(hitPointTmp) * weights[0];

	hitPointTmp.defaultUV.u = localPoint.x;
	hitPointTmp.defaultUV.v = localPoint.z;
	result += texY->GetSpectrumValue(hitPointTmp) * weights[1];

	hitPointTmp.defaultUV.u = localPoint.x;
	hitPointTmp.defaultUV.v = localPoint.y;
	result += texZ->GetSpectrumValue(hitPointTmp) * weights[2];

	return result;
}

Normal TriplanarTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	if (enableUVlessBumpMap) {
		// Calculate bump map value at intersection point
		const float base = GetFloatValue(hitPoint);

		// Compute offset positions and evaluate displacement texture
		const Point origP = hitPoint.p;

		HitPoint hitPointTmp = hitPoint;
		Normal dhdx;

		// Note: I should update not only hitPointTmp.p but also hitPointTmp.shadeN
		// however I can't because I don't know dndv/dndu so this is nearly an hack.

		hitPointTmp.p.x = origP.x + sampleDistance;
		hitPointTmp.p.y = origP.y;
		hitPointTmp.p.z = origP.z;
		const float offsetX = GetFloatValue(hitPointTmp);
		dhdx.x = (offsetX - base) / sampleDistance;

		hitPointTmp.p.x = origP.x;
		hitPointTmp.p.y = origP.y + sampleDistance;
		hitPointTmp.p.z = origP.z;
		const float offsetY = GetFloatValue(hitPointTmp);
		dhdx.y = (offsetY - base) / sampleDistance;

		hitPointTmp.p.x = origP.x;
		hitPointTmp.p.y = origP.y;
		hitPointTmp.p.z = origP.z + sampleDistance;
		const float offsetZ = GetFloatValue(hitPointTmp);
		dhdx.z = (offsetZ - base) / sampleDistance;

		Normal newShadeN = Normalize(hitPoint.shadeN - dhdx);
		newShadeN *= (Dot(hitPoint.shadeN, newShadeN) < 0.f) ? -1.f : 1.f;

		return newShadeN;
	} else
		return Texture::Bump(hitPoint, sampleDistance);
}

Properties TriplanarTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("triplanar"));
	props.Set(Property("scene.textures." + name + ".texture1")(texX->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(texY->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".texture3")(texZ->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".uvlessbumpmap.enable")(enableUVlessBumpMap));
    props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
