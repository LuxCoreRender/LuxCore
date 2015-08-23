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

#ifndef _SLG_CONSTFLOAT3TEX_H
#define	_SLG_CONSTFLOAT3TEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Constant float3 texture
//------------------------------------------------------------------------------

class ConstFloat3Texture : public Texture {
public:
	ConstFloat3Texture(const luxrays::Spectrum &c) : color(c) { }
	virtual ~ConstFloat3Texture() { }

	virtual TextureType GetType() const { return CONST_FLOAT3; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return color.Y(); }
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return color; }
	virtual float Y() const { return color.Y(); }
	virtual float Filter() const { return color.Filter(); }

	const luxrays::Spectrum &GetColor() const { return color; };

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	luxrays::Spectrum color;
};

}

#endif	/* _SLG_CONSTFLOAT3TEX_H */
