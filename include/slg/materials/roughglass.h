/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
	using TexRef = TextureConstPtr;

	RoughGlassMaterial(TexRef frontTransp, TexRef backTransp,
			TexRef emitted, TexRef bump,
			TexRef refl, TexRef trans,
			TexRef exteriorIorFact, TexRef interiorIorFact,
			TexRef u, TexRef v,
			TexRef filmThickness, TexRef filmIor);

	virtual MaterialType GetType() const { return ROUGHGLASS; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT | TRANSMIT; };
	virtual float GetGlossiness() const { return glossiness; }

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

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexsreferencedTexs) const;
	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex);

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;


	TexRef GetKr() const { return Kr; }
	TexRef GetKt() const { return Kt; }
	TexRef GetExteriorIOR() const { return exteriorIor; }
	TexRef GetInteriorIOR() const { return interiorIor; }
	TexRef GetNu() const { return nu; }
	TexRef GetNv() const { return nv; }
	TexRef GetFilmThickness() const { return filmThickness; }
	TexRef GetFilmIOR() const { return filmIor; }

private:
	TexRef Kr;
	TexRef Kt;
	TexRef exteriorIor;
	TexRef interiorIor;
	TexRef nu;
	TexRef nv;
	TexRef filmThickness;
	TexRef filmIor;
};

}

#endif	/* _SLG_ROUGHGLASSMAT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
