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

#ifndef _SLG_MIXMAT_H
#define	_SLG_MIXMAT_H

#include "slg/materials/material.h"
#include "slg/volumes/volume.h"

namespace slg {

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

class MixMaterial : public Material {
public:
	MixMaterial(TextureConstPtr frontTransp, TextureConstPtr backTransp,
			TextureConstPtr emitted, TextureConstPtr bump,
			MaterialConstRef mA, MaterialConstRef mB, TextureConstPtr mix);

	virtual MaterialType GetType() const { return MIX; }
	virtual BSDFEvent GetEventTypes() const { return eventTypes; };

	virtual bool IsLightSource() const { return isLightSource; }
	virtual bool IsDelta() const { return isDelta; }

	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent,
		const bool backTracing) const;

	virtual VolumeConstPtr GetInteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const;
	virtual VolumeConstPtr GetExteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const;

	virtual float GetEmittedRadianceY(const float oneOverPrimitiveArea) const;
	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;

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

	virtual void UpdateMaterialReferences(MaterialConstRef oldMat, MaterialRef newMat);
	virtual bool IsReferencing(MaterialConstRef mat) const;
	virtual void AddReferencedMaterials(
		std::unordered_set<const Material *> &referencedMats
	) const;
	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexsreferencedTexs) const;
	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex);

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	MaterialConstRef GetMaterialA() const { return *matA; }
	MaterialConstRef GetMaterialB() const { return *matB; }
	TextureConstRef GetMixFactor() const { return *mixFactor; }

protected:
	virtual void UpdateAvgPassThroughTransparency();

private:
	// Used by Preprocess()
	BSDFEvent GetEventTypesImpl() const;
	bool IsLightSourceImpl() const;
	bool IsDeltaImpl() const;

	void Preprocess();

	MaterialConstPtr matA;
	MaterialConstPtr matB;
	TextureConstPtr mixFactor;

	// Cached values for performance with very large material node trees
	BSDFEvent eventTypes;
	bool isLightSource, isDelta;

};

}

#endif	/* _SLG_MIXMAT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
