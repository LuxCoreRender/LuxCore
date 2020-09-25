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

#ifndef _SLG_TWOSIDEDMAT_H
#define	_SLG_TWOSIDEDMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Two-sided material
//------------------------------------------------------------------------------

class TwoSidedMaterial : public Material {
public:
	TwoSidedMaterial(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
			const Material *frontMat, const Material *backMat);

	virtual MaterialType GetType() const { return TWOSIDED; }
	virtual BSDFEvent GetEventTypes() const { return eventTypes; };

	virtual bool IsLightSource() const { return isLightSource; }
	virtual bool IsDelta() const { return isDelta; }

	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint  &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent,
		const bool backTracing) const;

	virtual const Volume *GetInteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const;
	virtual const Volume *GetExteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const;

	virtual float GetEmittedRadianceY(const float oneOverPrimitiveArea) const;
	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;

	virtual void Bump(HitPoint *hitPoint) const;

	virtual luxrays::Spectrum Albedo(const HitPoint &hitPoint) const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const;
	void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);
	virtual bool IsReferencing(const Material *mat) const;
	virtual void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const;
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	const Material *GetFrontMaterial() const { return frontMat; }
	const Material *GetBackMaterial() const { return backMat; }

protected:
	virtual void UpdateAvgPassThroughTransparency();

private:
	// Used by Preprocess()
	BSDFEvent GetEventTypesImpl() const;
	bool IsLightSourceImpl() const;
	bool IsDeltaImpl() const;

	void Preprocess();

	const Material *frontMat;
	const Material *backMat;

	// Cached values for performance with very large material node trees
	BSDFEvent eventTypes;
	bool isLightSource, isDelta;
	
};

}

#endif	/* _SLG_TWOSIDEDMAT_H */
