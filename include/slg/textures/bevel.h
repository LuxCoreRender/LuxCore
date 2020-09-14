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

#ifndef _SLG_BEVELTEX_H
#define	_SLG_BEVELTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Bevel texture
//------------------------------------------------------------------------------

class BevelTexture : public Texture {
public:
	BevelTexture(const Texture *t, const float radius);
	virtual ~BevelTexture();

	virtual TextureType GetType() const { return BEVEL_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return 0.f; }
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return luxrays::Spectrum(); }
	virtual float Y() const { return 0.f; }
	virtual float Filter() const { return 0.f; }

    virtual luxrays::Normal Bump(const HitPoint &hitPoint, const float sampleDistance) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		if (tex)
			tex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		if (tex)
			tex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
	}

	const Texture *GetTexture() const { return tex; }
	const float GetRadius() const { return radius; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	const Texture *tex;
	const float radius;
};

}

#endif	/* _SLG_BEVELTEX_H */
