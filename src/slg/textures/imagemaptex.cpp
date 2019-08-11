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

#include "slg/textures/imagemaptex.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

ImageMapTexture::ImageMapTexture(const ImageMap *img, const TextureMapping2D *mp, const float g) :
	imageMap(img), mapping(mp), gain(g) {
}

float ImageMapTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return gain * imageMap->GetFloat(mapping->Map(hitPoint));
}

Spectrum ImageMapTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return gain * imageMap->GetSpectrum(mapping->Map(hitPoint));
}

Normal ImageMapTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	UV dst, du, dv;
	dst = imageMap->GetDuv(mapping->MapDuv(hitPoint, &du, &dv));

	UV duv;
	duv.u = gain * (dst.u * du.u + dst.v * du.v);
	duv.v = gain * (dst.u * dv.u + dst.v * dv.v);
	
	const Vector dpdu = hitPoint.dpdu + duv.u * Vector(hitPoint.shadeN);
	const Vector dpdv = hitPoint.dpdv + duv.v * Vector(hitPoint.shadeN);

	Normal n(Normal(Normalize(Cross(dpdu, dpdv))));

	return ((Dot(n, hitPoint.shadeN) < 0.f) ? -1.f : 1.f) * n;
}

Properties ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache,
		const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("imagemap"));

	const string fileName = useRealFileName ?
		imageMap->GetName() : imgMapCache.GetSequenceFileName(imageMap);
	props.Set(Property("scene.textures." + name + ".file")(fileName));
	props.Set(Property("scene.textures." + name + ".gain")(gain));
	props.Set(imageMap->ToProperties("scene.textures." + name, false));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
