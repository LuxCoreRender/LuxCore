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

#ifndef _SLG_BRIGHTCONTRASTTEX_H
#define	_SLG_BRIGHTCONTRASTTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Brightness/Contrast texture
//------------------------------------------------------------------------------

class BrightContrastTexture : public Texture {
public:
	BrightContrastTexture(const Texture *tex, const Texture *brightnessTex, const Texture *contrastTex) :
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

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
		brightnessTex->AddReferencedTextures(referencedTexs);
		contrastTex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
		brightnessTex->AddReferencedImageMaps(referencedImgMaps);
		contrastTex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
		if (brightnessTex == oldTex)
			brightnessTex = newTex;
		if (contrastTex == oldTex)
			contrastTex = newTex;
	}

	const Texture *GetTex() const { return tex; }
	const Texture *GetBrightnessTex() const { return brightnessTex; }
	const Texture *GetContrastTex() const { return contrastTex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	const Texture *tex;
	const Texture *brightnessTex;
	const Texture *contrastTex;
};

}

#endif	/* _SLG_BRIGHTCONTRASTTEX_H */
