/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_UVTEX_H
#define	_SLG_UVTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

class UVTexture : public Texture {
public:
	UVTexture(const TextureMapping2D *mp) : mapping(mp) { }
	virtual ~UVTexture() { delete mapping; }

	virtual TextureType GetType() const { return UV_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		return luxrays::Spectrum(.5f, .5f, 0.f).Y();
	}
	virtual float Filter() const {
		return luxrays::Spectrum(.5f, .5f, 0.f).Filter();
	}

	const TextureMapping2D *GetTextureMapping() const { return mapping; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping2D *mapping;
};

}

#endif	/* _SLG_UVTEX_H */
