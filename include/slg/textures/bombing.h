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

#ifndef _SLG_BOMBINGTEX_H
#define	_SLG_BOMBINGTEX_H

#include "slg/textures/texture.h"
#include "slg/usings.h"

namespace slg {

//------------------------------------------------------------------------------
// Bombing texture
//------------------------------------------------------------------------------

class BombingTexture : public Texture {
public:
	BombingTexture(
			TextureMapping2DUPtr&& mp,
			TextureConstRef backgroundTx,
			TextureConstRef bulletTx,
			TextureConstRef bulletMaskTx,
			ImageMapConstRef randomIM,
			const float randomScaleFctr,
			const bool useRandomRot,
			const u_int multiBulletCnt
	) :
			mapping(std::move(mp)), backgroundTex(backgroundTx), bulletTex(bulletTx),
			bulletMaskTex(bulletMaskTx),
			randomImageMap(randomIM),
			randomScaleFactor(randomScaleFctr), useRandomRotation(useRandomRot),
			multiBulletCount(multiBulletCnt) { }

	virtual TextureType GetType() const { return BOMBING_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexsreferencedTexs) const;
	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap * > &referencedImgMaps) const;
	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex);

	TextureMapping2DConstRef GetTextureMapping() const { return *mapping; }
	TextureConstRef GetBackgroundTex() const { return backgroundTex; }
	TextureConstRef GetBulletTex() const { return bulletTex; }
	TextureConstRef GetBulletMaskTex() const { return bulletMaskTex; }
	
	const float GetRandomScaleFactor() const { return randomScaleFactor; }
	const bool GetUseRandomRotation() const { return useRandomRotation; }
	const u_int GetMultiBulletCount() const { return multiBulletCount; }

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:

	TextureMapping2DUPtr mapping;
	ImageMapConstRef randomImageMap;

	// Underlying textures. reference_wrapper is necessary in order to be able
	// rebind ref despite constness.
	std::reference_wrapper<const Texture> backgroundTex;
	std::reference_wrapper<const Texture> bulletTex;
	std::reference_wrapper<const Texture> bulletMaskTex;

	const float randomScaleFactor;
	const bool useRandomRotation;
	const u_int multiBulletCount;
};

}

#endif	/* _SLG_BOMBINGTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
