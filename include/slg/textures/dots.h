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

#ifndef _SLG_DOTSTEX_H
#define	_SLG_DOTSTEX_H

#include "slg/imagemap/imagemap.h"
#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

class DotsTexture : public Texture {
public:
	DotsTexture(TextureMapping2DUPtr&& mp, TextureRef insideTx, TextureRef outsideTx) :
		mapping(std::move(mp)), insideTex(insideTx), outsideTex(outsideTx) { }

	virtual TextureType GetType() const { return DOTS; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		return (GetInsideTex().Y() + GetOutsideTex().Y()) * .5f;
	}
	virtual float Filter() const {
		return (GetInsideTex().Filter() + GetOutsideTex().Filter()) * .5f;
	}

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetInsideTex().AddReferencedTextures(referencedTexs);
		GetOutsideTex().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetInsideTex().AddReferencedImageMaps(referencedImgMaps);
		GetOutsideTex().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		if (&GetInsideTex() == &oldTex)
			insideTex = newTex;
		if (&GetOutsideTex() == &oldTex)
			outsideTex = newTex;
	}

	TextureMapping2DConstRef GetTextureMapping() const { return *mapping; }
	TextureConstRef GetInsideTex() const { return insideTex; }
	TextureConstRef GetOutsideTex() const { return outsideTex; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	bool Evaluate(const HitPoint &hitPoint) const;

	TextureMapping2DUPtr mapping;
	std::reference_wrapper<Texture> insideTex;
	std::reference_wrapper<Texture> outsideTex;
};

}

#endif	/* _SLG_DOTSTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
