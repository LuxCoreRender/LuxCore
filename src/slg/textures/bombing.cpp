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
		return Lerp(bulletMaskTex->Y(), backgroundTex->Y(), bulletTex->Y());
}

float BombingTexture::Filter() const {
	return Lerp(bulletMaskTex->Filter(), backgroundTex->Filter(), bulletTex->Filter());
}

float BombingTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum BombingTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Spectrum backgroundValue = backgroundTex->GetSpectrumValue(hitPoint);

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

			// Pick cell random values
			const u_int randomImageMapStorageIndex = (((int)currentCell.u) % (randomImageMapWidth / 2)) * 2 +
					(((int)currentCell.v) % randomImageMapHeight) * randomImageMapWidth;
			const Spectrum noise0 = randomImageMapStorage->GetSpectrum(randomImageMapStorageIndex);

			const float currentCellRandomOffsetX = noise0.c[0];
			const float currentCellRandomOffsetY = noise0.c[1];
			const float currentCellRandomPriority = noise0.c[2];

			// Check the priority of this cell
			if (currentCellRandomPriority <= resultPriority)
				continue;

			// Find The cell UV coordinates
			UV bulletUV(currentCellOffset.u - currentCellRandomOffsetX, currentCellOffset.v - currentCellRandomOffsetY);

			// Pick some more cell random values
			const Spectrum noise1 = randomImageMapStorage->GetSpectrum(randomImageMapStorageIndex + 1);
			const float currentCellRandomRotate = noise1.c[0];
			const float currentCellRandomScale = noise1.c[1];
			const float currentCellRandomBullet = noise1.c[2];

			// Translate to the cell center
			const UV translatedUV(bulletUV.u - .5f, bulletUV.v - .5f);

			// Apply random scale
			const float scaleFactor = Lerp(currentCellRandomScale, 1.f, 1.f + randomScaleFactor);
			const UV scaledUV(translatedUV.u * scaleFactor, translatedUV.v * scaleFactor);

			// Apply random rotation
			const float angle = useRandomRotation ? currentCellRandomRotate * (2.f * M_PI) : 0.f;
			const float sinAngle = sinf(angle);
			const float cosAngle = cosf(angle);
			const UV rotatedUV(
					scaledUV.u * cosAngle - scaledUV.v * sinAngle,
					scaledUV.u * sinAngle + scaledUV.v * cosAngle);

			// Translate back the cell center
			bulletUV.u = rotatedUV.u + .5f;
			bulletUV.v = rotatedUV.v + .5f;

			// Check if I'm inside the cell
			if ((bulletUV.u < 0.f) || (bulletUV.u >= 1.f) ||
					(bulletUV.v < 0.f) || (bulletUV.v >= 1.f))
				continue;

			// Pick a bullet out if multiple available shapes (if multiBulletCount > 1)
			const u_int currentCellRandomBulletIndex = Floor2UInt(multiBulletCount * currentCellRandomBullet);
			bulletUV.u = (currentCellRandomBulletIndex + bulletUV.u) / multiBulletCount;

			hitPointTmp.defaultUV = bulletUV;

			const Spectrum bulletValue = bulletTex->GetSpectrumValue(hitPointTmp);
			const float maskValue = bulletMaskTex->GetFloatValue(hitPointTmp);
			
			if (maskValue > 0.f) {
				resultPriority = currentCellRandomPriority;
				result = Lerp(maskValue, backgroundValue, bulletValue);
			}
		}
	}

	return result;
}

void BombingTexture::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Texture::AddReferencedTextures(referencedTexs);

	backgroundTex->AddReferencedTextures(referencedTexs);
	bulletTex->AddReferencedTextures(referencedTexs);
	bulletMaskTex->AddReferencedTextures(referencedTexs);
}

void BombingTexture::AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	backgroundTex->AddReferencedImageMaps(referencedImgMaps);
	bulletTex->AddReferencedImageMaps(referencedImgMaps);
	bulletMaskTex->AddReferencedImageMaps(referencedImgMaps);
}

void BombingTexture::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	if (backgroundTex == oldTex)
		backgroundTex = newTex;
	if (bulletTex == oldTex)
		bulletTex = newTex;
	if (bulletMaskTex == oldTex)
		bulletMaskTex = newTex;
}

Properties BombingTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("bombing"));
	props.Set(Property("scene.textures." + name + ".background")(backgroundTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".bullet")(bulletTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".bullet.mask")(bulletMaskTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".bullet.randomscale.range")(randomScaleFactor));
	props.Set(Property("scene.textures." + name + ".bullet.randomrotation.enable")(useRandomRotation));
	props.Set(Property("scene.textures." + name + ".bullet.count")(multiBulletCount));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
