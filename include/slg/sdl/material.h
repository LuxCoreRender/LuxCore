/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/spectrum.h"
#include "luxrays/utils/mc.h"
#include "slg/sdl/bsdfevents.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/hitpoint.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/sdl/material_types.cl"
}

class Scene;

typedef enum {
	MATTE, MIRROR, GLASS, METAL, ARCHGLASS, MIX, NULLMAT, MATTETRANSLUCENT,
	GLOSSY2, METAL2, ROUGHGLASS,

	// The following types are used (in PATHOCL CompiledScene class) only to
	// recognize the usage of some specific material option
	GLOSSY2_ANISOTROPIC, GLOSSY2_ABSORPTION, GLOSSY2_INDEX, GLOSSY2_MULTIBOUNCE,
	METAL2_ANISOTROPIC, ROUGHGLASS_ANISOTROPIC
} MaterialType;

class Material {
public:
	Material(const Texture *emitted, const Texture *bump, const Texture *normal) :
		matID(0), lightID(0), samples(-1), emittedSamples(-1),
		emittedGain(1.f), emittedPower(0.f), emittedEfficency(0.f),
		emittedTex(emitted), bumpTex(bump), normalTex(normal),
		isVisibleIndirectDiffuse(true), isVisibleIndirectGlossy(true), isVisibleIndirectSpecular(true) {
		UpdateEmittedFactor();
	}
	virtual ~Material() { }

	std::string GetName() const { return "material-" + boost::lexical_cast<std::string>(this); }
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
	virtual bool HasNormalTex() const { 
		return (normalTex != NULL);
	}

	void SetIndirectDiffuseVisibility(const bool visible) { isVisibleIndirectDiffuse = visible; }
	bool IsVisibleIndirectDiffuse() const { return isVisibleIndirectDiffuse; }
	void SetIndirectGlossyVisibility(const bool visible) { isVisibleIndirectGlossy = visible; }
	bool IsVisibleIndirectGlossy() const { return isVisibleIndirectGlossy; }
	void SetIndirectSpecularVisibility(const bool visible) { isVisibleIndirectSpecular = visible; }
	bool IsVisibleIndirectSpecular() const { return isVisibleIndirectSpecular; }

	virtual bool IsDelta() const { return false; }
	virtual bool IsPassThrough() const { return false; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent) const {
		return luxrays::Spectrum(0.f);
	}

	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;
	virtual float GetEmittedRadianceY() const {
		if (emittedTex)
			return emittedTex->Y();
		else
			return 0.f;
	}
	virtual luxrays::UV GetBumpTexValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetNormalTexValue(const HitPoint &hitPoint) const {
		if (normalTex)
			return normalTex->GetSpectrumValue(hitPoint);
		else
			return luxrays::Spectrum();
	}

	const void SetSamples(const int sampleCount) { samples = sampleCount; }
	const int GetSamples() const { return samples; }
	const void SetEmittedSamples(const int sampleCount) { emittedSamples = sampleCount; }
	const int GetEmittedSamples() const { return emittedSamples; }
	const Texture *GetEmitTexture() const { return emittedTex; }
	const Texture *GetBumpTexture() const { return bumpTex; }
	const Texture *GetNormalTexture() const { return normalTex; }

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
	virtual void UpdateMaterialReferences(const Material *oldMat, const Material *newMat) { }
	// Return true if the material is referencing the specified material
	virtual bool IsReferencing(const Material *mat) const { return (this == mat); }
	virtual void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
		referencedMats.insert(this);
	}
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		if (emittedTex)
			emittedTex->AddReferencedTextures(referencedTexs);
		if (bumpTex)
			bumpTex->AddReferencedTextures(referencedTexs);
		if (normalTex)
			normalTex->AddReferencedTextures(referencedTexs);
	}
	// Update any reference to oldTex with newTex
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (emittedTex == oldTex)
			emittedTex = newTex;
		if (bumpTex == oldTex)
			bumpTex = newTex;
		if (normalTex == oldTex)
			normalTex = newTex;
	}

	virtual luxrays::Properties ToProperties() const;

protected:
	void UpdateEmittedFactor();

	u_int matID, lightID;

	int samples, emittedSamples;
	luxrays::Spectrum emittedGain, emittedFactor;
	float emittedPower, emittedEfficency;
	const Texture *emittedTex;
	const Texture *bumpTex;
	const Texture *normalTex;

	bool isVisibleIndirectDiffuse, isVisibleIndirectGlossy, isVisibleIndirectSpecular, usePrimitiveArea;
};

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

class MaterialDefinitions {
public:
	MaterialDefinitions();
	~MaterialDefinitions();

	bool IsMaterialDefined(const std::string &name) const {
		return (matsByName.count(name) > 0);
	}
	void DefineMaterial(const std::string &name, Material *m);

	void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	Material *GetMaterial(const std::string &name);
	Material *GetMaterial(const u_int index) {
		return mats[index];
	}
	u_int GetMaterialIndex(const std::string &name);
	u_int GetMaterialIndex(const Material *m) const;

	u_int GetSize() const { return static_cast<u_int>(mats.size()); }
	std::vector<std::string> GetMaterialNames() const;

	void DeleteMaterial(const std::string &name);
  
private:
	std::vector<Material *> mats;
	boost::unordered_map<std::string, Material *> matsByName;
};

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

class MatteMaterial : public Material {
public:
	MatteMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *col) : Material(emitted, bump, normal), Kd(col) { }

	virtual MaterialType GetType() const { return MATTE; }
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

private:
	const Texture *Kd;
};

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

class MirrorMaterial : public Material {
public:
	MirrorMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
		const Texture *refl) : Material(emitted, bump, normal), Kr(refl) { }

	virtual MaterialType GetType() const { return MIRROR; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT; };

	virtual bool IsDelta() const { return true; }

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKr() const { return Kr; }

private:
	const Texture *Kr;
};

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

class GlassMaterial : public Material {
public:
	GlassMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *trans,
			const Texture *outsideIorFact, const Texture *iorFact) :
			Material(emitted, bump, normal),
			Kr(refl), Kt(trans), ousideIor(outsideIorFact), ior(iorFact) { }

	virtual MaterialType GetType() const { return GLASS; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	virtual bool IsDelta() const { return true; }

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetOutsideIOR() const { return ousideIor; }
	const Texture *GetIOR() const { return ior; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *ousideIor;
	const Texture *ior;
};

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

class ArchGlassMaterial : public Material {
public:
	ArchGlassMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *trans,
			const Texture *outsideIorFact, const Texture *iorFact) :
			Material(emitted, bump, normal),
			Kr(refl), Kt(trans), ousideIor(outsideIorFact), ior(iorFact) { }

	virtual MaterialType GetType() const { return ARCHGLASS; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	virtual bool IsDelta() const { return true; }
	virtual bool IsShadowTransparent() const { return true; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint, const luxrays::Vector &localFixedDir,
		const float passThroughEvent) const;

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetOutsideIOR() const { return ousideIor; }
	const Texture *GetIOR() const { return ior; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *ousideIor;
	const Texture *ior;
};

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

class MetalMaterial : public Material {
public:
	MetalMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *exp) : Material(emitted, bump, normal),
			Kr(refl), exponent(exp) { }

	virtual MaterialType GetType() const { return METAL; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Texture *GetKr() const { return Kr; }
	const Texture *GetExp() const { return exponent; }

private:
	static luxrays::Vector GlossyReflection(const luxrays::Vector &localFixedDir, const float exponent,
			const float u0, const float u1);

	const Texture *Kr;
	const Texture *exponent;
};

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

class MixMaterial : public Material {
public:
	MixMaterial(const Texture *bump, const Texture *normal,
			Material *mA, Material *mB, const Texture *mix) :
			Material(NULL, bump, normal), matA(mA), matB(mB), mixFactor(mix) { }

	virtual MaterialType GetType() const { return MIX; }
	virtual BSDFEvent GetEventTypes() const { return (matA->GetEventTypes() | matB->GetEventTypes()); };

	virtual bool IsLightSource() const {
		return (matA->IsLightSource() || matB->IsLightSource());
	}
	virtual bool HasBumpTex() const { 
		return (matA->HasBumpTex() || matB->HasBumpTex());
	}
	virtual bool HasNormalTex() const { 
		return (matA->HasNormalTex() || matB->HasNormalTex());
	}

	virtual bool IsDelta() const {
		return (matA->IsDelta() && matB->IsDelta());
	}
	virtual bool IsPassThrough() const {
		return (matA->IsPassThrough() || matB->IsPassThrough());
	}
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent) const;

	virtual float GetEmittedRadianceY() const;
	virtual luxrays::Spectrum GetEmittedRadiance(const HitPoint &hitPoint,
		const float oneOverPrimitiveArea) const;
	virtual luxrays::UV GetBumpTexValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetNormalTexValue(const HitPoint &hitPoint) const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const;
	void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void UpdateMaterialReferences(Material *oldMat, Material *newMat);
	virtual bool IsReferencing(const Material *mat) const;
	virtual void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const;
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties() const;

	const Material *GetMaterialA() const { return matA; }
	const Material *GetMaterialB() const { return matB; }
	const Texture *GetMixFactor() const { return mixFactor; }

private:
	Material *matA;
	Material *matB;
	const Texture *mixFactor;
};

//------------------------------------------------------------------------------
// Null material
//------------------------------------------------------------------------------

class NullMaterial : public Material {
public:
	NullMaterial() : Material(NULL, NULL, NULL) { }

	virtual MaterialType GetType() const { return NULLMAT; }
	virtual BSDFEvent GetEventTypes() const { return SPECULAR | TRANSMIT; };

	virtual bool IsDelta() const { return true; }
	virtual bool IsPassThrough() const { return true; }
	virtual luxrays::Spectrum GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent) const { return luxrays::Spectrum(1.f); }

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
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	virtual luxrays::Properties ToProperties() const;
};

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

class MatteTranslucentMaterial : public Material {
public:
	MatteTranslucentMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *trans) : Material(emitted, bump, normal),
			Kr(refl), Kt(trans) { }

	virtual MaterialType GetType() const { return MATTETRANSLUCENT; }
	virtual BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT | TRANSMIT; };

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

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }

private:
	const Texture *Kr;
	const Texture *Kt;
};

//------------------------------------------------------------------------------
// Glossy2 material
//------------------------------------------------------------------------------

class Glossy2Material : public Material {
public:
	Glossy2Material(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *kd, const Texture *ks, const Texture *u, const Texture *v,
			const Texture *ka, const Texture *d, const Texture *i, const bool mbounce) :
			Material(emitted, bump, normal), Kd(kd), Ks(ks), nu(u), nv(v),
			Ka(ka), depth(d), index(i), multibounce(mbounce) { }

	virtual MaterialType GetType() const { return GLOSSY2; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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
	const Texture *GetKs() const { return Ks; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }
	const Texture *GetKa() const { return Ka; }
	const Texture *GetDepth() const { return depth; }
	const Texture *GetIndex() const { return index; }
	const bool IsMultibounce() const { return multibounce; }

private:
	float SchlickBSDF_CoatingWeight(const luxrays::Spectrum &ks, const luxrays::Vector &localFixedDir) const;
	luxrays::Spectrum SchlickBSDF_CoatingF(const bool fromLight, const luxrays::Spectrum &ks, const float roughness, const float anisotropy,
		const luxrays::Vector &localFixedDir,	const luxrays::Vector &localSampledDir) const;
	luxrays::Spectrum SchlickBSDF_CoatingSampleF(const bool fromLight, const luxrays::Spectrum ks,
		const float roughness, const float anisotropy, const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		float u0, float u1, float *pdf) const;
	float SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,
		const luxrays::Vector &localFixedDir, const luxrays::Vector &localSampledDir) const;
	luxrays::Spectrum SchlickBSDF_CoatingAbsorption(const float cosi, const float coso,
		const luxrays::Spectrum &alpha, const float depth) const;

	const Texture *Kd;
	const Texture *Ks;
	const Texture *nu;
	const Texture *nv;
	const Texture *Ka;
	const Texture *depth;
	const Texture *index;
	const bool multibounce;
};

//------------------------------------------------------------------------------
// Metal2 material
//------------------------------------------------------------------------------

class Metal2Material : public Material {
public:
	Metal2Material(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *nn, const Texture *kk, const Texture *u, const Texture *v) :
			Material(emitted, bump, normal), n(nn), k(kk), nu(u), nv(v) { }

	virtual MaterialType GetType() const { return METAL2; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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

	const Texture *GetN() const { return n; }
	const Texture *GetK() const { return k; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }

private:
	const Texture *n;
	const Texture *k;
	const Texture *nu;
	const Texture *nv;
};

//------------------------------------------------------------------------------
// RoughGlass material
//------------------------------------------------------------------------------

class RoughGlassMaterial : public Material {
public:
	RoughGlassMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *trans,
			const Texture *outsideIorFact, const Texture *iorFact,
			const Texture *u, const Texture *v) :
			Material(emitted, bump, normal), Kr(refl), Kt(trans),
			ousideIor(outsideIorFact), ior(iorFact), nu(u), nv(v) { }

	virtual MaterialType GetType() const { return ROUGHGLASS; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT | TRANSMIT; };

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

	const Texture *GetKr() const { return Kr; }
	const Texture *GetKt() const { return Kt; }
	const Texture *GetOutsideIOR() const { return ousideIor; }
	const Texture *GetIOR() const { return ior; }
	const Texture *GetNu() const { return nu; }
	const Texture *GetNv() const { return nv; }

private:
	const Texture *Kr;
	const Texture *Kt;
	const Texture *ousideIor;
	const Texture *ior;
	const Texture *nu;
	const Texture *nv;
};

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
// Fresnel related functions
//------------------------------------------------------------------------------

extern luxrays::Spectrum FresnelSlick_Evaluate(const luxrays::Spectrum &ks, const float cosi);
extern luxrays::Spectrum FresnelGeneral_Evaluate(const luxrays::Spectrum &eta, const luxrays::Spectrum &k, const float cosi);
extern luxrays::Spectrum FresnelCauchy_Evaluate(const float eta, const float cosi);

}

#endif	/* _SLG_MATERIAL_H */
