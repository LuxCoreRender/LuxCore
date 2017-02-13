/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_BANDTEX_H
#define	_SLG_BANDTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

class BandTexture : public Texture {
public:
	typedef enum {
		NONE,
		LINEAR,
		CUBIC
	} InterpolationType;

	BandTexture(const InterpolationType interp,
			const Texture *amnt,
			const std::vector<float> &os,
			const std::vector<luxrays::Spectrum> &vs) :
			interpType(interp), amount(amnt), offsets(os), values(vs) { }
	virtual ~BandTexture() { }

	virtual TextureType GetType() const { return BAND_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		amount->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		amount->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (amount == oldTex)
			amount = newTex;
	}

	InterpolationType GetInterpolationType() const { return interpType; }
	const Texture *GetAmountTexture() const { return amount; }
	const std::vector<float> &GetOffsets() const { return offsets; }
	const std::vector<luxrays::Spectrum> &GetValues() const { return values; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	static InterpolationType String2InterpolationType(const std::string &type);
	static std::string InterpolationType2String(const InterpolationType type);

private:
	const InterpolationType interpType;

	const Texture *amount;
	const std::vector<float> offsets;
	const std::vector<luxrays::Spectrum> values; 
};


}

#endif	/* _SLG_BANDTEX_H */
