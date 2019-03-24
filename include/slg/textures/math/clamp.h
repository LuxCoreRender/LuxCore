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

#ifndef _SLG_CLAMPTEX_H
#define	_SLG_CLAMPTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Clamp texture
//------------------------------------------------------------------------------

class ClampTexture : public Texture {
public:
	ClampTexture(const Texture *t, const float minv, const float maxv) : tex(t),
			minVal(minv), maxVal(maxv) { }
	virtual ~ClampTexture() { }

	virtual TextureType GetType() const { return CLAMP_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return luxrays::Clamp(tex->Y(), minVal, maxVal); } // This can be not correct
	virtual float Filter() const { return luxrays::Clamp(tex->Filter(), minVal, maxVal); } // This can be not correct

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
	}

	const Texture *GetTexture() const { return tex; }
	float GetMinVal() const { return minVal; }
	float GetMaxVal() const { return maxVal; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	const Texture *tex;
	const float minVal, maxVal;
};

}

#endif	/* _SLG_CLAMPTEX_H */
