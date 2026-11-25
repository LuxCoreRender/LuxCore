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

#ifndef _SLG_BRICKTEX_H
#define	_SLG_BRICKTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

typedef enum {
	FLEMISH, RUNNING, ENGLISH, HERRINGBONE, BASKET, KETTING
} MasonryBond;

class BrickTexture : public Texture {
public:
	BrickTexture(TextureMapping3DUPtr&& mp, TextureConstRef t1,
			TextureConstRef t2, TextureConstRef t3,
			float brickw, float brickh, float brickd, float mortar,
			float r, const std::string &b, const float modulationBias);
	virtual ~BrickTexture() {  }

	virtual TextureType GetType() const { return BRICK; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		const float m = powf(luxrays::Clamp(1.f - mortarsize, 0.f, 1.f), 3);
		return luxrays::Lerp(m, GetTexture2().Y(), GetTexture1().Y());
	}
	virtual float Filter() const {
		const float m = powf(luxrays::Clamp(1.f - mortarsize, 0.f, 1.f), 3);
		return luxrays::Lerp(m, GetTexture2().Filter(), GetTexture1().Filter());
	}

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		GetTexture1().AddReferencedTextures(referencedTexs);
		GetTexture2().AddReferencedTextures(referencedTexs);
		GetTexture3().AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const {
		GetTexture1().AddReferencedImageMaps(referencedImgMaps);
		GetTexture2().AddReferencedImageMaps(referencedImgMaps);
		GetTexture3().AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) {
		if (&GetTexture1() == &oldTex)
			tex1 = newTex;
		if (&GetTexture2() == &oldTex)
			tex2 = newTex;
		if (&GetTexture3() == &oldTex)
			tex3 = newTex;
	}

	TextureMapping3DConstRef GetTextureMapping() const { return *mapping; }
	TextureConstRef GetTexture1() const { return tex1; }
	TextureConstRef GetTexture2() const { return tex2; }
	TextureConstRef GetTexture3() const { return tex3; }
	MasonryBond GetBond() const { return bond; }
	const luxrays::Point &GetOffset() const { return offset; }
	float GetInitialBrickWidth() const { return initialbrickwidth; }
	float GetInitialBrickHeight() const { return initialbrickheight; }
	float GetInitialBrickDepth() const { return initialbrickdepth; }
	float GetBrickWidth() const { return brickwidth; }
	float GetBrickHeight() const { return brickheight; }
	float GetBrickDepth() const { return brickdepth; }
	float GetMortarSize() const { return mortarsize; }
	float GetProportion() const { return proportion; }
	float GetInvProportion() const { return invproportion; }
	float GetRun() const { return run; }
	float GetMortarWidth() const { return mortarwidth; }
	float GetMortarHeight() const { return mortarheight; }
	float GetMortarDepth() const { return mortardepth; }
	float GetModulationBias() const { return modulationBias; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	bool RunningAlternate(const luxrays::Point &p, luxrays::Point &i, luxrays::Point &b, int nWhole) const;
	bool Basket(const luxrays::Point &p, luxrays::Point &i) const;
	bool Herringbone(const luxrays::Point &p, luxrays::Point &i) const;
	bool Running(const luxrays::Point &p, luxrays::Point &i, luxrays::Point &b) const;
	bool English(const luxrays::Point &p, luxrays::Point &i, luxrays::Point &b) const;
	float BrickNoise(u_int n) const;

	TextureMapping3DUPtr mapping;
	std::reference_wrapper<const Texture> tex1, tex2, tex3;  // Const, but swappable...

	MasonryBond bond;
	luxrays::Point offset;
	float brickwidth, brickheight, brickdepth, mortarsize;
	float proportion, invproportion, run;
	float mortarwidth, mortarheight, mortardepth;

	// brickwidth, brickheight, brickdepth are modified by HERRINGBONE
	// and BASKET brick types. I need to save the initial values here.
	float initialbrickwidth, initialbrickheight, initialbrickdepth;
	
	float modulationBias;
};

}

#endif	/* _SLG_BRICKTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
