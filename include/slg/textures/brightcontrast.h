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

#ifndef _SLG_BRIGHTCONTRASTTEX_H
#define	_SLG_BRIGHTCONTRASTTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Brightness/Contrast texture
//------------------------------------------------------------------------------

class BrightContrastTexture : public Texture {
public:
	BrightContrastTexture(TextureRef tex, TextureRef brightnessTex, TextureRef contrastTex) :
		tex(tex), brightnessTex(brightnessTex), contrastTex(contrastTex) { }
	virtual ~BrightContrastTexture() { }

	virtual TextureType GetType() const { return BRIGHT_CONTRAST_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		return 0.f;  // TODO
	}
	virtual float Filter() const {
		return 0.f;  // TODO
	}

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTex().AddReferencedTextures(referencedTexs);
		GetBrightnessTex().AddReferencedTextures(referencedTexs);
		GetContrastTex().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTex().AddReferencedImageMaps(referencedImgMaps);
		GetBrightnessTex().AddReferencedImageMaps(referencedImgMaps);
		GetContrastTex().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		if (&tex.get() == &oldTex)
			tex = newTex;
		if (&brightnessTex.get() == &oldTex)
			brightnessTex = newTex;
		if (&contrastTex.get() == &oldTex)
			contrastTex = newTex;
	}

	TextureConstRef GetTex() const { return tex; }
	TextureConstRef GetBrightnessTex() const { return brightnessTex; }
	TextureConstRef GetContrastTex() const { return contrastTex; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	std::reference_wrapper<Texture> tex;
	std::reference_wrapper<Texture> brightnessTex;
	std::reference_wrapper<Texture> contrastTex;
};

}

#endif	/* _SLG_BRIGHTCONTRASTTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
