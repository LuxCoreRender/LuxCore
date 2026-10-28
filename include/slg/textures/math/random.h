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

#ifndef _SLG_RANDOMTEX_H
#define	_SLG_RANDOMTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Random texture
//------------------------------------------------------------------------------

class RandomTexture : public Texture {
public:
	RandomTexture(TextureRef t, const u_int o) : tex(t), seedOffset(o) { }
	virtual ~RandomTexture() { }

	virtual TextureType GetType() const { return RANDOM_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return luxrays::Spectrum(.5f).Y(); }
	virtual float Filter() const { return .5f; }

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTexture().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		updtex(tex, oldTex, newTex);
	}

	TextureConstRef GetTexture() const { return tex; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	std::reference_wrapper<Texture> tex;

	const u_int seedOffset;
};

}

#endif	/* _SLG_RANDOMTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
