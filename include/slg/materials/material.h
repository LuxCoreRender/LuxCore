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

#ifndef _SLG_MATERIAL_H
#define	_SLG_MATERIAL_H

#include <vector>

#include <boost/unordered_set.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/color/color.h"
#include "luxrays/utils/mc.h"
#include "luxrays/core/geometry/point.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/bsdf/bsdfevents.h"
#include "slg/bsdf/hitpoint.h"
#include "slg/textures/texture.h"
#include "slg/textures/fresnel/fresneltexture.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/materials/material_types.cl"
}

typedef enum {
	MATTE, MIRROR, GLASS, ARCHGLASS, MIX, NULLMAT, MATTETRANSLUCENT,
	GLOSSY2, METAL2, ROUGHGLASS, VELVET, CLOTH, CARPAINT, ROUGHMATTE,
	ROUGHMATTETRANSLUCENT, GLOSSYTRANSLUCENT, GLOSSYCOATING,

	// Volumes
	HOMOGENEOUS_VOL, CLEAR_VOL, HETEROGENEOUS_VOL,

	// The following types are used (in PATHOCL CompiledScene class) only to
	// recognize the usage of some specific material option
	GLOSSY2_ANISOTROPIC, GLOSSY2_ABSORPTION, GLOSSY2_INDEX, GLOSSY2_MULTIBOUNCE,
	GLOSSYTRANSLUCENT_ANISOTROPIC, GLOSSYTRANSLUCENT_ABSORPTION, GLOSSYTRANSLUCENT_INDEX, GLOSSYTRANSLUCENT_MULTIBOUNCE,
	GLOSSYCOATING_ANISOTROPIC, GLOSSYCOATING_ABSORPTION, GLOSSYCOATING_INDEX, GLOSSYCOATING_MULTIBOUNCE,
	METAL2_ANISOTROPIC, ROUGHGLASS_ANISOTROPIC
} MaterialType;

class Material {
public:
	Material(const Texture *transp, const Texture *emitted, const Texture *bump);
	virtual ~Material();

	virtual std::string GetName() const { return "material-" + boost::lexical_cast<std::string>(this); }
	void SetLightID(const u_int id) { lightID = id; }
	u_int GetLightID() const { return lightID; }
	void SetID(const u_int id) { matID = id; }
	u_int GetID() const { return matID; }
	void SetEmittedGain(const luxrays::Spectrum &v) { emittedGain = v; UpdateEmittedFactor(); }
	luxrays::Spectrum GetEmittedGain() const { return emittedGain; }
	void SetEmittedPower(const float v) { emittedPower = v; UpdateEmittedFactor(); }
	float GetEmittedPower() const { return emittedPower; }
	void SetEmittedEfficency(const float v) { emittedEfficency = v; UpdateEmittedFactor(); }
	float GetEmittedEfficency() const { return emittedEfficency; }
	const luxrays::Spectrum &GetEmittedFactor() const { return emittedFactor; }
	bool IsUsingPrimitiveArea() const { return usePrimitiveArea; }

	virtual MaterialType GetType() const = 0;
	virtual BSDFEvent GetEventTypes() const = 0;

	virtual bool IsLightSource() const {
		return (emittedTex != NULL);
	}
	virtual bool HasBumpTex() const { 
		return (bumpTex != NULL);
	}

	void SetIndirectDiffuseVisibility(const bool visible) { isVisibleIndirectDiffuse = visible; }
	bool IsVisibleIndirectDiffuse() const { return isVisibleIndirectDiffuse; }
	void SetIndirectGlossyVisibility(const bool visible) { isVisibleIndirectGlossy = visible; }
	bool IsVisibleIndirectGlossy() const { return isVisibleIndirectGlossy; }
	void SetIndirectSpecularVisibility(const bool visible) { isVisibleIndirectSpecular = visible; }
	bool IsVisibleIndirectSpecular() const { return isVisibleIndirectSpecular; }

    void SetBumpSampleDistance(const float dist) { bumpSampleDistance = dist; }
    float GetBumpSampleDistance() const { return bumpSampleDistance; }

	virtual bool IsDelta() const { return false; }
	virtual bool IsPassThrough() const { return false; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent) const;

	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;
	virtual float GetEmittedRadianceY() const;

	const void SetSamples(const int sampleCount) { samples = sampleCount; }
	const int GetSamples() const { return samples; }
	const void SetEmittedSamples(const int sampleCount) { emittedSamples = sampleCount; }
	const int GetEmittedSamples() const { return emittedSamples; }
	const void SetEmittedImportance(const float imp) { emittedImportance = imp; }
	const float GetEmittedImportance() const { return emittedImportance; }
	const Texture *GetTransparencyTexture() const { return transparencyTex; }
	const Texture *GetEmitTexture() const { return emittedTex; }
	const Texture *GetBumpTexture() const { return bumpTex; }
	void SetEmissionMap(const ImageMap *map);
	const ImageMap *GetEmissionMap() const { return emissionMap; }
	const SampleableSphericalFunction *GetEmissionFunc() const { return emissionFunc; }

	// MixMaterial can have multiple volumes assigned and needs the passThroughEvent
	// information to be able to return the correct volume
	void SetInteriorVolume(const Volume *vol) { interiorVolume = vol; }
	virtual const Volume *GetInteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const { return interiorVolume; }
	void SetExteriorVolume(const Volume *vol) { exteriorVolume = vol; }
	virtual const Volume *GetExteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const { return exteriorVolume; }	

	const Volume *GetInteriorVolume() const { return interiorVolume; }
	const Volume *GetExteriorVolume() const { return exteriorVolume; }	
	
	virtual void Bump(HitPoint *hitPoint) const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const = 0;

	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent = ALL) const = 0;

	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const = 0;

	// Update any reference to oldMat with newMat (mostly used for updating Mix material)
	// but also to update volume reference (because volumes are just a special kind of
	// materials)
	virtual void UpdateMaterialReferences(Material *oldMat, Material *newMat);
	// Return true if the material is referencing the specified material
	virtual bool IsReferencing(const Material *mat) const { return (this == mat); }
	virtual void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const;
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const;
	// Update any reference to oldTex with newTex
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);
	
	virtual luxrays::Properties ToProperties() const;

	static std::string MaterialType2String(const MaterialType type);

protected:
	void UpdateEmittedFactor();

	u_int matID, lightID;

	int samples, emittedSamples;
	float emittedImportance;
	luxrays::Spectrum emittedGain, emittedFactor;
	float emittedPower, emittedEfficency;

	const Texture *transparencyTex;
	const Texture *emittedTex;
	const Texture *bumpTex;
    float bumpSampleDistance;

	const ImageMap *emissionMap;
	SampleableSphericalFunction *emissionFunc;

	const Volume *interiorVolume, *exteriorVolume;

	bool isVisibleIndirectDiffuse, isVisibleIndirectGlossy, isVisibleIndirectSpecular, usePrimitiveArea;
};

//------------------------------------------------------------------------------
// IOR utilities
//------------------------------------------------------------------------------

extern float ExtractExteriorIors(const HitPoint &hitPoint, const Texture *exteriorIor);
extern float ExtractInteriorIors(const HitPoint &hitPoint, const Texture *interiorIor);

//------------------------------------------------------------------------------
// Coating absorption
//------------------------------------------------------------------------------

extern luxrays::Spectrum CoatingAbsorption(const float cosi, const float coso,
	const luxrays::Spectrum &alpha, const float depth);

//------------------------------------------------------------------------------
// SchlickDistribution related functions
//------------------------------------------------------------------------------

extern float SchlickDistribution_SchlickZ(const float roughness, const float cosNH);
extern float SchlickDistribution_SchlickA(const luxrays::Vector &H, const float anisotropy);
extern float SchlickDistribution_D(const float roughness, const luxrays::Vector &wh, const float anisotropy);
extern float SchlickDistribution_SchlickG(const float roughness, const float costheta);
extern void SchlickDistribution_SampleH(const float roughness, const float anisotropy,
	const float u0, const float u1, luxrays::Vector *wh, float *d, float *pdf);
extern float SchlickDistribution_Pdf(const float roughness, const luxrays::Vector &wh, const float anisotropy);
extern float SchlickDistribution_G(const float roughness, const luxrays::Vector &localFixedDir,
	const luxrays::Vector &localSampledDir);

//------------------------------------------------------------------------------
// SchlickBSDF related functions
//------------------------------------------------------------------------------
extern float SchlickBSDF_CoatingWeight(const luxrays::Spectrum &ks, const luxrays::Vector &localFixedDir);
extern luxrays::Spectrum SchlickBSDF_CoatingF(const bool fromLight, const luxrays::Spectrum &ks, const float roughness, const float anisotropy, const bool mbounce,
	const luxrays::Vector &localFixedDir,	const luxrays::Vector &localSampledDir);
extern luxrays::Spectrum SchlickBSDF_CoatingSampleF(const bool fromLight, const luxrays::Spectrum ks,
	const float roughness, const float anisotropy, const bool mbounce, const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
	float u0, float u1, float *pdf);
extern float SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,
	const luxrays::Vector &localFixedDir, const luxrays::Vector &localSampledDir);

}

#endif	/* _SLG_MATERIAL_H */
