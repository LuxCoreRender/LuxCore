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

#ifndef _SLG_ROUGHMATTETRANSLUCENTMAT_H
#define	_SLG_ROUGHMATTETRANSLUCENTMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// RoughMatteTranslucent material
//------------------------------------------------------------------------------

class RoughMatteTranslucentMaterial : public Material {
public:
	using TexRef = TextureConstPtr;
	RoughMatteTranslucentMaterial(TexRef frontTransp, TexRef backTransp,
			TexRef emitted, TexRef bump,
			TexRef refl, TexRef trans, TexRef s);

	virtual MaterialType GetType() const { return ROUGHMATTETRANSLUCENT; }
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

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexsreferencedTexs) const;
	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex);

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	TexRef GetKr() const { return Kr; }
	TexRef GetKt() const { return Kt; }
	TexRef GetSigma() const { return sigma; }

private:
	TexRef Kr;
	TexRef Kt;
	TexRef sigma;
};

}

#endif	/* _SLG_ROUGHMATTETRANSLUCENTMAT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
