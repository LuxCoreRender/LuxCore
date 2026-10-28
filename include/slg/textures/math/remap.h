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

#ifndef _SLG_REMAPTEX_H
#define	_SLG_REMAPTEX_H

#include "slg/imagemap/imagemap.h"
#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Remap texture
//------------------------------------------------------------------------------

class RemapTexture : public Texture {
public:
	RemapTexture(TextureRef value, TextureRef sourceMin,
			TextureRef sourceMax, TextureRef targetMin,
			TextureRef targetMax)
		: valueTex(value), sourceMinTex(sourceMin), sourceMaxTex(sourceMax),
		  targetMinTex(targetMin), targetMaxTex(targetMax) { }
	virtual ~RemapTexture() { }

	virtual TextureType GetType() const { return REMAP_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetValueTex().AddReferencedTextures(referencedTexs);
		GetSourceMinTex().AddReferencedTextures(referencedTexs);
		GetSourceMaxTex().AddReferencedTextures(referencedTexs);
		GetTargetMinTex().AddReferencedTextures(referencedTexs);
		GetTargetMaxTex().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetValueTex().AddReferencedImageMaps(referencedImgMaps);
		GetSourceMinTex().AddReferencedImageMaps(referencedImgMaps);
		GetSourceMaxTex().AddReferencedImageMaps(referencedImgMaps);
		GetTargetMinTex().AddReferencedImageMaps(referencedImgMaps);
		GetTargetMaxTex().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		updtex(valueTex, oldTex, newTex);
		updtex(sourceMinTex, oldTex, newTex);
		updtex(sourceMaxTex, oldTex, newTex);
		updtex(targetMinTex, oldTex, newTex);
		updtex(targetMaxTex, oldTex, newTex);
	}

	TextureConstRef GetValueTex() const { return valueTex; }
	TextureConstRef GetSourceMinTex() const { return sourceMinTex; }
	TextureConstRef GetSourceMaxTex() const { return sourceMaxTex; }
	TextureConstRef GetTargetMinTex() const { return targetMinTex; }
	TextureConstRef GetTargetMaxTex() const { return targetMaxTex; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache,
	                                         const bool useRealFileName) const;

private:
	std::reference_wrapper<Texture> valueTex;
	std::reference_wrapper<Texture> sourceMinTex;
	std::reference_wrapper<Texture> sourceMaxTex;
	std::reference_wrapper<Texture> targetMinTex;
	std::reference_wrapper<Texture> targetMaxTex;

	static float ClampedRemap(float value,
	                          const float sourceMin, const float sourceMax,
	                          const float targetMin, const float targetMax);

	static luxrays::Spectrum ClampedRemap(luxrays::Spectrum value,
	                                      const float sourceMin, const float sourceMax,
	                                      const float targetMin, const float targetMax);
};

}

#endif	/* _SLG_REMAPTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
