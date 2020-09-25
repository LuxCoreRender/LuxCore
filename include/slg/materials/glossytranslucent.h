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

#ifndef _SLG_GLOSSYTRANSLUCENTMAT_H
#define	_SLG_GLOSSYTRANSLUCENTMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Glossy Translucent material
//------------------------------------------------------------------------------

class GlossyTranslucentMaterial : public Material {
public:
	GlossyTranslucentMaterial(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
			const Texture *kd, const Texture *kt, const Texture *ks, const Texture *ks2,
			const Texture *u, const Texture *u2, const Texture *v, const Texture *v2,
			const Texture *ka, const Texture *ka2, const Texture *d, const Texture *d2,
			const Texture *i, const Texture *i2, const bool mbounce, const bool mbounce2);

	virtual MaterialType GetType() const { return GLOSSYTRANSLUCENT; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT | TRANSMIT; };

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

	const Texture *GetKd() const { return Kd; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetKs() const { return Ks; }
	const Texture *GetKs_bf() const { return Ks_bf; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNu_bf() const { return nu_bf; }
	const Texture *GetNv() const { return nv; }
	const Texture *GetNv_bf() const { return nv_bf; }
	const Texture *GetKa() const { return Ka; }
	const Texture *GetKa_bf() const { return Ka_bf; }
	const Texture *GetDepth() const { return depth; }
	const Texture *GetDepth_bf() const { return depth_bf; }
	const Texture *GetIndex() const { return index; }
	const Texture *GetIndex_bf() const { return index_bf; }
	const bool IsMultibounce() const { return multibounce; }
	const bool IsMultibounce_bf() const { return multibounce_bf; }

private:
	const Texture *Kd;
	const Texture *Kt;
	const Texture *Ks;
	const Texture *Ks_bf;
	const Texture *nu;
	const Texture *nu_bf;
	const Texture *nv;
	const Texture *nv_bf;
	const Texture *Ka;
	const Texture *Ka_bf;
	const Texture *depth;
	const Texture *depth_bf;
	const Texture *index;
	const Texture *index_bf;
	const bool multibounce;
	const bool multibounce_bf;
};

}

#endif	/* _SLG_GLOSSYTRANSLUCENTMAT_H */
