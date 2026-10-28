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

#ifndef _SLG_HSVTEX_H
#define	_SLG_HSVTEX_H

#include "slg/textures/texture.h"
#include <functional>

namespace slg {

//------------------------------------------------------------------------------
// Hue saturation value texture
//------------------------------------------------------------------------------

class HsvTexture : public Texture {
public:
	HsvTexture(TextureRef t, TextureRef h, 
			TextureRef s, TextureRef v) : tex(t), hue(h), sat(s), val(v) { }
	virtual ~HsvTexture() { }

	virtual TextureType GetType() const { return HSV_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture().AddReferencedTextures(referencedTexs);
		GetHue().AddReferencedTextures(referencedTexs);
		GetSaturation().AddReferencedTextures(referencedTexs);
		GetValue().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTexture().AddReferencedImageMaps(referencedImgMaps);
		GetHue().AddReferencedImageMaps(referencedImgMaps);
		GetSaturation().AddReferencedImageMaps(referencedImgMaps);
		GetValue().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		for (auto& t : std::array{tex, hue, sat, val})
			updtex(t, oldTex, newTex);
	}

	TextureConstRef GetTexture() const { return tex; }
	TextureConstRef GetHue() const { return hue; }
	TextureConstRef GetSaturation() const { return sat; }
	TextureConstRef GetValue() const { return val; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	luxrays::Spectrum RgbToHsv(const luxrays::Spectrum &rgb) const;
	luxrays::Spectrum HsvToRgb(const luxrays::Spectrum &hsv) const;
	luxrays::Spectrum ApplyTransformation(const luxrays::Spectrum &colorHitpoint,
			const float &hueHitpoint, const float &satHitpoint,
			const float &valHitpoint) const;

	std::reference_wrapper<Texture> tex;
	std::reference_wrapper<Texture> hue;
	std::reference_wrapper<Texture> sat;
	std::reference_wrapper<Texture> val;
};

}

#endif	/* _SLG_HSVTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
