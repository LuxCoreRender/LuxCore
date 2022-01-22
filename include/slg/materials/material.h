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

#ifndef _SLG_MATERIAL_H
#define	_SLG_MATERIAL_H

#include <vector>

#include <boost/unordered_set.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/utils/mc.h"
#include "luxrays/utils/properties.h"
#include "slg/bsdf/bsdfevents.h"
#include "slg/bsdf/hitpoint.h"
#include "slg/textures/texture.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/materials/material_types.cl"
}

typedef enum {
	MATTE, MIRROR, GLASS, ARCHGLASS, MIX, NULLMAT, MATTETRANSLUCENT,
	GLOSSY2, METAL2, ROUGHGLASS, VELVET, CLOTH, CARPAINT, ROUGHMATTE,
	ROUGHMATTETRANSLUCENT, GLOSSYTRANSLUCENT, GLOSSYCOATING, DISNEY,
	TWOSIDED,

	// Volumes
	HOMOGENEOUS_VOL, CLEAR_VOL, HETEROGENEOUS_VOL
} MaterialType;

// Material emission direct light sampling type
typedef enum {
	DLS_ENABLED, DLS_DISABLED, DLS_AUTO
} MaterialEmissionDLSType;

class SampleableSphericalFunction;

class Material : public luxrays::NamedObject {
public:
	Material(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump);
	virtual ~Material();

	void SetLightID(const u_int id) { lightID = id; }
	u_int GetLightID() const { return lightID; }
	void SetID(const u_int id) { matID = id; }
	u_int GetID() const { return matID; }
	void SetEmittedGain(const luxrays::Spectrum &v) { emittedGain = v; UpdateEmittedFactor(); }
	const luxrays::Spectrum &GetEmittedGain() const { return emittedGain; }
	void SetEmittedPower(const float v) { emittedPower = v; UpdateEmittedFactor(); }
	float GetEmittedPower() const { return emittedPower; }
	void SetEmittedPowerNormalize(const bool v) { emittedPowerNormalize = v; }
	bool IsEmittedPowerNormalize() { return emittedPowerNormalize; }
	void SetEmittedGainNormalize(const bool v) { emittedGainNormalize = v; }
	bool IsEmittedGainNormalize() { return emittedGainNormalize; }
	void SetEmittedEfficency(const float v) { emittedEfficiency = v; UpdateEmittedFactor(); }
	float GetEmittedEfficency() const { return emittedEfficiency; }
	const luxrays::Spectrum &GetEmittedFactor() const { return emittedFactor; }
	void SetEmittedTheta(const float theta);
	float GetEmittedTheta() const { return emittedTheta; }
	float GetEmittedCosThetaMax() const { return emittedCosThetaMax; }
	void SetEmittedTemperature(const float v) { emittedTemperature = v; UpdateEmittedFactor(); }
	void SetEmittedTemperatureNormalize(const bool v) { emittedNormalizeTemperature = v; UpdateEmittedFactor(); }
	bool IsUsingPrimitiveArea() const { return usePrimitiveArea; }

	virtual MaterialType GetType() const = 0;
	virtual BSDFEvent GetEventTypes() const = 0;

	virtual bool IsLightSource() const {
		return (emittedTex != NULL);
	}
	
	void SetPhotonGIEnabled(const bool v) { isPhotonGIEnabled = v; }
	virtual bool IsPhotonGIEnabled() const { return isPhotonGIEnabled; }
	virtual float GetGlossiness() const { return glossiness; }

	void SetDirectLightSamplingType(const MaterialEmissionDLSType type) { directLightSamplingType = type; }
	MaterialEmissionDLSType GetDirectLightSamplingType() const { return directLightSamplingType; }

	void SetIndirectDiffuseVisibility(const bool visible) { isVisibleIndirectDiffuse = visible; }
	bool IsVisibleIndirectDiffuse() const { return isVisibleIndirectDiffuse; }
	void SetIndirectGlossyVisibility(const bool visible) { isVisibleIndirectGlossy = visible; }
	bool IsVisibleIndirectGlossy() const { return isVisibleIndirectGlossy; }
	void SetIndirectSpecularVisibility(const bool visible) { isVisibleIndirectSpecular = visible; }
	bool IsVisibleIndirectSpecular() const { return isVisibleIndirectSpecular; }

	void SetShadowCatcher(const bool enabled) { isShadowCatcher = enabled; }
	bool IsShadowCatcher() const { return isShadowCatcher; }
	void SetShadowCatcherOnlyInfiniteLights(const bool enabled) { isShadowCatcherOnlyInfiniteLights = enabled; }
	bool IsShadowCatcherOnlyInfiniteLights() const { return (isShadowCatcher && isShadowCatcherOnlyInfiniteLights); }

	void SetHoldout(const bool enabled) { isHoldout = enabled; }
	bool IsHoldout() const { return isHoldout; }

    void SetBumpSampleDistance(const float dist) { bumpSampleDistance = dist; }
    float GetBumpSampleDistance() const { return bumpSampleDistance; }

	virtual bool IsDelta() const { return false; }
	virtual float GetAvgPassThroughTransparency() const { return avgPassThroughTransparency; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent,
		const bool backTracing) const;

	void SetPassThroughShadowTransparency(const luxrays::Spectrum &t) { passThroughShadowTransparency = t; }
	const luxrays::Spectrum &GetPassThroughShadowTransparency() const { return passThroughShadowTransparency; }

	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;
	virtual float GetEmittedRadianceY(const float oneOverPrimitiveArea) const;

	const void SetEmittedImportance(const float imp) { emittedImportance = imp; }
	const float GetEmittedImportance() const { return emittedImportance; }
	const Texture *GetFrontTransparencyTexture() const { return frontTransparencyTex; }
	const Texture *GetBackTransparencyTexture() const { return backTransparencyTex; }
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

	// Albedo() returns the material albedo. It is used for Albedo AOV.
	virtual luxrays::Spectrum Albedo(const HitPoint &hitPoint) const;

	// EvaluateTotal() returns the total reflection given an constant illumination
	// over the hemisphere. It is currently used only by PhotonGICache and
	// BakeCPU render engine.
	//
	// NOTE: this is called rho() in PBRT sources.
	virtual luxrays::Spectrum EvaluateTotal(const HitPoint &hitPoint) const;
	
	// Evaluate() is used to evaluate the material (i.e. get a color plus some
	// related data) knowing the eye and light vector. It used by the
	// path tracer to evaluate the material color when doing direct lighting
	// (i.e. you know where you are coming from and where is the light source).
	// BiDir uses this method also while connecting eye and light path vertices.
	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const = 0;

	// Sample() is used to obtain an outgoing direction (i.e. get an outgoing
	// direction plus some related data) knowing the incoming vector. It is
	// used to extend a path, you know where you are coming from and want to
	// know where to go next. It used by the path tracer to extend eye path and
	// by BiDir to extend both eye and light path.
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const = 0;

	// Pdf() is used to obtain direct and reverse PDFs while knowing the eye
	// and light vector. It is used only by BiDir.
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const = 0;

	// Update any reference to oldMat with newMat (mostly used for updating Mix material)
	// but also to update volume reference (because volumes are just a special kind of
	// materials)
	virtual void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);
	// Return true if the material is referencing the specified material
	virtual bool IsReferencing(const Material *mat) const { return (this == mat); }
	virtual void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const;
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const;
	// Update any reference to oldTex with newTex
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);
	
	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	static std::string MaterialType2String(const MaterialType type);

protected:
	static float ComputeGlossiness(const Texture *t1 = nullptr, const Texture *t2 = nullptr,
			const Texture *t3 = nullptr);

	void UpdateEmittedFactor();
	virtual void UpdateAvgPassThroughTransparency();

	u_int matID, lightID;

	MaterialEmissionDLSType directLightSamplingType;

	float emittedImportance;
	luxrays::Spectrum emittedGain, emittedFactor;
	float emittedPower, emittedEfficiency, emittedTheta, emittedCosThetaMax;
	bool emittedPowerNormalize, emittedGainNormalize;
	float emittedTemperature;
	bool emittedNormalizeTemperature;

	const Texture *frontTransparencyTex;
	const Texture *backTransparencyTex;
	luxrays::Spectrum passThroughShadowTransparency;
	const Texture *emittedTex;
	const Texture *bumpTex;
    float bumpSampleDistance;

	const ImageMap *emissionMap;
	SampleableSphericalFunction *emissionFunc;

	const Volume *interiorVolume, *exteriorVolume;

	float glossiness, avgPassThroughTransparency;

	bool isVisibleIndirectDiffuse, isVisibleIndirectGlossy, isVisibleIndirectSpecular,
		usePrimitiveArea, isShadowCatcher, isShadowCatcherOnlyInfiniteLights, isPhotonGIEnabled,
		isHoldout;
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
