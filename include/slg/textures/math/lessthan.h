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

#ifndef _SLG_LESSTHANTEX_H
#define	_SLG_LESSTHANTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Less Than texture
//------------------------------------------------------------------------------

class LessThanTexture : public Texture {
public:
	LessThanTexture(const Texture *t1, const Texture *t2) : tex1(t1), tex2(t2) { }
	virtual ~LessThanTexture() { }

	virtual TextureType GetType() const { return ADD_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { 
		// Doesn't really make sense here
		return (tex1->Y() < tex2->Y()) ? 1.f : 0.f;
	}
	virtual float Filter() const {
		// Doesn't really make sense here
		return (tex1->Filter() < tex2->Filter()) ? 1.f : 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
	}

	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	const Texture *tex1;
	const Texture *tex2;
};

}

#endif	/* _SLG_LESSTHANTEX_H */
