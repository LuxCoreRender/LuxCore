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

#ifndef _SLG_METAL2MAT_H
#define	_SLG_METAL2MAT_H

#include "slg/textures/fresnel/fresneltexture.h"
#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Metal2 material
//------------------------------------------------------------------------------

class Metal2Material : public Material {
public:
	Metal2Material(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
			const Texture *nn, const Texture *kk, const Texture *u, const Texture *v);
	Metal2Material(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
			const FresnelTexture *ft, const Texture *u, const Texture *v);

	virtual MaterialType GetType() const { return METAL2; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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

	const FresnelTexture *GetFresnel() const { return fresnelTex; }
	const Texture *GetN() const { return n; }
	const Texture *GetK() const { return k; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }
	
private:
	const FresnelTexture *fresnelTex;
	// For compatibility with the past
	const Texture *n, *k;

	const Texture *nu;
	const Texture *nv;
};

}

#endif	/* _SLG_METAL2MAT_H */
