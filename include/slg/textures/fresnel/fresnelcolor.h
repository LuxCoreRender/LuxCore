/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_FRESNELCOLORTEX_H
#define	_SLG_FRESNELCOLORTEX_H

#include "slg/textures/fresnel/fresneltexture.h"

namespace slg {

//------------------------------------------------------------------------------
// Fresnel color texture
//------------------------------------------------------------------------------

class FresnelColorTexture : public FresnelTexture {
public:
	FresnelColorTexture(const Texture *c) : kr(c) { }
	virtual ~FresnelColorTexture() { }

	virtual TextureType GetType() const { return FRESNELCOLOR_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint, const float cosi) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		kr->AddReferencedTextures(referencedTexs);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (kr == oldTex)
			kr = newTex;
	}

	const Texture *GetKr() const { return kr; };

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *kr;
};

}

#endif	/* _SLG_FRESNELCOLORTEX_H */
