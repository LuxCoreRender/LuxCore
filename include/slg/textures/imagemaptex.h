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

#ifndef _SLG_IMAGEMAPTEX_H
#define	_SLG_IMAGEMAPTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

class ImageMapTexture : public Texture {
public:
	ImageMapTexture(const ImageMap *img, const TextureMapping2D *mp, const float g,
			const bool rt);
	virtual ~ImageMapTexture();

	virtual TextureType GetType() const { return IMAGEMAP; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual luxrays::Normal Bump(const HitPoint &hitPoint, const float sampleDistance) const;
	virtual float Y() const { return gain * imageMap->GetSpectrumMeanY(); }
	virtual float Filter() const { return gain * imageMap->GetSpectrumMean(); }

	const ImageMap *GetImageMap() const { return imageMap; }
	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const float GetGain() const { return gain; }

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		referencedImgMaps.insert(imageMap);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	luxrays::Spectrum SampleTile(const luxrays::UV &vertex, const luxrays::UV &offset) const;

	const ImageMap *imageMap;
	const TextureMapping2D *mapping;
	float gain;
	bool randomizedTiling;

	// Used for randomized tiling
	ImageMap *randomizedTilingInvLUT;
	ImageMap *randomMap;
};

}

#endif	/* _SLG_IMAGEMAPTEX_H */
