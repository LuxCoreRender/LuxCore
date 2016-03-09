/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_MARBLETEX_H
#define	_SLG_MARBLETEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

class MarbleTexture : public Texture {
public:
	MarbleTexture(const TextureMapping3D *mp, const int octs, const float omg,
			float sc, float var) :
			mapping(mp), octaves(octs), omega(omg), scale(sc), variation(var) { }
	virtual ~MarbleTexture() { delete mapping; }

	virtual TextureType GetType() const { return MARBLE; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	int GetOctaves() const { return octaves; }
	float GetOmega() const { return omega; }
	float GetScale() const { return scale; }
	float GetVariation() const { return variation; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	const int octaves;
	const float omega, scale, variation;
};

}

#endif	/* _SLG_MARBLETEX_H */
