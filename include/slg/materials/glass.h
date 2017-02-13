/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_GLASSMAT_H
#define	_SLG_GLASSMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

class GlassMaterial : public Material {
public:
	GlassMaterial(const Texture *transp, const Texture *emitted, const Texture *bump,
			const Texture *refl, const Texture *trans,
			const Texture *exteriorIorFact, const Texture *interiorIorFact) :
			Material(transp, emitted, bump),
			Kr(refl), Kt(trans), exteriorIor(exteriorIorFact), interiorIor(interiorIorFact) { }

	virtual MaterialType GetType() const { return GLASS; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	virtual bool IsDelta() const { return true; }

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetExteriorIOR() const { return exteriorIor; }
	const Texture *GetInteriorIOR() const { return interiorIor; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *exteriorIor;
	const Texture *interiorIor;
};

}

#endif	/* _SLG_GLASSMAT_H */
