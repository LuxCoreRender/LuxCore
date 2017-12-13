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

#ifndef _SLG_FRESNELTEXTURE_H
#define	_SLG_FRESNELTEXTURE_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Fresnel texture interface
//------------------------------------------------------------------------------

class FresnelTexture : public Texture {
public:
	FresnelTexture() { }
	virtual ~FresnelTexture() { }

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint, const float cosi) const = 0;

	static float ApproxN(const float Fr);
	static luxrays::Spectrum ApproxN(const luxrays::Spectrum &Fr);
	static float ApproxK(const float Fr);
	static luxrays::Spectrum ApproxK(const luxrays::Spectrum &Fr);

	static luxrays::Spectrum SchlickEvaluate(const luxrays::Spectrum &ks,
			const float cosi);
	static luxrays::Spectrum GeneralEvaluate(const luxrays::Spectrum &eta,
			const luxrays::Spectrum &k, const float cosi);
	static luxrays::Spectrum CauchyEvaluate(const float eta, const float cosi);

private:
	static luxrays::Spectrum FrFull(const float cosi, const luxrays::Spectrum &cost,
			const luxrays::Spectrum &eta, const luxrays::Spectrum &k);
	static luxrays::Spectrum FrDiel2(const float cosi, const luxrays::Spectrum &cost,
			const luxrays::Spectrum &eta);
};

}

#endif	/* _SLG_FRESNELTEXTURE_H */
