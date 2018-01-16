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

#ifndef _SLG_COLORDEPTHTEX_H
#define	_SLG_COLORDEPTHTEX_H

#include "luxrays/utils/utils.h"
#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Color at depth texture
//------------------------------------------------------------------------------

class ColorDepthTexture : public Texture {
public:
	ColorDepthTexture(const float depth, const Texture *t) :
		d(-luxrays::Max(1e-3f, depth)), kt(t) { }
	virtual ~ColorDepthTexture() { }

	virtual TextureType GetType() const { return COLORDEPTH_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;
	
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		kt->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		kt->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (kt == oldTex)
			kt = newTex;
	}

	float GetD() const { return d; }
	const Texture *GetKt() const { return kt; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	float d;
	const Texture *kt;
};

}

#endif	/* _SLG_COLORDEPTHTEX_H */
