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

namespace slg {


//------------------------------------------------------------------------------
// DensityGrid texture
//------------------------------------------------------------------------------
class DensityGridTexture : public Texture {
public:
	enum WrapMode { WRAP_REPEAT, WRAP_BLACK, WRAP_WHITE, WRAP_CLAMP };
	DensityGridTexture(const TextureMapping3D *mp, const u_int nx, const u_int ny, const u_int nz,
            const float *dt, const std::string wrapmode);
	virtual ~DensityGridTexture() { }

	virtual TextureType GetType() const { return DENSITYGRID_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

	const std::vector<float> &GetData() const { return data; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;
	const TextureMapping3D *GetTextureMapping() const { return mapping; }

private:
	const float D(int x, int y, int z) const {
		return data[((luxrays::Clamp(z, 0, nz - 1) * ny) + luxrays::Clamp(y, 0, ny - 1)) * nx + luxrays::Clamp(x, 0, nx - 1)];
	}

	const TextureMapping3D *mapping;
    const int nx, ny, nz;
	std::vector<float> data;
	WrapMode wrapMode;
};

}

#endif	/* _SLG_DENSITYGRIDTEX_H */
