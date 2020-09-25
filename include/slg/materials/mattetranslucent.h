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

#ifndef _SLG_MATTETRANSLUCENTMAT_H
#define	_SLG_MATTETRANSLUCENTMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

class MatteTranslucentMaterial : public Material {
public:
	MatteTranslucentMaterial(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
			const Texture *refl, const Texture *trans);

	virtual MaterialType GetType() const { return MATTETRANSLUCENT; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT | TRANSMIT; };

	virtual luxrays::Spectrum Albedo(const HitPoint &hitPoint) const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }

private:
	const Texture *Kr;
	const Texture *Kt;
};

}

#endif	/* _SLG_MATTETRANSLUCENTMAT_H */
