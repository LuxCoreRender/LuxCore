/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_DENSITYGRIDTEX_H
#define	_SLG_DENSITYGRIDTEX_H

#include "slg/textures/texture.h"
#include "slg/imagemap/imagemap.h"

namespace slg {

//------------------------------------------------------------------------------
// DensityGrid texture
//------------------------------------------------------------------------------
	
class DensityGridTexture : public Texture {
public:
	DensityGridTexture(const TextureMapping3D *mp, const u_int nx, const u_int ny, const u_int nz,
            const ImageMap *imageMap);
	virtual ~DensityGridTexture() { }

	virtual TextureType GetType() const { return DENSITYGRID_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return imageMap->GetSpectrumMeanY(); }
	virtual float Filter() const { return imageMap->GetSpectrumMean(); }

	u_int GetWidth() const { return nx; }
	u_int GetHeight() const { return ny; }
	u_int GetDepth() const { return nz; }
	const ImageMap *GetImageMap() const { return imageMap; }

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		referencedImgMaps.insert(imageMap);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;
	const TextureMapping3D *GetTextureMapping() const { return mapping; }

	static ImageMap *ParseData(const luxrays::Property &Property,
			const u_int nx, const u_int ny, const u_int nz,
			ImageMapStorage::WrapType wrapMode);
	static ImageMap *ParseOpenVDB(const std::string &fileName, const std::string &gridName,
			const u_int nx, const u_int ny, const u_int nz,
			ImageMapStorage::WrapType wrapMode);

private:
	float D(int x, int y, int z) const;

	const TextureMapping3D *mapping;
    const int nx, ny, nz;

	const ImageMap *imageMap;
};

}

#endif	/* _SLG_DENSITYGRIDTEX_H */
