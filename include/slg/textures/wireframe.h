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

#ifndef _SLG_WIREFRAMETEX_H
#define	_SLG_WIREFRAMETEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// WireFrame texture
//------------------------------------------------------------------------------

class WireFrameTexture : public Texture {
public:
	WireFrameTexture(const float w,
			TextureRef borderTx, TextureRef insideTx) :
		width(w), borderTex(borderTx), insideTex(insideTx) { }
	virtual ~WireFrameTexture() { }

	virtual TextureType GetType() const { return WIREFRAME_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		return (GetBorderTex().Y() + GetInsideTex().Y()) * .5f;
	}
	virtual float Filter() const {
		return (GetBorderTex().Filter() + GetInsideTex().Filter()) * .5f;
	}

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetBorderTex().AddReferencedTextures(referencedTexs);
		GetInsideTex().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetBorderTex().AddReferencedImageMaps(referencedImgMaps);
		GetInsideTex().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		updtex(borderTex, oldTex, newTex);
		updtex(insideTex, oldTex, newTex);
	}

	float GetWidth() const { return width; }
	TextureConstRef GetBorderTex() const { return borderTex; }
	TextureConstRef GetInsideTex() const { return insideTex; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	bool Evaluate(const HitPoint &hitPoint) const;

	const float width;
	std::reference_wrapper<Texture> borderTex;
	std::reference_wrapper<Texture> insideTex;
};

}

#endif	/* _SLG_WIREFRAMETEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
