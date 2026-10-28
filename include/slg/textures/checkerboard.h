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

#ifndef _SLG_CHECKERBOARDTEX_H
#define	_SLG_CHECKERBOARDTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

class CheckerBoard2DTexture : public Texture {
public:
	CheckerBoard2DTexture(
		TextureMapping2DUPtr&& mp,
		TextureRef t1,
		TextureRef t2
	) : mapping(std::move(mp)), tex1(t1), tex2(t2)
	{ }

	virtual TextureType GetType() const { return CHECKERBOARD2D; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return (GetTexture1().Y() + GetTexture2().Y()) * .5f; }
	virtual float Filter() const { return (GetTexture1().Filter() + GetTexture2().Filter()) * .5f; }

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture1().AddReferencedTextures(referencedTexs);
		GetTexture2().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTexture1().AddReferencedImageMaps(referencedImgMaps);
		GetTexture2().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		if (&tex1.get() == &oldTex)
			tex1 = newTex;
		if (&tex2.get() == &oldTex)
			tex2 = newTex;
	}

	TextureMapping2DConstRef GetTextureMapping() const { return *mapping; }
	TextureConstRef GetTexture1() const { return tex1; }
	TextureConstRef GetTexture2() const { return tex2; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	TextureMapping2DUPtr mapping;  // Owned by this object
	std::reference_wrapper<Texture> tex1;
	std::reference_wrapper<Texture> tex2;
};

class CheckerBoard3DTexture : public Texture {
public:
	CheckerBoard3DTexture(TextureMapping3DUPtr&& mp, TextureRef t1, TextureRef t2) : mapping(std::move(mp)), tex1(t1), tex2(t2) { }
	virtual ~CheckerBoard3DTexture() {  }

	virtual TextureType GetType() const { return CHECKERBOARD3D; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return (GetTexture1().Y() + GetTexture2().Y()) * .5f; }
	virtual float Filter() const { return (GetTexture1().Filter() + GetTexture2().Filter()) * .5f; }

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture1().AddReferencedTextures(referencedTexs);
		GetTexture2().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const {
		GetTexture1().AddReferencedImageMaps(referencedImgMaps);
		GetTexture2().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		if (tex1 == &oldTex)
			tex1 = newTex;
		if (tex2 == &oldTex)
			tex2 = newTex;
	}

	TextureMapping3DConstRef GetTextureMapping() const { return *mapping; }
	TextureConstRef GetTexture1() const { return tex1; }
	TextureConstRef GetTexture2() const { return tex2; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	TextureMapping3DUPtr mapping;  // Mapping is owned
	std::reference_wrapper<Texture> tex1;
	std::reference_wrapper<Texture> tex2;
};

}

#endif	/* _SLG_CHECKERBOARDTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
