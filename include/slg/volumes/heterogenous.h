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

#ifndef _SLG_HETEROGENOUSVOL_H
#define	_SLG_HETEROGENOUSVOL_H

#include "slg/volumes/volume.h"

namespace slg {

//------------------------------------------------------------------------------
// HeterogeneousVolume
//------------------------------------------------------------------------------

class HeterogeneousVolume : public Volume {
public:
	HeterogeneousVolume(const Texture *iorTex, const Texture *emiTex,
			const Texture *a, const Texture *s,
			const Texture *g, const float stepSize, const u_int maxStepsCount,
			const bool multiScattering);

	virtual float Scatter(const luxrays::Ray &ray, const float u, const bool scatteredStart,
		luxrays::Spectrum *connectionThroughput, luxrays::Spectrum *connectionEmission) const;

	// Material interface

	virtual MaterialType GetType() const { return HETEROGENEOUS_VOL; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

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

	virtual luxrays::Properties ToProperties() const;
	
	const Texture *GetSigmaA() const { return sigmaA; }
	const Texture *GetSigmaS() const { return sigmaS; }
	const Texture *GetG() const { return schlickScatter.g; }
	float GetStepSize() const { return stepSize; }
	u_int GetMaxStepsCount() const { return maxStepsCount; }
	bool IsMultiScattering() const { return multiScattering; }

protected:
	virtual luxrays::Spectrum SigmaA(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum SigmaS(const HitPoint &hitPoint) const;

private:
	const Texture *sigmaA, *sigmaS;
	SchlickScatter schlickScatter;
	float stepSize;
	u_int maxStepsCount;
	const bool multiScattering;
};

}

#endif	/* _SLG_HETEROGENOUSVOL_H */
