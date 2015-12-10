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

#ifndef _SLG_HSVTEX_H
#define	_SLG_HSVTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Hue saturation value texture
//------------------------------------------------------------------------------

class HsvTexture : public Texture {
public:
	HsvTexture(const Texture *t, const Texture *h, 
			const Texture *s, const Texture *v) : tex(t), hue(h), sat(s), val(v) { }
	virtual ~HsvTexture() { }

	virtual TextureType GetType() const { return HSV_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { 
		return tex->Y(); // TODO
	}
	virtual float Filter() const { 
		return tex->Filter(); //TODO
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
		hue->AddReferencedTextures(referencedTexs);
		sat->AddReferencedTextures(referencedTexs);
		val->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
		hue->AddReferencedImageMaps(referencedImgMaps);
		sat->AddReferencedImageMaps(referencedImgMaps);
		val->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
		if (hue == oldTex)
			hue = newTex;
		if (sat == oldTex)
			sat = newTex;
		if (val == oldTex)
			val = newTex;
	}

	const Texture *GetTexture() const { return tex; }
	const Texture *GetHue() const { return hue; }
	const Texture *GetSaturation() const { return sat; }
	const Texture *GetValue() const { return val; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	void RgbToHsv(const luxrays::Spectrum &rgb, float *result) const;
	luxrays::Spectrum HsvToRgb(const float *hsv) const;

	const Texture *tex;
	const Texture *hue;
	const Texture *sat;
	const Texture *val;
};

}

#endif	/* _SLG_HSVTEX_H */
