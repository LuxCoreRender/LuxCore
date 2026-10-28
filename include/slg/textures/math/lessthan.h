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

#ifndef _SLG_LESSTHANTEX_H
#define	_SLG_LESSTHANTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Less Than texture
//------------------------------------------------------------------------------

class LessThanTexture : public Texture {
public:
	LessThanTexture(TextureRef t1, TextureRef t2) : tex1(t1), tex2(t2) { }
	virtual ~LessThanTexture() { }

	virtual TextureType GetType() const { return LESS_THAN_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		// Doesn't really make sense here
		return (GetTexture1().Y() < GetTexture2().Y()) ? 1.f : 0.f;
	}
	virtual float Filter() const {
		// Doesn't really make sense here
		return (GetTexture1().Filter() < GetTexture2().Filter()) ? 1.f : 0.f;
	}

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture1().AddReferencedTextures(referencedTexs);
		GetTexture2().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTexture1().AddReferencedImageMaps(referencedImgMaps);
		GetTexture2().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		updtex(tex1, oldTex, newTex);
		updtex(tex2, oldTex, newTex);
	}

	TextureConstRef GetTexture1() const { return tex1; }
	TextureConstRef GetTexture2() const { return tex2; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	std::reference_wrapper<Texture> tex1;
	std::reference_wrapper<Texture> tex2;
};

}

#endif	/* _SLG_LESSTHANTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
