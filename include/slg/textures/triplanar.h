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

#ifndef _SLG_TRIPLANAR_H
#define	_SLG_TRIPLANAR_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Triplanar Mapping texture
//------------------------------------------------------------------------------

class TriplanarTexture : public Texture {
public:
	TriplanarTexture(const Texture *t1, const Texture *t2, 
    const Texture *t3) :
    texX(t1), texY(t2), texZ(t3) {}

	virtual ~TriplanarTexture() {}

	virtual TextureType GetType() const { return TRIPLANAR_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { 
        return (texX->Y() + texY->Filter() + texZ->Filter()) * 1.f/3.f;
        }
	virtual float Filter() const { 
        return (texX->Filter() + texY->Filter() + texZ->Filter()) * 1.f/3.f;
        }

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		texX->AddReferencedTextures(referencedTexs);
		texY->AddReferencedTextures(referencedTexs);
        texZ->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		texX->AddReferencedImageMaps(referencedImgMaps);
		texY->AddReferencedImageMaps(referencedImgMaps);
        texZ->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (texX == oldTex)
			texX = newTex;
		if (texY == oldTex)
			texY = newTex;
        if (texZ == oldTex)
			texZ = newTex;
	}

	const Texture *GetTexture1() const { return texX; }
	const Texture *GetTexture2() const { return texY; }
    const Texture *GetTexture3() const { return texZ; }


	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	const TextureMapping3D *mapping;
	const Texture *texX;
	const Texture *texY;
    const Texture *texZ;
};


}

#endif	/* _SLG_TRIPLANAR_H */
