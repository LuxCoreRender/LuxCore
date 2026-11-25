/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_DISTORTTEXTURE_H
#define	_SLG_DISTORTTEXTURE_H

#include "slg/imagemap/imagemap.h"
#include "slg/textures/texture.h"
#include <functional>

namespace slg {

//------------------------------------------------------------------------------
// Distort texture
//------------------------------------------------------------------------------

class DistortTexture : public Texture {
public:
	DistortTexture(TextureRef tex, TextureRef offset, const float strength) :
		tex(tex), offset(offset), strength(strength) { }
	virtual ~DistortTexture() { }

	virtual TextureType GetType() const { return DISTORT_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		return GetTex().Y();
	}
	virtual float Filter() const {
		return GetTex().Filter();
	}

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTex().AddReferencedTextures(referencedTexs);
		GetOffset().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const {
		GetTex().AddReferencedImageMaps(referencedImgMaps);
		GetOffset().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) {
		if (&GetTex() == &oldTex)
			tex = newTex;
		if (&offset.get() == &oldTex)
			offset = newTex;
	}

	TextureConstRef GetTex() const { return tex; }
	TextureConstRef GetOffset() const { return offset; }
	const float GetStrength() const { return strength; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	void GetTmpHitPoint(const HitPoint &hitPoint, HitPoint &tmpHitPoint) const;

	std::reference_wrapper<Texture> tex;
	std::reference_wrapper<Texture> offset;
	const float strength;
};

}

#endif	/* _SLG_DISTORTTEXTURE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
