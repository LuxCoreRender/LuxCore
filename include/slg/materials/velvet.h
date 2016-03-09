/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_VELVETMAT_H
#define	_SLG_VELVETMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Velvet material
//------------------------------------------------------------------------------

class VelvetMaterial : public Material {
public:
	VelvetMaterial(const Texture *transp, const Texture *emitted, const Texture *bump,
			const Texture *kd, const Texture *p1, const Texture *p2, const Texture *p3,
			const Texture *thickness) : Material(transp, emitted, bump), Kd(kd),
			P1(p1), P2(p2), P3(p3), Thickness(thickness) { }

	virtual MaterialType GetType() const { return VELVET; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

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
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKd() const { return Kd; }
	const Texture *GetP1() const { return P1; }
	const Texture *GetP2() const { return P2; }
	const Texture *GetP3() const { return P3; }
	const Texture *GetThickness() const { return Thickness; }

private:
	const Texture *Kd;
	const Texture *P1;
	const Texture *P2;
	const Texture *P3;
	const Texture *Thickness;
};

}

#endif	/* _SLG_VELVETMAT_H */
