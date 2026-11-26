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

#ifndef _SLG_IMAGEMAPTEX_H
#define	_SLG_IMAGEMAPTEX_H

#include <functional>
#include <memory>

#include "cuew.h"
#include "luxrays/usings.h"
#include "slg/imagemap/imagemap.h"
#include "slg/textures/texture.h"
#include "slg/usings.h"

namespace slg {

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

class ImageMapTexture : public Texture {
public:
	ImageMapTexture(
		const std::string &texName,
		ImageMapConstRef img,
		TextureMapping2DUPtr&& mp,
		const float g,
		const bool rt,
		ImageMapConstRef randomIM
	);

	virtual TextureType GetType() const { return IMAGEMAP; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual luxrays::Normal Bump(const HitPoint &hitPoint, const float sampleDistance) const;
	virtual float Y() const { return gain * imageMap.GetSpectrumMeanY(); }
	virtual float Filter() const { return gain * imageMap.GetSpectrumMean(); }

	ImageMapConstRef GetImageMap() const { return imageMap; }
	TextureMapping2DConstRef GetTextureMapping() const { return *mapping; }
	const float GetGain() const { return gain; }

	bool HasRandomizedTiling() const { return randomizedTiling; }
	ImageMapConstRef GetRandomizedTilingLUT() const { return *refRandomizedTilingLUT; }
	ImageMapConstRef GetRandomizedTilingInvLUT() const { return *refRandomizedTilingInvLUT; }

	virtual void AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const;

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	static std::unique_ptr<ImageMapTexture> AllocImageMapTexture(
		const std::string &texName,
		ImageMapCache &imgMapCache,
		ImageMapConstRef img,
		TextureMapping2DUPtr&& mp,
		const float g,
		const bool rt,
		ImageMapConstRef randomIM
	);

	static ImageMapUPtr AllocRandomImageMap(const u_int size);

	//inline static ImageMapSPtr randomImageMap = std::move(AllocRandomImageMap(512));
	//TODO

	virtual ~ImageMapTexture();

private:

	luxrays::Spectrum SampleTile(const luxrays::UV &vertex, const luxrays::UV &offset) const;
	luxrays::Spectrum RandomizedTilingGetSpectrumValue(const luxrays::UV &pos) const;

	ImageMapConstRef imageMap;
	ImageMapConstRef randomImageMap;
	TextureMapping2DUPtr mapping;
	float gain;

	// Used for randomized tiling
	bool randomizedTiling;

	// References to the LUTs
	// Temporary LUTs MUST HAVE BEEN MOVED to Image Cache before those
	// properties are used, otherwise their behaviour is undefined
	const ImageMap * refRandomizedTilingLUT = nullptr;
	const ImageMap * refRandomizedTilingInvLUT = nullptr;

	// Temporary LUT. They first are created inside ImageMapTexture, but
	// they are moved to Image Cache
	ImageMapUPtr randomizedTilingLUT;
	ImageMapUPtr randomizedTilingInvLUT;
};

}

#endif	/* _SLG_IMAGEMAPTEX_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
