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

#ifndef _SLG_MATERIAL_H
#define	_SLG_MATERIAL_H

#include <vector>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/utils/mc.h"
#include "luxrays/utils/properties.h"
#include "slg/usings.h"
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
class Volume;

class Material : public luxrays::NamedObject
{
public:
	Material(
		TextureConstPtr frontTransp,
		TextureConstPtr backTransp,
		TextureConstPtr emitted,
		TextureConstPtr bump
	);
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
		return static_cast<bool>(emittedTex);
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

	void SetPassThroughShadowTransparency(const luxrays::Spectrum &t) {
		passThroughShadowTransparency = t;
	}
	const luxrays::Spectrum &GetPassThroughShadowTransparency() const {
		return passThroughShadowTransparency;
	}
	void SetPassThroughShadowTransparencyOverride(const bool &o) {
		passThroughShadowTransparencyOverride = o;
	}
	const bool &GetPassThroughShadowTransparencyOverride() const {
		return passThroughShadowTransparencyOverride;
	}

	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;
	virtual float GetEmittedRadianceY(const float oneOverPrimitiveArea) const;

	const void SetEmittedImportance(const float imp) { emittedImportance = imp; }
	const float GetEmittedImportance() const { return emittedImportance; }
	auto GetFrontTransparencyTexture() const { return frontTransparencyTex; }
	auto GetBackTransparencyTexture() const { return backTransparencyTex; }
	auto GetEmitTexture() const { return emittedTex; }
	auto GetBumpTexture() const { return bumpTex; }

	void SetEmissionMap(ImageMapConstRef map);
	ImageMapConstRef GetEmissionMap() const { return *emissionMap; }
	SampleableSphericalFunctionRPtr GetEmissionFunc() const { return emissionFunc; }

	// MixMaterial can have multiple volumes assigned and needs the passThroughEvent
	// information to be able to return the correct volume
	void SetInteriorVolume(VolumeConstRef vol);
	void SetInteriorVolume(VolumeConstPtr vol);
	virtual VolumeConstPtr GetInteriorVolume(
		const HitPoint &hitPoint,
		const float passThroughEvent
	) const;
	void SetExteriorVolume(VolumeConstRef vol);
	void SetExteriorVolume(VolumeConstPtr vol);
	virtual VolumeConstPtr GetExteriorVolume(
		const HitPoint &hitPoint,
		const float passThroughEvent
	) const;

	VolumeConstPtr GetInteriorVolume() const;
	VolumeConstPtr GetExteriorVolume() const;

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
	virtual luxrays::Spectrum Evaluate(
		const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir,
		const luxrays::Vector &localEyeDir,
		BSDFEvent *event,
		float *directPdfW = NULL,
		float *reversePdfW = NULL
	) const = 0;

	// Sample() is used to obtain an outgoing direction (i.e. get an outgoing
	// direction plus some related data) knowing the incoming vector. It is
	// used to extend a path, you know where you are coming from and want to
	// know where to go next. It used by the path tracer to extend eye path and
	// by BiDir to extend both eye and light path.
	virtual luxrays::Spectrum Sample(
		const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir,
		luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW,
		BSDFEvent *event
	) const = 0;

	// Pdf() is used to obtain direct and reverse PDFs while knowing the eye
	// and light vector. It is used only by BiDir.
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const = 0;

	// Update any reference to oldMat with newMat (mostly used for updating Mix material)
	// but also to update volume reference (because volumes are just a special kind of
	// materials)
	virtual void UpdateMaterialReferences(MaterialConstRef oldMat, MaterialRef newMat);
	// Return true if the material is referencing the specified material
	virtual bool IsReferencing(MaterialConstRef mat) const {
		return (this == &mat);
	}
	virtual void AddReferencedMaterials(
		std::unordered_set<const Material *> &referencedMats
	) const;
	virtual void AddReferencedTextures(
		std::unordered_set<const Texture *>  &referencedTexs
	) const;
	virtual void AddReferencedImageMaps(
		std::unordered_set<const ImageMap *> &referencedImgMaps
	) const;
	// Update any reference to oldTex with newTex
	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex);

	virtual luxrays::PropertiesUPtr ToProperties(
		const ImageMapCache &imgMapCache, const bool useRealFileName
	) const;

	static std::string MaterialType2String(const MaterialType type);

protected:
	static float ComputeGlossiness(
		TextureConstPtr t1 = nullptr,
		TextureConstPtr t2 = nullptr,
		TextureConstPtr t3 = nullptr
	);

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

	TextureConstPtr frontTransparencyTex;
	TextureConstPtr backTransparencyTex;
	luxrays::Spectrum passThroughShadowTransparency;
	bool passThroughShadowTransparencyOverride;
	TextureConstPtr emittedTex;
	TextureConstPtr bumpTex;
    float bumpSampleDistance;

	ImageMapConstPtr emissionMap;
	SampleableSphericalFunctionUPtr emissionFunc;

	VolumeConstPtr interiorVolume, exteriorVolume;

	float glossiness, avgPassThroughTransparency;

	bool isVisibleIndirectDiffuse, isVisibleIndirectGlossy, isVisibleIndirectSpecular,
		usePrimitiveArea, isShadowCatcher, isShadowCatcherOnlyInfiniteLights, isPhotonGIEnabled,
		isHoldout;
};

//------------------------------------------------------------------------------
// IOR utilities
//------------------------------------------------------------------------------

extern float ExtractExteriorIors(const HitPoint &hitPoint, TextureConstPtr exteriorIor);
extern float ExtractInteriorIors(const HitPoint &hitPoint, TextureConstPtr interiorIor);

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

}  // namespace slg


#endif	/* _SLG_MATERIAL_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
