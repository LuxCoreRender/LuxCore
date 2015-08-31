/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include <boost/format.hpp>

#include "slg/textures/imagemaptex.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

ImageMapTexture::ImageMapTexture(const ImageMap * im, const TextureMapping2D *mp, const float g) :
	imgMap(im), mapping(mp), gain(g) {
	imageY = gain * imgMap->GetSpectrumMeanY();
	imageFilter = gain * imgMap->GetSpectrumMean();
}

float ImageMapTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetFloat(mapping->Map(hitPoint));
}

Spectrum ImageMapTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetSpectrum(mapping->Map(hitPoint));
}

Normal ImageMapTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	UV dst, du, dv;
	dst = imgMap->GetDuv(mapping->MapDuv(hitPoint, &du, &dv));
	UV duv;
	duv.u = gain * (dst.u * du.u + dst.v * du.v);
	duv.v = gain * (dst.u * dv.u + dst.v * dv.v);
	Normal n(Normal(Normalize(Cross(hitPoint.dpdu + duv.u * Vector(hitPoint.shadeN), hitPoint.dpdv + duv.v * Vector(hitPoint.shadeN)))));
	if (Dot(n, hitPoint.shadeN) < 0.f)
		return -n;
	else
		return n;
}

Properties ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("imagemap"));
	props.Set(Property("scene.textures." + name + ".file")("imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imgMap)).str() +
		"." + imgMap->GetFileExtension()));
	props.Set(Property("scene.textures." + name + ".gamma")(1.f));
	props.Set(Property("scene.textures." + name + ".gain")(gain));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
