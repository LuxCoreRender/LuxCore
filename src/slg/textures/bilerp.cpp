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

#include "slg/textures/bilerp.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Bilerp texture
//------------------------------------------------------------------------------

float BilerpTexture::GetFloatValue(const HitPoint &hitPoint) const
{
	UV uv = hitPoint.uv;
	uv.u -= Floor2Int(uv.u);
	uv.v -= Floor2Int(uv.v);
	return Lerp(uv.u, Lerp(uv.v, t00->GetFloatValue(hitPoint), t01->GetFloatValue(hitPoint)),
		Lerp(uv.v, t10->GetFloatValue(hitPoint), t11->GetFloatValue(hitPoint)));
}

Spectrum BilerpTexture::GetSpectrumValue(const HitPoint &hitPoint) const
{
	UV uv = hitPoint.uv;
	uv.u -= Floor2Int(uv.u);
	uv.v -= Floor2Int(uv.v);
	return Lerp(uv.u, Lerp(uv.v, t00->GetSpectrumValue(hitPoint), t01->GetSpectrumValue(hitPoint)),
		Lerp(uv.v, t10->GetSpectrumValue(hitPoint), t11->GetSpectrumValue(hitPoint)));
}

Properties BilerpTexture::ToProperties(const ImageMapCache &imgMapCache) const
{
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("bilerp"));
	props.Set(Property("scene.textures." + name + ".texture00")(t00->GetName()));
	props.Set(Property("scene.textures." + name + ".texture01")(t01->GetName()));
	props.Set(Property("scene.textures." + name + ".texture10")(t10->GetName()));
	props.Set(Property("scene.textures." + name + ".texture11")(t11->GetName()));

	return props;
}

float BilerpTexture::Y() const
{
	return (t00->Y() + t01->Y() + t10->Y() + t11->Y()) * .25f;
}

float BilerpTexture::Filter() const
{
	return (t00->Filter() + t01->Filter() + t10->Filter() + t11->Filter()) * .25f;
}

void BilerpTexture::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const
{
	Texture::AddReferencedTextures(referencedTexs);

	t00->AddReferencedTextures(referencedTexs);
	t01->AddReferencedTextures(referencedTexs);
	t10->AddReferencedTextures(referencedTexs);
	t11->AddReferencedTextures(referencedTexs);
}
void BilerpTexture::AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const
{
	t00->AddReferencedImageMaps(referencedImgMaps);
	t01->AddReferencedImageMaps(referencedImgMaps);
	t10->AddReferencedImageMaps(referencedImgMaps);
	t11->AddReferencedImageMaps(referencedImgMaps);
}

void BilerpTexture::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex)
{
	if (t00 == oldTex)
		t00 = newTex;
	if (t01 == oldTex)
		t01 = newTex;
	if (t10 == oldTex)
		t10 = newTex;
	if (t11 == oldTex)
		t11 = newTex;
}

