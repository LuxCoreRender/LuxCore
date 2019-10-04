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
#include "slg/textures/checkerboard.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

float CheckerBoard2DTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetFloatValue(hitPoint);
	else
		return tex2->GetFloatValue(hitPoint);
}

Spectrum CheckerBoard2DTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetSpectrumValue(hitPoint);
	else
		return tex2->GetSpectrumValue(hitPoint);
}

Properties CheckerBoard2DTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("checkerboard2d"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetSDLValue()));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

float CheckerBoard3DTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p = mapping->Map(hitPoint);
	// The +DEFAULT_EPSILON_STATIC is there as workaround for planes placed exactly on 0.0
	if ((Floor2Int(p.x + DEFAULT_EPSILON_STATIC) + Floor2Int(p.y + DEFAULT_EPSILON_STATIC) + Floor2Int(p.z + DEFAULT_EPSILON_STATIC)) % 2 == 0)
		return tex1->GetFloatValue(hitPoint);
	else
		return tex2->GetFloatValue(hitPoint);
}

Spectrum CheckerBoard3DTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Point p = mapping->Map(hitPoint);
	// The +DEFAULT_EPSILON_STATIC is there as workaround for planes placed exactly on 0.0
	if ((Floor2Int(p.x + DEFAULT_EPSILON_STATIC) + Floor2Int(p.y + DEFAULT_EPSILON_STATIC) + Floor2Int(p.z + DEFAULT_EPSILON_STATIC)) % 2 == 0)
		return tex1->GetSpectrumValue(hitPoint);
	else
		return tex2->GetSpectrumValue(hitPoint);
}

Properties CheckerBoard3DTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("checkerboard3d"));
	props.Set(Property("scene.textures." + name + ".texture1")(tex1->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".texture2")(tex2->GetSDLValue()));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
