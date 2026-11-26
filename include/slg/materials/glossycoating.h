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

#ifndef _SLG_GLOSSYCOATTINGMAT_H
#define	_SLG_GLOSSYCOATTINGMAT_H

#include "slg/materials/material.h"
#include "slg/volumes/volume.h"

namespace slg {

//------------------------------------------------------------------------------
// GlossyCoating material
//------------------------------------------------------------------------------

class GlossyCoatingMaterial : public Material {
public:
	GlossyCoatingMaterial(
		TextureConstPtr frontTransp,
		TextureConstPtr backTransp,
		TextureConstPtr emitted,
		TextureConstPtr bump,
		MaterialConstPtr mB,
		TextureConstPtr ks,
		TextureConstPtr u,
		TextureConstPtr v,
		TextureConstPtr ka,
		TextureConstPtr d,
		TextureConstPtr i,
		const bool mbounce
	);

	virtual MaterialType GetType() const { return GLOSSYCOATING; }
	virtual BSDFEvent GetEventTypes() const { return (GLOSSY | REFLECT | matBase->GetEventTypes()); };

	virtual bool IsLightSource() const {
		return (Material::IsLightSource() || matBase->IsLightSource());
	}

	virtual bool IsDelta() const {
		return matBase->IsDelta();
	}
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

	auto GetMaterialBase() const { return matBase; }
	auto GetKs() const { return Ks; }
	auto GetNu() const { return nu; }
	auto GetNv() const { return nv; }
	auto GetKa() const { return Ka; }
	auto GetDepth() const { return depth; }
	auto GetIndex() const { return index; }
	const bool IsMultibounce() const { return multibounce; }

protected:
	virtual void UpdateAvgPassThroughTransparency();

private:
	MaterialConstPtr matBase;
	TextureConstPtr Ks;
	TextureConstPtr nu;
	TextureConstPtr nv;
	TextureConstPtr Ka;
	TextureConstPtr depth;
	TextureConstPtr index;
	const bool multibounce;
};

}

#endif	/* _SLG_GLOSSYCOATTINGMAT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
