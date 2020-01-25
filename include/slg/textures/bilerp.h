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

#ifndef _SLG_BILERPTEX_H
#define	_SLG_BILERPTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Bilerp texture
//------------------------------------------------------------------------------

class BilerpTexture : public Texture {
public:
	BilerpTexture(const Texture *tex00, const Texture *tex01, const Texture *tex10, const Texture *tex11) : t00(tex00), t01(tex01), t10(tex10), t11(tex11) { }
	virtual ~BilerpTexture() { }

	virtual TextureType GetType() const { return BILERP_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const;

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	const Texture *GetTexture00() const { return t00; }
	const Texture *GetTexture01() const { return t01; }
	const Texture *GetTexture10() const { return t10; }
	const Texture *GetTexture11() const { return t11; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	const Texture *t00, *t01, *t10, *t11;
};

}

#endif	/* _SLG_BILERPTEX_H */

