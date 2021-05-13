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

#ifndef _SLG_GLOSSYMAT_H
#define	_SLG_GLOSSYMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Glossy2 material
//------------------------------------------------------------------------------

class Glossy2Material : public Material {
public:
	Glossy2Material(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
			const Texture *kd, const Texture *ks, const Texture *u, const Texture *v,
			const Texture *ka, const Texture *d, const Texture *i, const bool mbounce, const bool doublesided);

	virtual MaterialType GetType() const { return GLOSSY2; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; }

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
	const Texture *GetKs() const { return Ks; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }
	const Texture *GetKa() const { return Ka; }
	const Texture *GetDepth() const { return depth; }
	const Texture *GetIndex() const { return index; }
	const bool IsMultibounce () const { return multibounce; }
	const bool IsDoubleSided () const { return doublesided; }

private:
	const Texture *Kd;
	const Texture *Ks;
	const Texture *nu;
	const Texture *nv;
	const Texture *Ka;
	const Texture *depth;
	const Texture *index;
	const bool multibounce;
	const bool doublesided;
};

}

#endif	/* _SLG_GLOSSYMAT_H */
