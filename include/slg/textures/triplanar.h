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
	TriplanarTexture(const TextureMapping2D *mp, const Texture *t1, const Texture *t2, 
    const Texture *t3, const Texture *t4, const Texture *t5, const Texture *t6) :
    mapping(mp), texX1(t1), texX2(t4), texY1(t2), texY2(t5), texZ1(t3), texZ2(t6) {}

	virtual ~TriplanarTexture() { delete mapping; }

	virtual TextureType GetType() const { return TRIPLANAR_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { 
        return (texX1->Y() + texX2->Filter() + texY1->Filter() + texY2->Filter() + texZ1->Filter() + texZ2->Filter()) * 1.f/6.f;
        }
	virtual float Filter() const { 
        return (texX1->Filter() + texX2->Filter() + texY1->Filter() + texY2->Filter() + texZ1->Filter() + texZ2->Filter()) * 1.f/6.f;
        }

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		texX1->AddReferencedTextures(referencedTexs);
		texX2->AddReferencedTextures(referencedTexs);
        texY1->AddReferencedTextures(referencedTexs);
		texY2->AddReferencedTextures(referencedTexs);
        texZ1->AddReferencedTextures(referencedTexs);
		texZ2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		texX1->AddReferencedImageMaps(referencedImgMaps);
		texX2->AddReferencedImageMaps(referencedImgMaps);
        texY1->AddReferencedImageMaps(referencedImgMaps);
		texY2->AddReferencedImageMaps(referencedImgMaps);
        texZ1->AddReferencedImageMaps(referencedImgMaps);
		texZ2->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (texX1 == oldTex)
			texX1 = newTex;
		if (texX2 == oldTex)
			texX2 = newTex;
        if (texY1 == oldTex)
			texY1 = newTex;
        if (texY2 == oldTex)
			texY2 = newTex;
        if (texZ1 == oldTex)
			texZ1 = newTex;
        if (texZ2 == oldTex)
			texZ2 = newTex;    
	}

	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const Texture *GetTexture1() const { return texX1; }
	const Texture *GetTexture2() const { return texX2; }
    const Texture *GetTexture3() const { return texY1; }
    const Texture *GetTexture4() const { return texY2; }
    const Texture *GetTexture5() const { return texZ1; }
    const Texture *GetTexture6() const { return texZ2; }


	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	const TextureMapping2D *mapping;
	const Texture *texX1;
	const Texture *texX2;
    const Texture *texY1;
	const Texture *texY2;
    const Texture *texZ1;
	const Texture *texZ2;
};


}

#endif	/* _SLG_TRIPLANAR_H */
