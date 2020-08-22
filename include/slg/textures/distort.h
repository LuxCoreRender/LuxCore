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

#ifndef _SLG_DISTORTTEXTURE_H
#define	_SLG_DISTORTTEXTURE_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Distort texture
//------------------------------------------------------------------------------

class DistortTexture : public Texture {
public:
	DistortTexture(const Texture *tex, const Texture *offset, const float strength) :
		tex(tex), offset(offset), strength(strength) { }
	virtual ~DistortTexture() { }

	virtual TextureType GetType() const { return DISTORT_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		return tex->Y();
	}
	virtual float Filter() const {
		return tex->Filter();
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
		offset->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
		offset->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
		if (offset == oldTex)
			offset = newTex;
	}

	const Texture *GetTex() const { return tex; }
	const Texture *GetOffset() const { return offset; }
	const float GetStrength() const { return strength; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	void GetTmpHitPoint(const HitPoint &hitPoint, HitPoint &tmpHitPoint) const;

	const Texture *tex;
	const Texture *offset;
	const float strength;
};

}

#endif	/* _SLG_DISTORTTEXTURE_H */
