/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_FRESNELCONSTTEX_H
#define	_SLG_FRESNELCONSTTEX_H

#include "slg/textures/fresnel/fresneltexture.h"

namespace slg {

//------------------------------------------------------------------------------
// Fresnel const texture
//------------------------------------------------------------------------------

class FresnelConstTexture : public FresnelTexture {
public:
	FresnelConstTexture(const luxrays::Spectrum &nVal, const luxrays::Spectrum &kVal) : n(nVal), k(kVal) { }
	virtual ~FresnelConstTexture() { }

	virtual TextureType GetType() const { return FRESNELCONST_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const;
	virtual float Filter() const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint, const float cosi) const;

	const luxrays::Spectrum GetN() const { return n; };
	const luxrays::Spectrum GetK() const { return k; };

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const luxrays::Spectrum n, k;
};

}

#endif	/* _SLG_FRESNELCONSTTEX_H */
