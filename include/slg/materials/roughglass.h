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

#ifndef _SLG_ROUGHGLASSMAT_H
#define	_SLG_ROUGHGLASSMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// RoughGlass material
//------------------------------------------------------------------------------

class RoughGlassMaterial : public Material {
public:
	RoughGlassMaterial(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
			const Texture *refl, const Texture *trans,
			const Texture *exteriorIorFact, const Texture *interiorIorFact,
			const Texture *u, const Texture *v,
			const Texture *filmThickness, const Texture *filmIor);

	virtual MaterialType GetType() const { return ROUGHGLASS; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT | TRANSMIT; };
	virtual float GetGlossiness() const { return glossiness; }

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event, const BSDFEvent eventHint = NONE) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetExteriorIOR() const { return exteriorIor; }
	const Texture *GetInteriorIOR() const { return interiorIor; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }
	const Texture *GetFilmThickness() const { return filmThickness; }
	const Texture *GetFilmIOR() const { return filmIor; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *exteriorIor;
	const Texture *interiorIor;
	const Texture *nu;
	const Texture *nv;
	const Texture *filmThickness;
	const Texture *filmIor;
};

}

#endif	/* _SLG_ROUGHGLASSMAT_H */
