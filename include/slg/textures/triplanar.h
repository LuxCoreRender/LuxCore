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

#ifndef _SLG_TRIPLANAR_H
#define	_SLG_TRIPLANAR_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Triplanar Mapping texture
//------------------------------------------------------------------------------

class TriplanarTexture : public Texture {
public:
	TriplanarTexture(TextureMapping3DUPtr mp, TextureRef t1, TextureRef t2, 
    TextureRef t3, const bool uvlessBumpMap) :
    mapping(std::move(mp)), texX(t1), texY(t2), texZ(t3),
	enableUVlessBumpMap(uvlessBumpMap) {}

	virtual ~TriplanarTexture() {}

	virtual TextureType GetType() const {
		return TRIPLANAR_TEX;
	}
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual float Y() const {
		return (GetTexture1().Y() + GetTexture2().Y() + GetTexture3().Y()) * (1.f / 3.f);
	}

	virtual float Filter() const {
		return (GetTexture1().Filter() + GetTexture2().Filter() + GetTexture3().Filter()) * (1.f / 3.f);
	}

	virtual luxrays::Normal Bump(const HitPoint &hitPoint, const float sampleDistance) const;

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture1().AddReferencedTextures(referencedTexs);
		GetTexture2().AddReferencedTextures(referencedTexs);
		GetTexture3().AddReferencedTextures(referencedTexs);
	}

	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTexture1().AddReferencedImageMaps(referencedImgMaps);
		GetTexture2().AddReferencedImageMaps(referencedImgMaps);
		GetTexture3().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		updtex(texX, oldTex, newTex);
		updtex(texY, oldTex, newTex);
		updtex(texZ, oldTex, newTex);
	}

	TextureMapping3DConstRef GetTextureMapping() const { return *mapping; }
	TextureConstRef GetTexture1() const { return texX; }
	TextureConstRef GetTexture2() const { return texY; }
    TextureConstRef GetTexture3() const { return texZ; }
	const bool IsUVlessBumpMap() const { return enableUVlessBumpMap; }


	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	TextureMapping3DUPtr mapping;
	std::reference_wrapper<Texture> texX;
	std::reference_wrapper<Texture> texY;
    std::reference_wrapper<Texture> texZ;

	const bool enableUVlessBumpMap;
};

}

#endif	/* _SLG_TRIPLANAR_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
