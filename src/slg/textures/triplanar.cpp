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

    float weights[3] = {
		Sqr(Sqr(fabsf(hitPoint.shadeN.x))),
		Sqr(Sqr(fabsf(hitPoint.shadeN.y))),
		Sqr(Sqr(fabsf(hitPoint.shadeN.z)))
	};
    
    const float sum = weights[0] + weights[1] + weights[2];
    weights[0] = weights[0] / sum;
    weights[1] = weights[1] / sum;
    weights[2] = weights[2] / sum;

	switch (mapping->GetType()) {
		case GLOBALMAPPING3D:
		case LOCALMAPPING3D: {
			return texX->GetSpectrumValue(hitPoint) * weights[0] +
					texY->GetSpectrumValue(hitPoint) * weights[1] +
					texZ->GetSpectrumValue(hitPoint) * weights[2];
		}
		case UVMAPPING3D: {
			const Point p = mapping->Map(hitPoint);
			const u_int dataIndex = static_cast<const UVMapping3D*>(mapping)->GetDataIndex();

			HitPoint hitPointTmp = hitPoint;
			hitPointTmp.uv[dataIndex].u = p.y;
			hitPointTmp.uv[dataIndex].v = p.z;
			Spectrum result = texX->GetSpectrumValue(hitPointTmp) * weights[0];

			hitPointTmp.uv[dataIndex].u = p.x;
			hitPointTmp.uv[dataIndex].v = p.z;
			result += texY->GetSpectrumValue(hitPointTmp) * weights[1];

			hitPointTmp.uv[dataIndex].u = p.x;
			hitPointTmp.uv[dataIndex].v = p.y;
			result += texZ->GetSpectrumValue(hitPointTmp) * weights[2];

			return result;
		}
		default:
			throw runtime_error("Unknown mapping type in TriplanarTexture: " + ToString(mapping->GetType()));
	}
}

Properties TriplanarTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("triplanar"));
	props.Set(Property("scene.textures." + name + ".texture1")(texX->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(texY->GetSDLValue()));
    props.Set(Property("scene.textures." + name + ".texture3")(texZ->GetSDLValue()));
    props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
