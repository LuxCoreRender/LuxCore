/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include "slg/usings.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Bilerp texture
//------------------------------------------------------------------------------

float BilerpTexture::GetFloatValue(const HitPoint &hitPoint) const
{
	UV uv = hitPoint.GetUV(0);
	uv.u -= Floor2Int(uv.u);
	uv.v -= Floor2Int(uv.v);
	return Lerp(
		uv.u,
		Lerp( uv.v, GetTexture00().GetFloatValue(hitPoint), GetTexture01().GetFloatValue(hitPoint)),
		Lerp(uv.v, GetTexture10().GetFloatValue(hitPoint), GetTexture11().GetFloatValue(hitPoint))
	);
}

Spectrum BilerpTexture::GetSpectrumValue(const HitPoint &hitPoint) const
{
	UV uv = hitPoint.GetUV(0);
	uv.u -= Floor2Int(uv.u);
	uv.v -= Floor2Int(uv.v);
	return Lerp(
		uv.u,
		Lerp(uv.v, GetTexture00().GetSpectrumValue(hitPoint), GetTexture01().GetSpectrumValue(hitPoint)),
		Lerp(uv.v, GetTexture10().GetSpectrumValue(hitPoint), GetTexture11().GetSpectrumValue(hitPoint))
	);
}

PropertiesUPtr BilerpTexture::ToProperties(
	const ImageMapCache &imgMapCache,
	const bool useRealFileName
) const
{
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("bilerp"));
	props->Set(Property("scene.textures." + name + ".texture00")(GetTexture00().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture01")(GetTexture01().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture10")(GetTexture10().GetSDLValue()));
	props->Set(Property("scene.textures." + name + ".texture11")(GetTexture11().GetSDLValue()));

	return props;
}

float BilerpTexture::Y() const
{
	return (GetTexture00().Y() + GetTexture01().Y() + GetTexture10().Y() + GetTexture11().Y()) * .25f;
}

float BilerpTexture::Filter() const
{
	return (GetTexture00().Filter() + GetTexture01().Filter() + GetTexture10().Filter() + GetTexture11().Filter()) * .25f;
}

void BilerpTexture::AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const
{
	Texture::AddReferencedTextures(referencedTexs);

	GetTexture00().AddReferencedTextures(referencedTexs);
	GetTexture01().AddReferencedTextures(referencedTexs);
	GetTexture10().AddReferencedTextures(referencedTexs);
	GetTexture11().AddReferencedTextures(referencedTexs);
}
void BilerpTexture::AddReferencedImageMaps(
	std::unordered_set<const ImageMap *> &referencedImgMaps
) const
{
	GetTexture00().AddReferencedImageMaps(referencedImgMaps);
	GetTexture01().AddReferencedImageMaps(referencedImgMaps);
	GetTexture10().AddReferencedImageMaps(referencedImgMaps);
	GetTexture11().AddReferencedImageMaps(referencedImgMaps);
}

void BilerpTexture::UpdateTextureReferences(
	TextureConstRef oldTex, TextureRef newTex
)
{
	if (&GetTexture00() == &oldTex)
		t00 = newTex;
	if (&GetTexture01() == &oldTex)
		t01 = newTex;
	if (&GetTexture10() == &oldTex)
		t10 = newTex;
	if (&GetTexture11() == &oldTex)
		t11 = newTex;
}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
