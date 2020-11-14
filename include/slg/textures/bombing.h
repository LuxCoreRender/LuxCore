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

#ifndef _SLG_BOMBINGTEX_H
#define	_SLG_BOMBINGTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Bombing texture
//------------------------------------------------------------------------------

class BombingTexture : public Texture {
public:
	BombingTexture(const TextureMapping2D *mp, const Texture *backgroundTx,
			const Texture *bulletTx, const Texture *bulletMaskTx,
			const float randomScaleFctr, const bool useRandomRot,
			const u_int multiBulletCnt) :
			mapping(mp), backgroundTex(backgroundTx), bulletTex(bulletTx),
			bulletMaskTex(bulletMaskTx),
			randomScaleFactor(randomScaleFctr), useRandomRotation(useRandomRot),
			multiBulletCount(multiBulletCnt) { }
	virtual ~BombingTexture() { delete mapping; }

	virtual TextureType GetType() const { return BOMBING_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const Texture *GetBackgroundTex() const { return backgroundTex; }
	const Texture *GetBulletTex() const { return bulletTex; }
	const Texture *GetBulletMaskTex() const { return bulletMaskTex; }
	
	const float GetRandomScaleFactor() const { return randomScaleFactor; }
	const bool GetUseRandomRotation() const { return useRandomRotation; }
	const u_int GetMultiBulletCount() const { return multiBulletCount; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:

	const TextureMapping2D *mapping;
	const Texture *backgroundTex;
	const Texture *bulletTex;
	const Texture *bulletMaskTex;

	const float randomScaleFactor;
	const bool useRandomRotation;
	const u_int multiBulletCount;
};

}

#endif	/* _SLG_BOMBINGTEX_H */
