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

#include "slg/textures/bombing.h"
#include "slg/textures/imagemaptex.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Bombing texture
//
// From GPU Gems: Chapter 20. Texture Bombing
// https://developer.download.nvidia.com/books/HTML/gpugems/gpugems_ch20.html
//------------------------------------------------------------------------------

float BombingTexture::Y() const {
		return Lerp(bulletMaskTex->Y(), backgourndTex->Y(), bulletTex->Y());
}

float BombingTexture::Filter() const {
	return Lerp(bulletMaskTex->Filter(), backgourndTex->Filter(), bulletTex->Filter());
}

float BombingTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum BombingTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Spectrum backgroundValue = backgourndTex->GetSpectrumValue(hitPoint);

	const UV uv = mapping->Map(hitPoint);

	const UV cell(floorf(uv.u), floorf(uv.v));
	const UV cellOffset(uv.u - cell.u, uv.v - cell.v);

	HitPoint hitPointTmp = hitPoint;
	Spectrum result = backgroundValue;
	float resultPriority = -1.f;

	const ImageMapStorage *randomImageMapStorage =  ImageMapTexture::randomImageMap->GetStorage();
	const u_int randomImageMapWidth = ImageMapTexture::randomImageMap->GetWidth();
	const u_int randomImageMapHeight = ImageMapTexture::randomImageMap->GetHeight();

	for (int i = -1; i <= 0; ++i) {
	  for (int j = -1; j <= 0; ++j) {
			const UV currentCell = cell + UV(i, j);
			const UV currentCellOffset = cellOffset - UV(i, j);

			const Spectrum noise = randomImageMapStorage->GetSpectrum(
					((int)currentCell.u) % randomImageMapWidth,
					((int)currentCell.v) % randomImageMapHeight);

			const float currentCellRandomOffsetX = noise.c[0];
			const float currentCellRandomOffsetY = noise.c[1];
			const float currentCellRandomPriority = noise.c[2];

			UV bulletUV(currentCellOffset.u - currentCellRandomOffsetX, currentCellOffset.v - currentCellRandomOffsetY);
			if ((bulletUV.u < 0.f) || (bulletUV.u >= 1.f) ||
					(bulletUV.v < 0.f) || (bulletUV.v >= 1.f))
				continue;

			// I generate a new random variable starting from currentCellRandomPriority
			const float currentCellRandomBullet = fabsf(noise.c[2] - .5f) * 2.f;
			// Pick a bullet out if multiple avilable shapes (if multiBulletCount > 1)
			const u_int currentCellRandomBulletIndex = Floor2UInt(multiBulletCount * currentCellRandomBullet);
			bulletUV.u = (currentCellRandomBulletIndex + bulletUV.u) / multiBulletCount;

			hitPointTmp.defaultUV = bulletUV;

			const Spectrum bulletValue = bulletTex->GetSpectrumValue(hitPointTmp);
			const float maskValue = bulletMaskTex->GetFloatValue(hitPointTmp);
			
			if ((currentCellRandomPriority > resultPriority) && (maskValue > 0.f)) {
				resultPriority = currentCellRandomPriority;
				result = Lerp(maskValue, backgroundValue, bulletValue);
			}
		}
	}

	return result;
}

void BombingTexture::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Texture::AddReferencedTextures(referencedTexs);

	backgourndTex->AddReferencedTextures(referencedTexs);
	bulletTex->AddReferencedTextures(referencedTexs);
	bulletMaskTex->AddReferencedTextures(referencedTexs);
}

void BombingTexture::AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	backgourndTex->AddReferencedImageMaps(referencedImgMaps);
	bulletTex->AddReferencedImageMaps(referencedImgMaps);
	bulletMaskTex->AddReferencedImageMaps(referencedImgMaps);
}

void BombingTexture::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	if (backgourndTex == oldTex)
		backgourndTex = newTex;
	if (bulletTex == oldTex)
		bulletTex = newTex;
	if (bulletMaskTex == oldTex)
		bulletMaskTex = newTex;
}

Properties BombingTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("bombing"));
	props.Set(Property("scene.textures." + name + ".background")(backgourndTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".bullet")(bulletTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".bulletmask")(bulletMaskTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".bulletcount")(multiBulletCount));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
