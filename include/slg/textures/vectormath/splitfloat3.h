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

#ifndef _SLG_SPLITFLOAT3TEX_H
#define	_SLG_SPLITFLOAT3TEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Split float3 texture
//------------------------------------------------------------------------------

class SplitFloat3Texture : public Texture {
public:
	SplitFloat3Texture(TextureRef t, const u_int ch) : tex(t), channel(ch) { }
	virtual ~SplitFloat3Texture() { }

	virtual TextureType GetType() const { return SPLIT_FLOAT3; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTexture().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) override {
		updtex(tex, oldTex, newTex);
	}

	TextureConstRef GetTexture() const { return tex; }
	u_int GetChannel() const { return channel; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	std::reference_wrapper<Texture> tex;
	u_int channel;
};

}

#endif	/* _SLG_SPLITFLOAT3TEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
