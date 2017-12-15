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

#ifndef _SLG_CONSTFLOATTEX_H
#define	_SLG_CONSTFLOATTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Constant float texture
//------------------------------------------------------------------------------

class ConstFloatTexture : public Texture {
public:
	ConstFloatTexture(const float v) : value(v) { }
	virtual ~ConstFloatTexture() { }

	virtual TextureType GetType() const { return CONST_FLOAT; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return value; }
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return luxrays::Spectrum(value); }
	virtual float Y() const { return value; }
	virtual float Filter() const { return value; }
	virtual luxrays::Normal Bump(const HitPoint &hitPoint, const float sampleDistance) const { return hitPoint.shadeN; }

	float GetValue() const { return value; };

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
	float value;
};

}

#endif	/* _SLG_CONSTFLOATTEX_H */
