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

#ifndef _SLG_REMAPTEX_H
#define	_SLG_REMAPTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Remap texture
//------------------------------------------------------------------------------

class RemapTexture : public Texture {
public:
	RemapTexture(const Texture *value, const Texture *sourceMin,
			const Texture *sourceMax, const Texture *targetMin,
			const Texture *targetMax)
		: valueTex(value), sourceMinTex(sourceMin), sourceMaxTex(sourceMax),
		  targetMinTex(targetMin), targetMaxTex(targetMax) { }
	virtual ~RemapTexture() { }

	virtual TextureType GetType() const { return REMAP_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		valueTex->AddReferencedTextures(referencedTexs);
		sourceMinTex->AddReferencedTextures(referencedTexs);
		sourceMaxTex->AddReferencedTextures(referencedTexs);
		targetMinTex->AddReferencedTextures(referencedTexs);
		targetMaxTex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		valueTex->AddReferencedImageMaps(referencedImgMaps);
		sourceMinTex->AddReferencedImageMaps(referencedImgMaps);
		sourceMaxTex->AddReferencedImageMaps(referencedImgMaps);
		targetMinTex->AddReferencedImageMaps(referencedImgMaps);
		targetMaxTex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (valueTex == oldTex)
			valueTex = newTex;
		if (sourceMinTex == oldTex)
			sourceMinTex = newTex;
		if (sourceMaxTex == oldTex)
			sourceMaxTex = newTex;
		if (targetMinTex == oldTex)
			targetMinTex = newTex;
		if (targetMaxTex == oldTex)
			targetMaxTex = newTex;
	}

	const Texture *GetValueTex() const { return valueTex; }
	const Texture *GetSourceLowTex() const { return sourceMinTex; }
	const Texture *GetSourceHighTex() const { return sourceMaxTex; }
	const Texture *GetTargetLowTex() const { return targetMinTex; }
	const Texture *GetTargetHighTex() const { return targetMaxTex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache,
	                                         const bool useRealFileName) const;

private:
	const Texture *valueTex;
	const Texture *sourceMinTex;
	const Texture *sourceMaxTex;
	const Texture *targetMinTex;
	const Texture *targetMaxTex;
	
	static float ClampedRemap(float value,
	                          const float sourceMin, const float sourceMax,
	                          const float targetMin, const float targetMax);

	static luxrays::Spectrum ClampedRemap(luxrays::Spectrum value,
		const luxrays::Spectrum &sourceMin,
		const luxrays::Spectrum &sourceMax,
		const luxrays::Spectrum &targetMin,
		const luxrays::Spectrum &targetMax);
};

}

#endif	/* _SLG_REMAPTEX_H */
