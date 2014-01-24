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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/frame.h"
#include "slg/sdl/material.h"
#include "slg/sdl/bsdf.h"
#include "slg/sdl/texture.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Material
//------------------------------------------------------------------------------

void Material::SetEmissionMap(const ImageMap *map) {
	emissionMap = map;
	delete emissionFunc;
	if (emissionMap)
		emissionFunc = new SampleableSphericalFunction(new ImageMapSphericalFunction(emissionMap));
	else
		emissionFunc = NULL;
}

Spectrum Material::GetEmittedRadiance(const HitPoint &hitPoint, const float oneOverPrimitiveArea) const {
	if (emittedTex) {
		return (emittedFactor * (usePrimitiveArea ? oneOverPrimitiveArea : 1.f)) *
				emittedTex->GetSpectrumValue(hitPoint);
	} else
		return luxrays::Spectrum();
}

UV Material::GetBumpTexValue(const HitPoint &hitPoint) const {
	if (bumpTex) {
		const luxrays::UV &dudv = bumpTex->GetDuDv();

		const float b0 = bumpTex->GetFloatValue(hitPoint);

		float dbdu;
		if (dudv.u > 0.f) {
			// This is a simple trick. The correct code would require true differential information.
			HitPoint tmpHitPoint = hitPoint;
			tmpHitPoint.p.x += dudv.u;
			tmpHitPoint.uv.u += dudv.u;
			const float bu = bumpTex->GetFloatValue(tmpHitPoint);

			dbdu = (bu - b0) / dudv.u;
		} else
			dbdu = 0.f;

		float dbdv;
		if (dudv.v > 0.f) {
			// This is a simple trick. The correct code would require true differential information.
			HitPoint tmpHitPoint = hitPoint;
			tmpHitPoint.p.y += dudv.v;
			tmpHitPoint.uv.v += dudv.v;
			const float bv = bumpTex->GetFloatValue(tmpHitPoint);

			dbdv = (bv - b0) / dudv.v;
		} else
			dbdv = 0.f;

		return luxrays::UV(dbdu, dbdv);
	} else
		return luxrays::UV();
}

Properties Material::ToProperties() const {
	luxrays::Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".emission.gain")(emittedGain));
	props.Set(Property("scene.materials." + name + ".emission.power")(emittedPower));
	props.Set(Property("scene.materials." + name + ".emission.efficency")(emittedEfficency));
	props.Set(Property("scene.materials." + name + ".emission.samples")(emittedSamples));
	if (emittedTex)
		props.Set(Property("scene.materials." + name + ".emission")(emittedTex->GetName()));
	if (bumpTex)
		props.Set(Property("scene.materials." + name + ".bumptex")(bumpTex->GetName()));
	if (normalTex)
		props.Set(Property("scene.materials." + name + ".normaltex")(normalTex->GetName()));

	props.Set(Property("scene.materials." + name + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse));
	props.Set(Property("scene.materials." + name + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy));
	props.Set(Property("scene.materials." + name + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular));

	return props;
}

void Material::UpdateEmittedFactor() {
	if (emittedTex) {
		emittedFactor = emittedGain * (emittedPower * emittedEfficency / (M_PI * emittedTex->Y()));
		if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN()) {
			emittedFactor = emittedGain;
			usePrimitiveArea = false;
		} else
			usePrimitiveArea = true;
	} else {
		emittedFactor = emittedGain;
		usePrimitiveArea = false;
	}
}

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

MaterialDefinitions::MaterialDefinitions() { }

MaterialDefinitions::~MaterialDefinitions() {
	BOOST_FOREACH(Material *m, mats)
		delete m;
}

void MaterialDefinitions::DefineMaterial(const string &name, Material *newMat) {
	if (IsMaterialDefined(name)) {
		const Material *oldMat = GetMaterial(name);

		// Update name/material definition
		const u_int index = GetMaterialIndex(name);
		mats[index] = newMat;
		matsByName.erase(name);
		matsByName.insert(make_pair(name, newMat));

		// Update all possible reference to old material with the new one
		BOOST_FOREACH(Material *mat, mats)
			mat->UpdateMaterialReferences(oldMat, newMat);

		// Delete old material
		delete oldMat;
	} else {
		// Add the new material
		mats.push_back(newMat);
		matsByName.insert(make_pair(name, newMat));
	}
}

void MaterialDefinitions::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	BOOST_FOREACH(Material *mat, mats)
		mat->UpdateTextureReferences(oldTex, newTex);
}

Material *MaterialDefinitions::GetMaterial(const string &name) {
	// Check if the material has been already defined
	boost::unordered_map<string, Material *>::const_iterator it = matsByName.find(name);

	if (it == matsByName.end())
		throw runtime_error("Reference to an undefined material: " + name);
	else
		return it->second;
}

u_int MaterialDefinitions::GetMaterialIndex(const string &name) {
	return GetMaterialIndex(GetMaterial(name));
}

u_int MaterialDefinitions::GetMaterialIndex(const Material *m) const {
	for (u_int i = 0; i < mats.size(); ++i) {
		if (m == mats[i])
			return i;
	}

	throw runtime_error("Reference to an undefined material: " + boost::lexical_cast<string>(m));
}

vector<string> MaterialDefinitions::GetMaterialNames() const {
	vector<string> names;
	names.reserve(mats.size());
	for (boost::unordered_map<string, Material *>::const_iterator it = matsByName.begin(); it != matsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void MaterialDefinitions::DeleteMaterial(const string &name) {
	const u_int index = GetMaterialIndex(name);
	mats.erase(mats.begin() + index);
	matsByName.erase(name);
}

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

Spectrum MatteMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);

	*event = DIFFUSE | REFLECT;
	return Kd->GetSpectrumValue(hitPoint).Clamp() * (INV_PI * fabsf(localLightDir.z));
}

Spectrum MatteMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);

	*absCosSampledDir = fabsf(localSampledDir->z);
	if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*event = DIFFUSE | REFLECT;
	if (hitPoint.fromLight)
		return Kd->GetSpectrumValue(hitPoint).Clamp() * fabsf(localFixedDir.z / localSampledDir->z);
	else
		return Kd->GetSpectrumValue(hitPoint).Clamp();
}

void MatteMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);
}

void MatteMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kd->AddReferencedTextures(referencedTexs);
}

void MatteMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kd == oldTex)
		Kd = newTex;
}

Properties MatteMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("matte"));
	props.Set(Property("scene.materials." + name + ".kd")(Kd->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

Spectrum MirrorMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum MirrorMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (SPECULAR | REFLECT)))
		return Spectrum();

	*event = SPECULAR | REFLECT;

	*localSampledDir = Vector(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
	*pdfW = 1.f;

	*absCosSampledDir = fabsf(localSampledDir->z);
	return Kr->GetSpectrumValue(hitPoint).Clamp();
}

void MirrorMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
}

void MirrorMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
}

Properties MirrorMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("mirror"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

Spectrum GlassMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum GlassMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & SPECULAR))
		return Spectrum();

	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp();

	const bool isKtBlack = kt.Black();
	const bool isKrBlack = kr.Black();
	if (isKtBlack && isKrBlack)
		return Spectrum();

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = ousideIor->GetFloatValue(hitPoint);
	const float nt = ior->GetFloatValue(hitPoint);
	const float ntc = nt / nc;
	const float eta = entering ? (nc / nt) : ntc;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return Spectrum();
	}

	Spectrum result;
	if (passThroughEvent < threshold) {
		// Transmit
	
		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return Spectrum();

		const float cost = sqrtf(Max(0.f, 1.f - sint2)) * (entering ? -1.f : 1.f);
		*localSampledDir = Vector(-eta * localFixedDir.x, -eta * localFixedDir.y, cost);
		*absCosSampledDir = fabsf(CosTheta(*localSampledDir));

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;

		if (!hitPoint.fromLight)
			result = (Spectrum(1.f) - FresnelCauchy_Evaluate(ntc, cost)) * eta2;
		else
			result = (Spectrum(1.f) - FresnelCauchy_Evaluate(ntc, costheta)) * fabsf(localFixedDir.z / *absCosSampledDir);

		result *= kt;
	} else {
		// Reflect
		*localSampledDir = Vector(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabsf(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	}

	return result / *pdfW;
}

void GlassMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
	ousideIor->AddReferencedTextures(referencedTexs);
	ior->AddReferencedTextures(referencedTexs);
}

void GlassMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (Kt == oldTex)
		Kt = newTex;
	if (ousideIor == oldTex)
		ousideIor = newTex;
	if (ior == oldTex)
		ior = newTex;
}

Properties GlassMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("glass"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetName()));
	props.Set(Property("scene.materials." + name + ".ioroutside")(ousideIor->GetName()));
	props.Set(Property("scene.materials." + name + ".iorinside")(ior->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

Spectrum ArchGlassMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum ArchGlassMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & SPECULAR))
		return Spectrum();

	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp();

	const bool isKtBlack = kt.Black();
	const bool isKrBlack = kr.Black();
	if (isKtBlack && isKrBlack)
		return Spectrum();

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = ousideIor->GetFloatValue(hitPoint);
	const float nt = ior->GetFloatValue(hitPoint);
	const float ntc = nt / nc;
	const float eta = nc / nt;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return Spectrum();
	}

	Spectrum result;
	if (passThroughEvent < threshold) {
		// Transmit

		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return Spectrum();

		*localSampledDir = -localFixedDir;
		*absCosSampledDir = fabsf(CosTheta(*localSampledDir));

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;

		if (!hitPoint.fromLight) {
			if (entering)
				result = Spectrum();
			else
				result = FresnelCauchy_Evaluate(ntc, -costheta);
		} else {
			if (entering)
				result = FresnelCauchy_Evaluate(ntc, costheta);
			else
				result = Spectrum();
		}
		result *= Spectrum(1.f) + (Spectrum(1.f) - result) * (Spectrum(1.f) - result);
		result = Spectrum(1.f) - result;

		result *= kt;
	} else {
		// Reflect
		if (costheta <= 0.f)
			return Spectrum();

		*localSampledDir = Vector(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabsf(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	}

	return result / *pdfW;
}

Spectrum ArchGlassMaterial::GetPassThroughTransparency(const HitPoint &hitPoint,
		const Vector &localFixedDir, const float passThroughEvent) const {
	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp();

	const bool isKtBlack = kt.Black();
	const bool isKrBlack = kr.Black();
	if (isKtBlack && isKrBlack)
		return Spectrum();

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float nc = ousideIor->GetFloatValue(hitPoint);
	const float nt = ior->GetFloatValue(hitPoint);
	const float ntc = nt / nc;
	const float eta = nc / nt;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (passThroughEvent < threshold) {
		// Transmit
	
		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return Spectrum();

		Spectrum result;
		if (!hitPoint.fromLight) {
			if (entering)
				result = Spectrum();
			else
				result = FresnelCauchy_Evaluate(ntc, -costheta);
		} else {
			if (entering)
				result = FresnelCauchy_Evaluate(ntc, costheta);
			else
				result = Spectrum();
		}
		result *= Spectrum(1.f) + (Spectrum(1.f) - result) * (Spectrum(1.f) - result);
		result = Spectrum(1.f) - result;

		return kt * result;
	} else
		return Spectrum();
}

void ArchGlassMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
	ousideIor->AddReferencedTextures(referencedTexs);
	ior->AddReferencedTextures(referencedTexs);
}

void ArchGlassMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (Kt == oldTex)
		Kt = newTex;
	if (ousideIor == oldTex)
		ousideIor = newTex;
	if (ior == oldTex)
		ior = newTex;
}

Properties ArchGlassMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("archglass"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetName()));
	props.Set(Property("scene.materials." + name + ".ioroutside")(ousideIor->GetName()));
	props.Set(Property("scene.materials." + name + ".iorinside")(ior->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

Spectrum MixMaterial::GetPassThroughTransparency(const HitPoint &hitPoint,
		const Vector &localFixedDir, const float passThroughEvent) const {
	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (passThroughEvent < weight1)
		return matA->GetPassThroughTransparency(hitPoint, localFixedDir, passThroughEvent / weight1);
	else
		return matB->GetPassThroughTransparency(hitPoint, localFixedDir, (passThroughEvent - weight2) / weight2);
}

float MixMaterial::GetEmittedRadianceY() const {
	return luxrays::Lerp(mixFactor->Y(), matA->GetEmittedRadianceY(), matB->GetEmittedRadianceY());
}

Spectrum MixMaterial::GetEmittedRadiance(const HitPoint &hitPoint, const float oneOverPrimitiveArea) const {
	Spectrum result;

	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (matA->IsLightSource() && (weight1 > 0.f))
		result += weight1 * matA->GetEmittedRadiance(hitPoint, oneOverPrimitiveArea);
	if (matB->IsLightSource() && (weight2 > 0.f))
		result += weight2 * matB->GetEmittedRadiance(hitPoint, oneOverPrimitiveArea);

	return result;
}

UV MixMaterial::GetBumpTexValue(const HitPoint &hitPoint) const {
	UV result;

	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (matA->HasBumpTex() && (weight1 > 0.f))
		result += weight1 * matA->GetBumpTexValue(hitPoint);
	if (matB->HasBumpTex() && (weight2 > 0.f))
		result += weight2 * matB->GetBumpTexValue(hitPoint);

	return result;	
}

Spectrum MixMaterial::GetNormalTexValue(const HitPoint &hitPoint) const {
	Spectrum result;

	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (matA->HasNormalTex() && (weight1 > 0.f))
		result += weight1 * matA->GetNormalTexValue(hitPoint);
	if (matB->HasNormalTex() && (weight2 > 0.f))
		result += weight2 * matB->GetNormalTexValue(hitPoint);

	return result;
}

Spectrum MixMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	Spectrum result;

	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (directPdfW)
		*directPdfW = 0.f;
	if (reversePdfW)
		*reversePdfW = 0.f;

	BSDFEvent eventMatA = NONE;
	if (weight1 > 0.f) {
		float directPdfWMatA, reversePdfWMatA;
		const Spectrum matAResult = matA->Evaluate(hitPoint, localLightDir, localEyeDir, &eventMatA, &directPdfWMatA, &reversePdfWMatA);
		if (!matAResult.Black()) {
			result += weight1 * matAResult;

			if (directPdfW)
				*directPdfW += weight1 * directPdfWMatA;
			if (reversePdfW)
				*reversePdfW += weight1 * reversePdfWMatA;
		}
	}

	BSDFEvent eventMatB = NONE;
	if (weight2 > 0.f) {
		float directPdfWMatB, reversePdfWMatB;
		const Spectrum matBResult = matB->Evaluate(hitPoint, localLightDir, localEyeDir, &eventMatB, &directPdfWMatB, &reversePdfWMatB);
		if (!matBResult.Black()) {
			result += weight2 * matBResult;

			if (directPdfW)
				*directPdfW += weight2 * directPdfWMatB;
			if (reversePdfW)
				*reversePdfW += weight2 * reversePdfWMatB;
		}
	}

	*event = eventMatA | eventMatB;

	return result;
}

Spectrum MixMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const bool sampleMatA = (passThroughEvent < weight1);

	const float weightFirst = sampleMatA ? weight1 : weight2;
	const float weightSecond = sampleMatA ? weight2 : weight1;

	const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

	// Sample the first material, evaluate the second
	const Material *matFirst = sampleMatA ? matA : matB;
	const Material *matSecond = sampleMatA ? matB : matA;

	// Sample the first material
	Spectrum result = matFirst->Sample(hitPoint, localFixedDir, localSampledDir,
			u0, u1, passThroughEventFirst, pdfW, absCosSampledDir, event, requestedEvent);
	if (result.Black())
		return Spectrum();
	*pdfW *= weightFirst;
	result *= *pdfW;

	// Evaluate the second material
	const Vector &localLightDir = (hitPoint.fromLight) ? localFixedDir : *localSampledDir;
	const Vector &localEyeDir = (hitPoint.fromLight) ? *localSampledDir : localFixedDir;
	BSDFEvent eventSecond;
	float pdfWSecond;
	Spectrum evalSecond = matSecond->Evaluate(hitPoint, localLightDir, localEyeDir, &eventSecond, &pdfWSecond);
	if (!evalSecond.Black()) {
		result += weightSecond * evalSecond;
		*pdfW += weightSecond * pdfWSecond;
	}

	return result / *pdfW;
}

void MixMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	float directPdfWMatA = 1.f;
	float reversePdfWMatA = 1.f;
	if (weight1 > 0.f)
		matA->Pdf(hitPoint, localLightDir, localEyeDir, &directPdfWMatA, &reversePdfWMatA);

	float directPdfWMatB = 1.f;
	float reversePdfWMatB = 1.f;
	if (weight2 > 0.f)
		matB->Pdf(hitPoint, localLightDir, localEyeDir, &directPdfWMatB, &reversePdfWMatB);

	if (directPdfW)
		*directPdfW = weight1 * directPdfWMatA + weight2 *directPdfWMatB;
	if (reversePdfW)
		*reversePdfW = weight1 * reversePdfWMatA + weight2 * reversePdfWMatB;
}

void MixMaterial::UpdateMaterialReferences(Material *oldMat, Material *newMat) {
	if (matA == oldMat)
		matA = newMat;

	if (matB == oldMat)
		matB = newMat;
}

bool MixMaterial::IsReferencing(const Material *mat) const {
	if (matA == mat)
		return true;
	if (matB == mat)
		return true;

	const MixMaterial *mixA = dynamic_cast<const MixMaterial *>(matA);
	if (mixA && mixA->IsReferencing(mat))
		return true;
	const MixMaterial *mixB = dynamic_cast<const MixMaterial *>(matB);
	if (mixB && mixB->IsReferencing(mat))
		return true;

	return false;
}

void MixMaterial::AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
	Material::AddReferencedMaterials(referencedMats);

	referencedMats.insert(matA);
	matA->AddReferencedMaterials(referencedMats);

	referencedMats.insert(matB);
	matB->AddReferencedMaterials(referencedMats);
}

void MixMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	matA->AddReferencedTextures(referencedTexs);
	matB->AddReferencedTextures(referencedTexs);
	mixFactor->AddReferencedTextures(referencedTexs);
}

void MixMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	matA->UpdateTextureReferences(oldTex, newTex);
	matB->UpdateTextureReferences(oldTex, newTex);

	if (mixFactor == oldTex)
		mixFactor = newTex;
}

Properties MixMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("mix"));
	props.Set(Property("scene.materials." + name + ".material1")(matA->GetName()));
	props.Set(Property("scene.materials." + name + ".material2")(matB->GetName()));
	props.Set(Property("scene.materials." + name + ".amount")(mixFactor->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Null material
//------------------------------------------------------------------------------

Spectrum NullMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
		return Spectrum();
}

Spectrum NullMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (SPECULAR | TRANSMIT)))
		return Spectrum();

	//throw runtime_error("Internal error, called NullMaterial::Sample()");

	*localSampledDir = -localFixedDir;
	*absCosSampledDir = 1.f;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return Spectrum(1.f);
}

Properties NullMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("null"));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

Spectrum MatteTranslucentMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const Spectrum r = Kr->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum t = Kt->GetSpectrumValue(hitPoint).Clamp() * 
		// Energy conservation
		(Spectrum(1.f) - r);

	const bool isKrBlack = r.Black();
	const bool isKtBlack = t.Black();

	// Decide to transmit or reflect
	float threshold;
	if (!isKrBlack) {
		if (!isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if (!isKtBlack)
			threshold = 1.f;
		else {
			if (directPdfW)
				*directPdfW = 0.f;
			if (reversePdfW)
				*reversePdfW = 0.f;
			return Spectrum();
		}
	}
	const float weight = (localLightDir.z * localEyeDir.z > 0.f) ?
		threshold : (1.f - threshold);
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * (weight * INV_PI));

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * (weight * INV_PI));

	if (localLightDir.z * localEyeDir.z > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * INV_PI * localLightDir.z;
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * INV_PI * localLightDir.z;
	}
}

Spectrum MatteTranslucentMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (DIFFUSE | REFLECT | TRANSMIT)) ||
			(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	*localSampledDir = CosineSampleHemisphere(u0, u1, pdfW);
	*absCosSampledDir = fabsf(localSampledDir->z);
	if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp() * 
		// Energy conservation
		(Spectrum(1.f) - kr);

	const bool isKrBlack = kr.Black();
	const bool isKtBlack = kt.Black();

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return Spectrum();
	}

	if (passThroughEvent < threshold) {
		*localSampledDir *= Sgn(localFixedDir.z);
		*event = DIFFUSE | REFLECT;
		*pdfW *= threshold;
		if (hitPoint.fromLight)
			return kr * fabsf(localFixedDir.z / (*absCosSampledDir * threshold));
		else
			return kr / threshold;
	} else {
		*localSampledDir *= -Sgn(localFixedDir.z);
		*event = DIFFUSE | TRANSMIT;
		*pdfW *= (1.f - threshold);
		if (hitPoint.fromLight)
			return kt * fabsf(localFixedDir.z / (*absCosSampledDir * (1.f - threshold)));
		else
			return kt / (1.f - threshold);
	}
}

void MatteTranslucentMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * (.5f * INV_PI));

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * (.5f * INV_PI));
}

void MatteTranslucentMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
}

void MatteTranslucentMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (Kt == oldTex)
		Kt = newTex;
}

Properties MatteTranslucentMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("mattetranslucent"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Glossy2 material
//
// LuxRender Glossy2 material porting.
//------------------------------------------------------------------------------

float Glossy2Material::SchlickBSDF_CoatingWeight(const Spectrum &ks, const Vector &localFixedDir) const {
	// No sampling on the back face
	if (localFixedDir.z <= 0.f)
		return 0.f;

	// Approximate H by using reflection direction for wi
	const float u = fabsf(localFixedDir.z);
	const Spectrum S = FresnelSlick_Evaluate(ks, u);

	// Ensures coating is never sampled less than half the time
	// unless we are on the back face
	return .5f * (1.f + S.Filter());
}

Spectrum Glossy2Material::SchlickBSDF_CoatingF(const bool fromLight, const Spectrum &ks, const float roughness,
		const float anisotropy, const Vector &localFixedDir, const Vector &localSampledDir) const {
	// No sampling on the back face
	if (localFixedDir.z <= 0.f)
		return Spectrum();

	const float coso = fabsf(localFixedDir.z);
	const float cosi = fabsf(localSampledDir.z);

	const Vector wh(Normalize(localFixedDir + localSampledDir));
	const Spectrum S = FresnelSlick_Evaluate(ks, AbsDot(localSampledDir, wh));

	const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);

	// Multibounce - alternative with interreflection in the coating creases
	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;
	if (!fromLight)
		factor = factor / 4.f * coso +
				(multibounce ? cosi * Clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);
	else
		factor = factor / (4.f * cosi) + 
				(multibounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);

	return factor * S;
}

Spectrum Glossy2Material::SchlickBSDF_CoatingAbsorption(const float cosi, const float coso,
		const Spectrum &alpha, const float depth) const {
	if (depth > 0.f) {
		// 1/cosi+1/coso=(cosi+coso)/(cosi*coso)
		const float depthFactor = depth * (cosi + coso) / (cosi * coso);
		return Exp(alpha * -depthFactor);
	} else
		return Spectrum(1.f);
}

Spectrum Glossy2Material::SchlickBSDF_CoatingSampleF(const bool fromLight, const Spectrum ks,
		const float roughness, const float anisotropy, const Vector &localFixedDir, Vector *localSampledDir,
		float u0, float u1, float *pdf) const {
	// No sampling on the back face
	if (localFixedDir.z <= 0.f)
		return Spectrum();

	Vector wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	const float cosWH = Dot(localFixedDir, wh);
	*localSampledDir = 2.f * cosWH * wh - localFixedDir;

	if ((localSampledDir->z < DEFAULT_COS_EPSILON_STATIC) || (localFixedDir.z * localSampledDir->z < 0.f))
		return Spectrum();

	const float coso = fabsf(localFixedDir.z);
	const float cosi = fabsf(localSampledDir->z);

	*pdf = specPdf / (4.f * cosWH);
	if (*pdf <= 0.f)
		return Spectrum();

	Spectrum S = FresnelSlick_Evaluate(ks, cosWH);

	const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
	if (!fromLight)
		//CoatingF(sw, *wi, wo, f_);
		S *= (d / *pdf) * G / (4.f * coso) + 
				(multibounce ? cosi * Clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) / *pdf : 0.f);
	else
		//CoatingF(sw, wo, *wi, f_);
		S *= (d / *pdf) * G / (4.f * cosi) + 
				(multibounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) / *pdf : 0.f);

	return S;
}

float Glossy2Material::SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,
		const Vector &localFixedDir, const Vector &localSampledDir) const {
	// No sampling on the back face
	if (localFixedDir.z <= 0.f)
		return 0.f;

	const Vector wh(Normalize(localFixedDir + localSampledDir));
	return SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localFixedDir, wh));
}

Spectrum Glossy2Material::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;

	const Spectrum baseF = Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI * fabsf(localLightDir.z);
	if (localEyeDir.z <= 0.f) {
		// Back face: no coating

		if (directPdfW)
			*directPdfW = fabsf(localSampledDir.z * INV_PI);

		if (reversePdfW)
			*reversePdfW = fabsf(localFixedDir.z * INV_PI);

		*event = DIFFUSE | REFLECT;
		return baseF;
	}

	// Front face: coating+base
	*event = GLOSSY | REFLECT;

	Spectrum ks = Ks->GetSpectrumValue(hitPoint);
	const float i = index->GetFloatValue(hitPoint);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = ks.Clamp();

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * fabsf(localSampledDir.z * INV_PI) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, localSampledDir);
	}

	if (reversePdfW) {
		const float wCoatingR = SchlickBSDF_CoatingWeight(ks, localSampledDir);
		const float wBaseR = 1.f - wCoatingR;

		*reversePdfW = wBaseR * fabsf(localFixedDir.z * INV_PI) +
				wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localSampledDir, localFixedDir);
	}

	// Absorption
	const float cosi = fabsf(localSampledDir.z);
	const float coso = fabsf(localFixedDir.z);
	const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp();
	const float d = depth->GetFloatValue(hitPoint);
	const Spectrum absorption = SchlickBSDF_CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const Vector H(Normalize(localFixedDir + localSampledDir));
	const Spectrum S = FresnelSlick_Evaluate(ks, AbsDot(localSampledDir, H));

	const Spectrum coatingF = SchlickBSDF_CoatingF(hitPoint.fromLight, ks, roughness, anisotropy, localFixedDir, localSampledDir);

	// Blend in base layer Schlick style
	// assumes coating bxdf takes fresnel factor S into account

	return coatingF + absorption * (Spectrum(1.f) - S) * baseF;
}

Spectrum Glossy2Material::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if ((!(requestedEvent & (GLOSSY | REFLECT)) && localFixedDir.z > 0.f) ||
		(!(requestedEvent & (DIFFUSE | REFLECT)) && localFixedDir.z <= 0.f) ||
		(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	if (localFixedDir.z <= 0.f) {
		// Back face
		*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);

		*absCosSampledDir = fabsf(localSampledDir->z);
		if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return Spectrum();
		*event = DIFFUSE | REFLECT;
		if (hitPoint.fromLight)
			return Kd->GetSpectrumValue(hitPoint) * fabsf(localFixedDir.z / *absCosSampledDir);
		else
			return Kd->GetSpectrumValue(hitPoint);
	}

	Spectrum ks = Ks->GetSpectrumValue(hitPoint);
	const float i = index->GetFloatValue(hitPoint);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = ks.Clamp();

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	// Coating is used only on the front face
	const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
	const float wBase = 1.f - wCoating;

	float basePdf, coatingPdf;
	Spectrum baseF, coatingF;

	if (passThroughEvent < wBase) {
		// Sample base BSDF (Matte BSDF)
		*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, &basePdf);

		*absCosSampledDir = fabsf(localSampledDir->z);
		if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return Spectrum();

		baseF = Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI * fabsf(hitPoint.fromLight ? localFixedDir.z : *absCosSampledDir);

		// Evaluate coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingF(hitPoint.fromLight, ks, roughness, anisotropy, localFixedDir, *localSampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, *localSampledDir);

		*event = GLOSSY | REFLECT;
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(hitPoint.fromLight, ks, roughness, anisotropy,
				localFixedDir, localSampledDir, u0, u1, &coatingPdf);
		if (coatingF.Black())
			return Spectrum();

		*absCosSampledDir = fabsf(localSampledDir->z);
		if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return Spectrum();

		coatingF *= coatingPdf;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = *absCosSampledDir * INV_PI;
		baseF = Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI * fabsf(hitPoint.fromLight ? localFixedDir.z : *absCosSampledDir);

		*event = GLOSSY | REFLECT;
	}

	*pdfW = coatingPdf * wCoating + basePdf * wBase;

	// Absorption
	const float cosi = fabsf(localSampledDir->z);
	const float coso = fabsf(localFixedDir.z);
	const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp();
	const float d = depth->GetFloatValue(hitPoint);
	const Spectrum absorption = SchlickBSDF_CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const Vector H(Normalize(localFixedDir + *localSampledDir));
	const Spectrum S = FresnelSlick_Evaluate(ks, AbsDot(*localSampledDir, H));

	// Blend in base layer Schlick style
	// coatingF already takes fresnel factor S into account

	return (coatingF + absorption * (Spectrum(1.f) - S) * baseF) / *pdfW;
}

void Glossy2Material::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;

	Spectrum ks = Ks->GetSpectrumValue(hitPoint);
	const float i = index->GetFloatValue(hitPoint);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = ks.Clamp();

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * fabsf(localSampledDir.z * INV_PI) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, localSampledDir);
	}

	if (reversePdfW) {
		const float wCoatingR = SchlickBSDF_CoatingWeight(ks, localSampledDir);
		const float wBaseR = 1.f - wCoatingR;

		*reversePdfW = wBaseR * fabsf(localFixedDir.z * INV_PI) +
				wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localSampledDir, localFixedDir);
	}
}

void Glossy2Material::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kd->AddReferencedTextures(referencedTexs);
	Ks->AddReferencedTextures(referencedTexs);
	nu->AddReferencedTextures(referencedTexs);
	nv->AddReferencedTextures(referencedTexs);
	Ka->AddReferencedTextures(referencedTexs);
	depth->AddReferencedTextures(referencedTexs);
	index->AddReferencedTextures(referencedTexs);
}

void Glossy2Material::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kd == oldTex)
		Kd = newTex;
	if (Ks == oldTex)
		Ks = newTex;
	if (nu == oldTex)
		nu = newTex;
	if (nv == oldTex)
		nv = newTex;
	if (Ka == oldTex)
		Ka = newTex;
	if (depth == oldTex)
		depth = newTex;
	if (index == oldTex)
		index = newTex;
}

Properties Glossy2Material::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("glossy2"));
	props.Set(Property("scene.materials." + name + ".kd")(Kd->GetName()));
	props.Set(Property("scene.materials." + name + ".ks")(Ks->GetName()));
	props.Set(Property("scene.materials." + name + ".uroughness")(nu->GetName()));
	props.Set(Property("scene.materials." + name + ".vroughness")(nv->GetName()));
	props.Set(Property("scene.materials." + name + ".ka")(Ka->GetName()));
	props.Set(Property("scene.materials." + name + ".d")(depth->GetName()));
	props.Set(Property("scene.materials." + name + ".index")(index->GetName()));
	props.Set(Property("scene.materials." + name + ".multibounce")(multibounce));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Metal2 material
//
// LuxRender Metal2 material porting.
//------------------------------------------------------------------------------

Spectrum Metal2Material::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	const Vector wh(Normalize(localLightDir + localEyeDir));
	const float cosWH = Dot(localLightDir, wh);

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * cosWH);

	if (reversePdfW)
		*reversePdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * cosWH);

	const Spectrum etaVal = n->GetSpectrumValue(hitPoint);
	const Spectrum kVal = k->GetSpectrumValue(hitPoint);
	const Spectrum F = FresnelGeneral_Evaluate(etaVal, kVal, cosWH);

	const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);

	*event = GLOSSY | REFLECT;
	return (SchlickDistribution_D(roughness, wh, anisotropy) * G / (4.f * fabsf(localEyeDir.z))) * F;
}

Spectrum Metal2Material::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
			fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	Vector wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	const float cosWH = Dot(localFixedDir, wh);
	*localSampledDir = 2.f * cosWH * wh - localFixedDir;

	const float coso = fabsf(localFixedDir.z);
	const float cosi = fabsf(localSampledDir->z);
	*absCosSampledDir = cosi;
	if ((*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) || (localFixedDir.z * localSampledDir->z < 0.f))
		return Spectrum();

	*pdfW = specPdf / (4.f * fabsf(cosWH));
	if (*pdfW <= 0.f)
		return Spectrum();

	const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);

	const Spectrum etaVal = n->GetSpectrumValue(hitPoint);
	const Spectrum kVal = k->GetSpectrumValue(hitPoint);
	const Spectrum F = FresnelGeneral_Evaluate(etaVal, kVal, cosWH);

	float factor = (d / *pdfW) * G * fabsf(cosWH);
	if (!hitPoint.fromLight)
		factor /= 4.f * coso;
	else
		factor /= 4.f * cosi;

	*event = GLOSSY | REFLECT;

	return factor * F;
}

void Metal2Material::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	const Vector wh(Normalize(localLightDir + localEyeDir));

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localLightDir, wh));

	if (reversePdfW)
		*reversePdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localLightDir, wh));
}

void Metal2Material::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	n->AddReferencedTextures(referencedTexs);
	k->AddReferencedTextures(referencedTexs);
	nu->AddReferencedTextures(referencedTexs);
	nv->AddReferencedTextures(referencedTexs);
}

void Metal2Material::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (n == oldTex)
		n = newTex;
	if (k == oldTex)
		k = newTex;
	if (nu == oldTex)
		nu = newTex;
	if (nv == oldTex)
		nv = newTex;
}

Properties Metal2Material::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("metal2"));
	props.Set(Property("scene.materials." + name + ".n")(n->GetName()));
	props.Set(Property("scene.materials." + name + ".k")(k->GetName()));
	props.Set(Property("scene.materials." + name + ".uroughness")(nu->GetName()));
	props.Set(Property("scene.materials." + name + ".vroughness")(nv->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// RoughGlass material
//
// LuxRender RoughGlass material porting.
//------------------------------------------------------------------------------

Spectrum RoughGlassMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp();

	const bool isKtBlack = kt.Black();
	const bool isKrBlack = kr.Black();
	if (isKtBlack && isKrBlack)
		return Spectrum();

	const float nc = ousideIor->GetFloatValue(hitPoint);
	const float nt = ior->GetFloatValue(hitPoint);
	const float ntc = nt / nc;

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (localLightDir.z * localEyeDir.z < 0.f) {
		// Transmit

		const bool entering = (CosTheta(localLightDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		Vector wh = eta * localLightDir + localEyeDir;
		if (wh.z < 0.f)
			wh = -wh;

		const float lengthSquared = wh.LengthSquared();
		if (!(lengthSquared > 0.f))
			return Spectrum();
		wh /= sqrtf(lengthSquared);
		const float cosThetaI = fabsf(CosTheta(localEyeDir));
		const float cosThetaIH = AbsDot(localEyeDir, wh);
		const float cosThetaOH = Dot(localLightDir, wh);

		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);

		if (directPdfW)
			*directPdfW = threshold * specPdf * (hitPoint.fromLight ? fabsf(cosThetaIH) : (fabsf(cosThetaOH) * eta * eta)) / lengthSquared;

		if (reversePdfW)
			*reversePdfW = threshold * specPdf * (hitPoint.fromLight ? (fabsf(cosThetaOH) * eta * eta) : fabsf(cosThetaIH)) / lengthSquared;

		Spectrum result = (fabsf(cosThetaOH) * cosThetaIH * D *
			G / (cosThetaI * lengthSquared)) *
			kt * (Spectrum(1.f) - F);

		return result;
	} else {
		// Reflect
		const float cosThetaO = fabsf(CosTheta(localLightDir));
		const float cosThetaI = fabsf(CosTheta(localEyeDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f)
			return Spectrum();
		Vector wh = localLightDir + localEyeDir;
		if (wh == Vector(0.f))
			return Spectrum();
		wh = Normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		float cosThetaH = Dot(localEyeDir, wh);
		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaH);

		if (directPdfW)
			*directPdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localLightDir, wh));

		if (reversePdfW)
			*reversePdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localLightDir, wh));

		Spectrum result = (D * G / (4.f * cosThetaI)) * kr * F;

		return result;
	}
}

Spectrum RoughGlassMaterial::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (GLOSSY | REFLECT | TRANSMIT)) ||
			(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp();

	const bool isKtBlack = kt.Black();
	const bool isKrBlack = kr.Black();
	if (isKtBlack && isKrBlack)
		return Spectrum();

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	Vector wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	if (wh.z < 0.f)
		wh = -wh;
	const float cosThetaOH = Dot(localFixedDir, wh);

	const float nc = ousideIor->GetFloatValue(hitPoint);
	const float nt = ior->GetFloatValue(hitPoint);
	const float ntc = nt / nc;

	const float coso = fabsf(localFixedDir.z);

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return Spectrum();
	}

	Spectrum result;
	if (passThroughEvent < threshold) {
		// Transmit

		const bool entering = (CosTheta(localFixedDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;
		const float eta2 = eta * eta;
		const float sinThetaIH2 = eta2 * Max(0.f, 1.f - cosThetaOH * cosThetaOH);
		if (sinThetaIH2 >= 1.f)
			return Spectrum();
		float cosThetaIH = sqrtf(1.f - sinThetaIH2);
		if (entering)
			cosThetaIH = -cosThetaIH;
		const float length = eta * cosThetaOH + cosThetaIH;
		*localSampledDir = length * wh - eta * localFixedDir;

		const float lengthSquared = length * length;
		*pdfW = specPdf * fabsf(cosThetaIH) / lengthSquared;
		if (*pdfW <= 0.f)
			return Spectrum();

		const float cosi = fabsf(localSampledDir->z);
		*absCosSampledDir = cosi;

		const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
		float factor = (d / specPdf) * G * fabsf(cosThetaOH) / threshold;

		if (!hitPoint.fromLight) {
			const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaIH);
			result = (factor / coso) * kt * (Spectrum(1.f) - F);
		} else {
			const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
			result = (factor / cosi) * kt * (Spectrum(1.f) - F);
		}

		*pdfW *= threshold;
		*event = GLOSSY | TRANSMIT;
	} else {
		// Reflect
		*pdfW = specPdf / (4.f * fabsf(cosThetaOH));
		if (*pdfW <= 0.f)
			return Spectrum();

		*localSampledDir = 2.f * cosThetaOH * wh - localFixedDir;

		const float cosi = fabsf(localSampledDir->z);
		*absCosSampledDir = cosi;
		if ((*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) || (localFixedDir.z * localSampledDir->z < 0.f))
			return Spectrum();

		const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
		float factor = (d / specPdf) * G * fabsf(cosThetaOH) / (1.f - threshold);

		const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		factor /= (!hitPoint.fromLight) ? coso : cosi;
		result = factor * F * kr;

		*pdfW *= (1.f - threshold);
		*event = GLOSSY | REFLECT;
	}

	return result;
}

void RoughGlassMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = 0.f;
	if (reversePdfW)
		*reversePdfW = 0.f;

	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp();

	const bool isKtBlack = kt.Black();
	const bool isKrBlack = kr.Black();
	if (isKtBlack && isKrBlack)
		return;


	const float nc = ousideIor->GetFloatValue(hitPoint);
	const float nt = ior->GetFloatValue(hitPoint);
	const float ntc = nt / nc;

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (localLightDir.z * localEyeDir.z < 0.f) {
		// Transmit

		const bool entering = (CosTheta(localLightDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		Vector wh = eta * localLightDir + localEyeDir;
		if (wh.z < 0.f)
			wh = -wh;

		const float lengthSquared = wh.LengthSquared();
		if (!(lengthSquared > 0.f))
			return;

		wh /= sqrtf(lengthSquared);
		const float cosThetaIH = AbsDot(localEyeDir, wh);
		const float cosThetaOH = AbsDot(localLightDir, wh);

		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);

		if (directPdfW)
			*directPdfW = threshold * specPdf * cosThetaIH / lengthSquared;

		if (reversePdfW)
			*reversePdfW = threshold * specPdf * cosThetaOH * eta * eta / lengthSquared;
	} else {
		// Reflect
		const float cosThetaO = fabsf(CosTheta(localLightDir));
		const float cosThetaI = fabsf(CosTheta(localEyeDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f)
			return;

		Vector wh = localLightDir + localEyeDir;
		if (wh == Vector(0.f))
			return;
		wh = Normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);

		if (directPdfW)
			*directPdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localLightDir, wh));

		if (reversePdfW)
			*reversePdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localLightDir, wh));
	}
}

void RoughGlassMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
	ousideIor->AddReferencedTextures(referencedTexs);
	ior->AddReferencedTextures(referencedTexs);
	nu->AddReferencedTextures(referencedTexs);
	nv->AddReferencedTextures(referencedTexs);
}

void RoughGlassMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (Kt == oldTex)
		Kt = newTex;
	if (ousideIor == oldTex)
		ousideIor = newTex;
	if (ior == oldTex)
		ior = newTex;
	if (nu == oldTex)
		nu = newTex;
	if (nv == oldTex)
		nv = newTex;
}

Properties RoughGlassMaterial::ToProperties() const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("roughglass"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetName()));
	props.Set(Property("scene.materials." + name + ".ioroutside")(ousideIor->GetName()));
	props.Set(Property("scene.materials." + name + ".iorinside")(ior->GetName()));
	props.Set(Property("scene.materials." + name + ".uroughness")(nu->GetName()));
	props.Set(Property("scene.materials." + name + ".vroughness")(nv->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Velvet material
//------------------------------------------------------------------------------

Spectrum VelvetMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {


	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);

	*event = DIFFUSE | REFLECT;
	
	const float A1 = P1->GetFloatValue(hitPoint);
	const float A2 = P2->GetFloatValue(hitPoint);
	const float A3 = P3->GetFloatValue(hitPoint);
	const float delta = Thickness->GetFloatValue(hitPoint);
	
	const float cosv = -Dot(localLightDir, localEyeDir);

	// Compute phase function

	const float B = 3.0f * cosv;

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI);
 
	p = (p * delta) / fabsf(localEyeDir.z);

	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;

	return Kd->GetSpectrumValue(hitPoint).Clamp() * p;
}

Spectrum VelvetMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {

	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);

	*absCosSampledDir = fabsf(localSampledDir->z);
	if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*event = DIFFUSE | REFLECT;
	
	float A1 = P1->GetFloatValue(hitPoint);
	float A2 = P2->GetFloatValue(hitPoint);
	float A3 = P3->GetFloatValue(hitPoint);
	float delta = Thickness->GetFloatValue(hitPoint);
	
	const float cosv = -Dot(localFixedDir, *localSampledDir);

	// Compute phase function

	const float B = 3.0f * cosv;

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI);
 
	p = (p * delta) / (hitPoint.fromLight ? fabsf(localSampledDir->z) : fabsf(localFixedDir.z));
	
	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;
	
	return Kd->GetSpectrumValue(hitPoint).Clamp() * (p / *pdfW);
}

void VelvetMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);
}

void VelvetMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kd->AddReferencedTextures(referencedTexs);
	P1->AddReferencedTextures(referencedTexs);
	P2->AddReferencedTextures(referencedTexs);
	P3->AddReferencedTextures(referencedTexs);
	Thickness->AddReferencedTextures(referencedTexs);
}

void VelvetMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kd == oldTex)
		Kd = newTex;
	if (P1 == oldTex)
		P1 = newTex;
	if (P2 == oldTex)
		P2 = newTex;
	if (P3 == oldTex)
		P3 = newTex;
	if (Thickness == oldTex)
		Thickness = newTex;
}

Properties VelvetMaterial::ToProperties() const  {
	Properties props;

	const std::string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("velvet"));
	props.Set(Property("scene.materials." + name + ".kd")(Kd->GetName()));
	props.Set(Property("scene.materials." + name + ".p1")(P1->GetName()));
	props.Set(Property("scene.materials." + name + ".p2")(P2->GetName()));
	props.Set(Property("scene.materials." + name + ".p3")(P3->GetName()));
	props.Set(Property("scene.materials." + name + ".thickness")(Thickness->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Cloth material
//------------------------------------------------------------------------------

static WeaveConfig DenimWeave = {
  3, 6,
  0.01f, 4.0f,
  0.0f, 0.5f, 
  5.0f, 1.0f, 3.0f,
  0.0f, 0.0f, 0.0f, 0.0f,
  0.0f
};

static int DenimPattern[3*6] = {1, 3, 8,  1, 3, 5,  1, 7, 5,  1, 4, 5,  6, 4, 5,  2, 4, 5};

static Yarn DenimYarn[8] = {
    {-30, 12, 0, 1, 5, 0.1667, 0.75, WARP},
    {-30, 12, 0, 1, 5, 0.1667, -0.25, WARP},
    {-30, 12, 0, 1, 5, 0.5, 1.0833, WARP},
    {-30, 12, 0, 1, 5, 0.5, 0.0833, WARP},
    {-30, 12, 0, 1, 5, 0.8333, 0.4167, WARP},
    {-30, 38, 0, 1, 1, 0.1667, 0.25, WEFT},
    {-30, 38, 0, 1, 1, 0.5, 0.5833, WEFT},
    {-30, 38, 0, 1, 1, 0.8333, 0.9167, WEFT},
};

static WeaveConfig SilkShantungWeave = {
  6, 8,
  0.02f, 1.5f,
  0.5f, 0.5f, 
  8.0f, 16.0f, 0.0f,
  20.0f, 20.0f, 10.0f, 10.0f,
  500.0f
};

static int SilkShantungPattern[6*8] = {3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,
                                       4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5
};

static Yarn SilkShantungYarn[5] = {
    {0, 50, -0.5, 2, 4,  0.3333, 0.25, WARP},
    {0, 50, -0.5, 2, 4,  0.8333, 0.75, WARP},
    {0, 23, -0.3, 4, 4,  0.3333, 0.75, WEFT},
    {0, 23, -0.3, 4, 4, -0.1667, 0.25, WEFT},
    {0, 23, -0.3, 4, 4,  0.8333, 0.25, WEFT},
};

static WeaveConfig SilkCharmeuseWeave = {
  5, 10,
  0.02f, 7.3f,
  0.5f, 0.5f, 
  9.0f, 1.0f, 3.0f,
  0.0f, 0.0f, 0.0f, 0.0f,
  0.0f
};

static int SilkCharmeusePattern[5*10] = {10, 2, 4, 6, 8,   1, 2, 4, 6,  8,  1, 2, 4, 13, 8,  1, 2,  4, 7, 8,  1, 11, 4, 7, 8,
                                          1, 3, 4, 7, 8,   1, 3, 4, 7, 14,  1, 3, 4,  7, 9,  1, 3, 12, 7, 9,  1,  3, 5, 7, 9};
static Yarn SilkCharmeuseYarn[14] = {
    {0, 40, 2, 1, 9, 0.1, 0.45, WARP},
    {0, 40, 2, 1, 9, 0.3, 1.05, WARP},
    {0, 40, 2, 1, 9, 0.3, 0.05, WARP},
    {0, 40, 2, 1, 9, 0.5, 0.65, WARP},
    {0, 40, 2, 1, 9, 0.5, -0.35, WARP},
    {0, 40, 2, 1, 9, 0.7, 1.25, WARP},
    {0, 40, 2, 1, 9, 0.7, 0.25, WARP},
    {0, 40, 2, 1, 9, 0.9, 0.85, WARP},
    {0, 40, 2, 1, 9, 0.9, -0.15, WARP},
    {0, 60, 0, 1, 1, 0.1, 0.95, WEFT},
    {0, 60, 0, 1, 1, 0.3, 0.55, WEFT},
    {0, 60, 0, 1, 1, 0.5, 0.15, WEFT},
    {0, 60, 0, 1, 1, 0.7, 0.75, WEFT},
    {0, 60, 0, 1, 1, 0.9, 0.35, WEFT},
};

static WeaveConfig CottonTwillWeave = {
  4, 8,
  0.01f, 4.0f,
  0.0f, 0.5f, 
  6.0f, 2.0f, 4.0f,
  0.0f, 0.0f, 0.0f, 0.0f,
  0.0f
};

static int CottonTwillPattern[4*8] = {7, 2, 4, 6,  7, 2, 4, 6,  1, 8, 4,  6,  1, 8, 4,  6,
                                      1, 3, 9, 6,  1, 3, 9, 6,  1, 3, 5, 10,  1, 3, 5, 10};

static Yarn CottonTwillYarn[10] = {
    {-30, 24, 0, 1, 6, 0.125,  0.375, WARP},
    {-30, 24, 0, 1, 6, 0.375,  1.125, WARP},
    {-30, 24, 0, 1, 6, 0.375,  0.125, WARP},
    {-30, 24, 0, 1, 6, 0.625,  0.875, WARP},
    {-30, 24, 0, 1, 6, 0.625, -0.125, WARP},
    {-30, 24, 0, 1, 6, 0.875,  0.625, WARP},
    {-30, 36, 0, 2, 1, 0.125,  0.875, WEFT},
    {-30, 36, 0, 2, 1, 0.375,  0.625, WEFT},
    {-30, 36, 0, 2, 1, 0.625,  0.375, WEFT},
    {-30, 36, 0, 2, 1, 0.875,  0.125, WEFT},
};

static WeaveConfig WoolGarbardineWeave = {
  6, 9,
  0.01f, 4.0f,
  0.0f, 0.5f, 
  12.0f, 6.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 0.0f,
  0.0f
};

static int WoolGarbardinePattern[6*9] = {1, 1, 2, 2, 7, 7,  1, 1, 2, 2, 7, 7,  1, 1, 2, 2, 7, 7,
                                         1, 1, 6, 6, 4, 4,  1, 1, 6, 6, 4, 4,  1, 1, 6, 6, 4, 4,
					 5, 5, 3, 3, 4, 4,  5, 5, 3, 3, 4, 4,  5, 5, 3, 3, 4, 4
};

static Yarn WoolGarbardineYarn[7] = {
    {30, 30, 0, 2, 6, 0.167, 0.667, WARP},
    {30, 30, 0, 2, 6, 0.500, 1.000, WARP},
    {30, 30, 0, 2, 6, 0.500, 0.000, WARP},
    {30, 30, 0, 2, 6, 0.833, 0.333, WARP},
    {30, 30, 0, 3, 2, 0.167, 0.167, WEFT},
    {30, 30, 0, 3, 2, 0.500, 0.500, WEFT},
    {30, 30, 0, 3, 2, 0.833, 0.833, WEFT},
};

static WeaveConfig PolyesterWeave = {
  2, 2,
  0.015f, 4.0f,
  0.5f, 0.5f,
  1.0f, 1.0f, 0.0f, 
  8.0f, 8.0f, 6.0f, 6.0f,
  50.0f
};

static int PolyesterPattern[2*2] = {3, 2, 1, 4};
static Yarn PolyesterYarn[4] = {
    {0, 22, -0.7, 1, 1, 0.25, 0.25, WARP},
    {0, 22, -0.7, 1, 1, 0.75, 0.75, WARP},
    {0, 16, -0.7, 1, 1, 0.25, 0.75, WEFT},
    {0, 16, -0.7, 1, 1, 0.75, 0.25, WEFT}
};

static uint64_t sampleTEA(uint32_t v0, uint32_t v1, u_int rounds = 4)
{
	uint32_t sum = 0;

	for (u_int i = 0; i < rounds; ++i) {
		sum += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xA341316C) ^ (v1 + sum) ^ ((v1 >> 5) + 0xC8013EA4);
		v1 += ((v0 << 4) + 0xAD90777D) ^ (v0 + sum) ^ ((v0 >> 5) + 0x7E95761E);
	}

	return ((uint64_t) v1 << 32) + v0;
}

static float sampleTEAfloat(uint32_t v0, uint32_t v1, u_int rounds = 4)
{
	/* Trick from MTGP: generate an uniformly distributed
	   single precision number in [1,2) and subtract 1. */
	union {
		uint32_t u;
		float f;
	} x;
	x.u = ((sampleTEA(v0, v1, rounds) & 0xFFFFFFFF) >> 9) | 0x3f800000UL;
	return x.f - 1.0f;
}

// von Mises Distribution
static float vonMises(float cos_x, float b)
{
	// assumes a = 0, b > 0 is a concentration parameter.

	const float factor = expf(b * cos_x) * INV_TWOPI;
	const float absB = fabsf(b);
	if (absB <= 3.75f) {
		const float t0 = absB / 3.75f;
		const float t = t0 * t0;
		return factor / (1.0f + t * (3.5156229f + t * (3.0899424f +
			t * (1.2067492f + t * (0.2659732f + t * (0.0360768f +
			t * 0.0045813f))))));
	} else {
		const float t = 3.75f / absB;
		return factor * sqrtf(absB) / (expf(absB) * (0.39894228f +
			t * (0.01328592f + t * (0.00225319f +
			t * (-0.00157565f + t * (0.00916281f +
			t * (-0.02057706f + t * (0.02635537f +
			t * (-0.01647633f + t * 0.00392377f)))))))));
	}
}

// Attenuation term
static float seeliger(float cos_th1, float cos_th2, float sg_a, float sg_s)
{
	const float al = sg_s / (sg_a + sg_s); // albedo
	const float c1 = max(0.f, cos_th1);
	const float c2 = max(0.f, cos_th2);
	if (c1 == 0.0f || c2 == 0.0f)
		return 0.0f;
	return al * INV_TWOPI * .5f * c1 * c2 / (c1 + c2);
}

Spectrum ClothMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	  
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);

	*event = GLOSSY | REFLECT;

	UV uv;
	float umax, scale = specularNormalization;
	const Yarn *yarn = GetYarn(hitPoint.uv.u, hitPoint.uv.v, &uv, &umax, &scale);
	
	scale = scale * EvalSpecular(yarn, uv, umax, localLightDir, localEyeDir);
	
	const Texture *ks = yarn->yarn_type == WARP ? Warp_Ks :  Weft_Ks;
	const Texture *kd = yarn->yarn_type == WARP ? Warp_Kd :  Weft_Kd;

	return (kd->GetSpectrumValue(hitPoint).Clamp() + ks->GetSpectrumValue(hitPoint).Clamp() * scale) * INV_PI * fabsf(localLightDir.z);
}

Spectrum ClothMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {

	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
			(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);

	*absCosSampledDir = fabsf(localSampledDir->z);
	if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*event = GLOSSY | REFLECT;
	
	UV uv;
	float umax, scale = specularNormalization;

	const Yarn *yarn = GetYarn(hitPoint.uv.u, hitPoint.uv.v, &uv, &umax, &scale);
	
	if (!hitPoint.fromLight)
	    scale = scale * EvalSpecular(yarn, uv, umax, localFixedDir, *localSampledDir);
	else
	    scale = scale * EvalSpecular(yarn, uv, umax, *localSampledDir, localFixedDir);

	const Texture *ks = yarn->yarn_type == WARP ? Warp_Ks :  Weft_Ks;
	const Texture *kd = yarn->yarn_type == WARP ? Warp_Kd :  Weft_Kd;
	
	return kd->GetSpectrumValue(hitPoint).Clamp() + ks->GetSpectrumValue(hitPoint).Clamp() * scale;
}

void ClothMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);
}

void ClothMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Warp_Ks->AddReferencedTextures(referencedTexs);
	Weft_Ks->AddReferencedTextures(referencedTexs);
	Weft_Kd->AddReferencedTextures(referencedTexs);
	Warp_Kd->AddReferencedTextures(referencedTexs);
}

void ClothMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Weft_Kd == oldTex)
		Weft_Kd = newTex;
	if (Weft_Ks == oldTex)
		Weft_Ks = newTex;
	if (Warp_Kd == oldTex)
		Warp_Kd = newTex;
	if (Warp_Ks == oldTex)
		Warp_Ks = newTex;
}

Properties ClothMaterial::ToProperties() const  {
	Properties props;

	const std::string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("cloth"));
	
	switch (Preset) {
	  case DENIM:
		props.Set(Property("scene.materials." + name + ".preset")("denim"));
		break;
	  default:
	    break;
	}
	
	props.Set(Property("scene.materials." + name + ".weft_kd")(Weft_Kd->GetName()));
	props.Set(Property("scene.materials." + name + ".weft_ks")(Weft_Ks->GetName()));
	props.Set(Property("scene.materials." + name + ".warp_kd")(Warp_Kd->GetName()));
	props.Set(Property("scene.materials." + name + ".warp_ks")(Warp_Ks->GetName()));
	props.Set(Property("scene.materials." + name + ".repeat_u")(Repeat_U));
	props.Set(Property("scene.materials." + name + ".repeat_v")(Repeat_V));
	props.Set(Material::ToProperties());

	return props;
}

void ClothMaterial::GetYarnUV(const Yarn *yarn, const Point &center, const Point &xy, UV *uv, float *umaxMod) const
{
	*umaxMod = luxrays::Radians(yarn->umax);
	if (Weave->period > 0.f) {
		/* Number of TEA iterations (the more, the better the
		   quality of the pseudorandom floats) */
		const int teaIterations = 8;

		// Correlated (Perlin) noise.
		// generate 1 seed per yarn segment
		const float random1 = Noise((center.x *
			(Weave->tileHeight * Repeat_V +
			sampleTEAfloat(center.x, 2.f * center.y,
			teaIterations)) + center.y) / Weave->period, 0.0, 0.0);
		const float random2 = Noise((center.y *
			(Weave->tileWidth * Repeat_U +
			sampleTEAfloat(center.x, 2.f * center.y + 1.f,
			teaIterations)) + center.x) / Weave->period, 0.0, 0.0);
		
		if (yarn->yarn_type == WARP)
	  		*umaxMod += random1 * luxrays::Radians(Weave->dWarpUmaxOverDWarp) +
				random2 * luxrays::Radians(Weave->dWarpUmaxOverDWeft);
		else
			*umaxMod += random1 * luxrays::Radians(Weave->dWeftUmaxOverDWarp) +
				random2 * luxrays::Radians(Weave->dWeftUmaxOverDWeft);
	}
	

	// Compute u and v.
	// See Chapter 6.
	// Rotate pi/2 radians around z axis
	if (yarn->yarn_type == WARP) {
		uv->u = xy.y * 2.f * *umaxMod / yarn->length;
		uv->v = xy.x * M_PI / yarn->width;
	}
	else {
		uv->u = xy.x * 2.f * *umaxMod / yarn->length;
		uv->v = -xy.y * M_PI / yarn->width;
	}
}


const Yarn *ClothMaterial::GetYarn(const float u_i, const float v_i, UV *uv, float *umax, float *scale) const
{
	const float u = u_i * Repeat_U;
	const int bu = Floor2Int(u);
	const float ou = u - bu;
	const float v = v_i * Repeat_V;
	const int bv = Floor2Int(v);
	const float ov = v - bv;
	const u_int lx = min(Weave->tileWidth - 1, Floor2UInt(ou * Weave->tileWidth));
	const u_int ly = Weave->tileHeight - 1 -
		min(Weave->tileHeight - 1, Floor2UInt(ov * Weave->tileHeight));

	const int yarnID = Pattern[lx + Weave->tileWidth * ly] - 1;

	const Yarn *yarn = &Yarns[yarnID];

	const Point center((bu + yarn->centerU) * Weave->tileWidth,
		(bv + yarn->centerV) * Weave->tileHeight);
	const Point xy((ou - yarn->centerU) * Weave->tileWidth,
		(ov - yarn->centerV) * Weave->tileHeight);

	GetYarnUV(yarn, center, xy, uv, umax);

	/* Number of TEA iterations (the more, the better the
	   quality of the pseudorandom floats) */
	const int teaIterations = 8;

	// Compute random variation and scale specular component.
	if (Weave->fineness > 0.0f) {
		// Initialize random number generator based on texture location.
		// Generate fineness^2 seeds per 1 unit of texture.
		const uint32_t index1 = (uint32_t) ((center.x + xy.x) * Weave->fineness);
		const uint32_t index2 = (uint32_t) ((center.y + xy.y) * Weave->fineness);

		const float xi = sampleTEAfloat(index1, index2, teaIterations);
		
		*scale *= min(-logf(xi), 10.0f);
	}

	return yarn;
}

float ClothMaterial::EvalFilamentIntegrand(const Yarn *yarn, const Vector &om_i, const Vector &om_r, float u, float v, float umaxMod) const
{
	// 0 <= ss < 1.0
	if (Weave->ss < 0.0f || Weave->ss >= 1.0f)
		return 0.0f;

	// w * sin(umax) < l
	if (yarn->width * sinf(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const Vector h(Normalize(om_r + om_i));

	// u_of_v is location of specular reflection.
	const float u_of_v = atan2f(h.y, h.z);

	// Check if u_of_v within the range of valid u values
	if (fabsf(u_of_v) >= umaxMod)
		return 0.f;

	// Highlight has constant width delta_u
	const float delta_u = umaxMod * Weave->hWidth;

	// Check if |u(v) - u| < delta_u.
	if (fabsf(u_of_v - u) >= delta_u)
		return 0.f;

	
	// n is normal to the yarn surface
	// t is tangent of the fibers.
	const Normal n(Normalize(Normal(sinf(v), sinf(u_of_v) * cosf(v),
		cosf(u_of_v) * cosf(v))));
	const Vector t(Normalize(Vector(0.0f, cosf(u_of_v), -sinf(u_of_v))));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, min(fabsf(u_of_v),
		(1.f - Weave->ss) * umaxMod), (1.f - Weave->ss) * umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const Vector om_i_plus_om_r(om_i + om_r), t_cross_h(Cross(t, h));
	const float Gu = a * (R + a * cosf(v)) /
		(om_i_plus_om_r.Length() * fabsf(t_cross_h.x));


	// fc is phase function
	const float fc = Weave->alpha + vonMises(-Dot(om_i, om_r), Weave->beta);

	// attenuation function without smoothing.
	float As = seeliger(Dot(n, om_i), Dot(n, om_r), 0, 1);
	// As is attenuation function with smoothing.
	if (Weave->ss > 0.0f)
		As *= SmoothStep(0.f, 1.f, (umaxMod - fabsf(u_of_v)) /
			(Weave->ss * umaxMod));

	// fs is scattering function.
	const float fs = Gu * fc * As;

	// Domain transform.
	return fs * M_PI / Weave->hWidth;
}

float ClothMaterial::RadiusOfCurvature(const Yarn *yarn, float u, float umaxMod) const
{
	// rhat determines whether the spine is a segment
	// of an ellipse, a parabole, or a hyperbola.
	// See Section 5.3.
	const float rhat = 1.0f + yarn->kappa * (1.0f + 1.0f / tanf(umaxMod));
	const float a = 0.5f * yarn->width;
	
	if (rhat == 1.0f) { // circle; see Subsection 5.3.1.
		return 0.5f * yarn->length / sinf(umaxMod) - a;
	} else if (rhat > 0.0f) { // ellipsis
		const float tmax = atanf(rhat * tanf(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sinf(umaxMod)) / sinf(tmax);
		const float ahat = bhat / rhat;
		const float t = atanf(rhat * tanf(u));
		return powf(bhat * bhat * cosf(t) * cosf(t) +
			ahat * ahat * sinf(t) * sinf(t), 1.5f) / (ahat * bhat);
	} else if (rhat < 0.0f) { // hyperbola; see Subsection 5.3.3.
		const float tmax = -atanhf(rhat * tanf(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sinf(umaxMod)) / sinhf(tmax);
		const float ahat = bhat / rhat;
		const float t = -atanhf(rhat * tanf(u));
		return -powf(bhat * bhat * coshf(t) * coshf(t) +
			ahat * ahat * sinhf(t) * sinhf(t), 1.5f) / (ahat * bhat);
	} else { // rhat == 0  // parabola; see Subsection 5.3.2.
		const float tmax = tanf(umaxMod);
		const float ahat = (0.5f * yarn->length - a * sinf(umaxMod)) / (2.f * tmax);
		const float t = tanf(u);
		return 2.f * ahat * powf(1.f + t * t, 1.5f);
	}
}

float ClothMaterial::EvalStapleIntegrand(const Yarn *yarn, const Vector &om_i, const Vector &om_r, float u, float v, float umaxMod) const
{
	// w * sin(umax) < l
	if (yarn->width * sinf(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const Vector h(Normalize(om_i + om_r));

	// v_of_u is location of specular reflection.
	const float D = (h.y * cosf(u) - h.z * sinf(u)) /
		(sqrtf(h.x * h.x + powf(h.y * sinf(u) + h.z * cosf(u),
		2.0f)) * tanf(luxrays::Radians(yarn->psi)));
	if (!(fabsf(D) < 1.f))
		return 0.f;
	const float v_of_u = atan2f(-h.y * sinf(u) - h.z * cosf(u), h.x) +
		acosf(D);

	// Highlight has constant width delta_x on screen.
	const float delta_v = .5f * M_PI * Weave->hWidth;

	// Check if |x(v(u)) - x(v)| < delta_x/2.
	if (fabsf(v_of_u - v) >= delta_v)
		return 0.f;

	// n is normal to the yarn surface.
	const Vector n(Normalize(Vector(sinf(v_of_u), sinf(u) * cosf(v_of_u),
		cosf(u) * cosf(v_of_u))));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, fabsf(u), umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const Vector om_i_plus_om_r(om_i + om_r);
	const float Gv = a * (R + a * cosf(v_of_u)) /
		(om_i_plus_om_r.Length() * Dot(n, h) * fabsf(sinf(luxrays::Radians(yarn->psi))));

	// fc is phase function.
	const float fc = Weave->alpha + vonMises(-Dot(om_i, om_r), Weave->beta);

	// A is attenuation function without smoothing.
	const float A = seeliger(Dot(n, om_i), Dot(n, om_r), 0, 1);

	// fs is scattering function.
	const float fs = Gv * fc * A;
	
	// Domain transform.
	return fs * 2.0f * umaxMod / Weave->hWidth;
}

float ClothMaterial::EvalIntegrand(const Yarn *yarn, const UV &uv, float umaxMod, Vector &om_i, Vector &om_r) const
{
	if (yarn->yarn_type == WARP) {
		if (luxrays::Radians(yarn->psi != 0.0f))
			return EvalStapleIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
		else
			return EvalFilamentIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
	}
	else {
		// Rotate pi/2 radians around z axis
		swap(om_i.x, om_i.y);
		om_i.x = -om_i.x;
		swap(om_r.x, om_r.y);
		om_r.x = -om_r.x;

		if (luxrays::Radians(yarn->psi != 0.0f))
			return EvalStapleIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
		else
			return EvalFilamentIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
	}
}


float ClothMaterial::EvalSpecular(const Yarn *yarn,const UV &uv, float umax, const Vector &wo, const Vector &wi) const
{
	// Get incident and exitant directions.
	Vector om_i(wi);
	if (om_i.z < 0.f)
		om_i = -om_i;
	Vector om_r(wo);
	if (om_r.z < 0.f)
		om_r = -om_r;

	// Compute specular contribution.
	return EvalIntegrand(yarn, uv, umax, om_i, om_r);
}

void ClothMaterial::SetPreset() {
	switch (Preset) {
	  case DENIM:
		Yarns = DenimYarn;
		Pattern = DenimPattern;
		Weave = &DenimWeave;
		break;
	  case SILKCHARMEUSE:
		Yarns = SilkCharmeuseYarn;
		Pattern = SilkCharmeusePattern;
		Weave = &SilkCharmeuseWeave;
		break;
	  case SILKSHANTUNG:
		Yarns = SilkShantungYarn;
		Pattern = SilkShantungPattern;
		Weave = &SilkShantungWeave;
		break;
	  case COTTONTWILL:
		Yarns = CottonTwillYarn;
		Pattern = CottonTwillPattern;
		Weave = &CottonTwillWeave;
		break;
	  case WOOLGARBARDINE:
		Yarns = WoolGarbardineYarn;
		Pattern = WoolGarbardinePattern;
		Weave = &WoolGarbardineWeave;
		break;
	  case POLYESTER:
		Yarns = PolyesterYarn;
		Pattern = PolyesterPattern;
		Weave = &PolyesterWeave;
		break;
	  default:
	    break;
	}

	// Calibrate scale factor
	
	RandomGenerator random(1);

	const u_int nSamples = 100000;
	
	float result = 0.f;
	for (u_int i = 0; i < nSamples; ++i) {
		const Vector wi = CosineSampleHemisphere(random.floatValue(), random.floatValue());
		const Vector wo = CosineSampleHemisphere(random.floatValue(), random.floatValue());
		
		UV uv;
		float umax, scale = 1.f;
		
		const Yarn *yarn = GetYarn(random.floatValue(), random.floatValue(), &uv, &umax, &scale);
		
		result += EvalSpecular(yarn, uv, umax, wo, wi) * scale;
	}

	if (result > 0.f)
		specularNormalization = nSamples / result;
	else
		specularNormalization = 0;
//	printf("********************** specularNormalization = %f\n", specularNormalization);
}

//------------------------------------------------------------------------------
// SchlickDistribution
//------------------------------------------------------------------------------

float slg::SchlickDistribution_SchlickZ(const float roughness, const float cosNH) {
	const float cosNH2 = cosNH * cosNH;
	// expanded for increased numerical stability
	const float d = cosNH2 * roughness + (1.f - cosNH2);
	// use double division to avoid overflow in d*d product
	return (roughness / d) / d;
}

float slg::SchlickDistribution_SchlickA(const Vector &H, const float anisotropy) {
	const float h = sqrtf(H.x * H.x + H.y * H.y);
	if (h > 0.f) {
		const float w = (anisotropy > 0.f ? H.x : H.y) / h;
		const float p = 1.f - fabsf(anisotropy);
		return sqrtf(p / (p * p + w * w * (1.f - p * p)));
	}

	return 1.f;
}

float slg::SchlickDistribution_D(const float roughness, const Vector &wh,
		const float anisotropy) {
	const float cosTheta = fabsf(wh.z);
	return SchlickDistribution_SchlickZ(roughness, cosTheta) * SchlickDistribution_SchlickA(wh, anisotropy) * INV_PI;
}

float slg::SchlickDistribution_SchlickG(const float roughness,
		const float costheta) {
	return costheta / (costheta * (1.f - roughness) + roughness);
}

float slg::SchlickDistribution_G(const float roughness, const Vector &localFixedDir,
	const Vector &localSampledDir) {
	return SchlickDistribution_SchlickG(roughness, fabsf(localFixedDir.z)) *
			SchlickDistribution_SchlickG(roughness, fabsf(localSampledDir.z));
}

static float GetPhi(const float a, const float b) {
	return M_PI * .5f * sqrtf(a * b / (1.f - a * (1.f - b)));
}

void slg::SchlickDistribution_SampleH(const float roughness, const float anisotropy,
		const float u0, const float u1, Vector *wh, float *d, float *pdf) {
	float u1x4 = u1 * 4.f;
	const float cos2Theta = u0 / (roughness * (1 - u0) + u0);
	const float cosTheta = sqrtf(cos2Theta);
	const float sinTheta = sqrtf(1.f - cos2Theta);
	const float p = 1.f - fabsf(anisotropy);
	float phi;
	if (u1x4 < 1.f) {
		phi = GetPhi(u1x4 * u1x4, p * p);
	} else if (u1x4 < 2.f) {
		u1x4 = 2.f - u1x4;
		phi = M_PI - GetPhi(u1x4 * u1x4, p * p);
	} else if (u1x4 < 3.f) {
		u1x4 -= 2.f;
		phi = M_PI + GetPhi(u1x4 * u1x4, p * p);
	} else {
		u1x4 = 4.f - u1x4;
		phi = M_PI * 2.f - GetPhi(u1x4 * u1x4, p * p);
	}

	if (anisotropy > 0.f)
		phi += M_PI * .5f;

	*wh = Vector(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
	*d = SchlickDistribution_SchlickZ(roughness, cosTheta) * SchlickDistribution_SchlickA(*wh, anisotropy) * INV_PI;
	*pdf = *d;
}

float slg::SchlickDistribution_Pdf(const float roughness, const Vector &wh,
		const float anisotropy) {
	return SchlickDistribution_D(roughness, wh, anisotropy);
}

//------------------------------------------------------------------------------
// FresnelSlick BSDF
//------------------------------------------------------------------------------

Spectrum slg::FresnelSlick_Evaluate(const Spectrum &normalIncidence, const float cosi) {
	return normalIncidence + (Spectrum(1.f) - normalIncidence) *
		powf(1.f - cosi, 5.f);
}

//------------------------------------------------------------------------------
// FresnelGeneral material
//------------------------------------------------------------------------------

static Spectrum FrDiel2(const float cosi, const Spectrum &cost,
		const Spectrum &eta) {
	Spectrum Rparl(eta * cosi);
	Rparl = (cost - Rparl) / (cost + Rparl);
	Spectrum Rperp(eta * cost);
	Rperp = (Spectrum(cosi) - Rperp) / (Spectrum(cosi) + Rperp);

	return (Rparl * Rparl + Rperp * Rperp) * .5f;
}

//static Spectrum FrDiel(const float cosi, const float cost,
//		const Spectrum &etai, const Spectrum &etat) {
//	return FrDiel2(cosi, Spectrum(cost), etat / etai);
//}
//
//static Spectrum FrCond(const float cosi, const Spectrum &eta,
//		const Spectrum &k) {
//	const Spectrum tmp = (eta * eta + k*k) * (cosi * cosi) + 1;
//	const Spectrum Rparl2 = 
//		(tmp - (2.f * eta * cosi)) /
//		(tmp + (2.f * eta * cosi));
//	const Spectrum tmp_f = eta * eta + k*k + (cosi * cosi);
//	const Spectrum Rperp2 =
//		(tmp_f - (2.f * eta * cosi)) /
//		(tmp_f + (2.f * eta * cosi));
//	return (Rparl2 + Rperp2) * .5f;
//}

static Spectrum FrFull(const float cosi, const Spectrum &cost, const Spectrum &eta, const Spectrum &k) {
	const Spectrum tmp = (eta * eta + k * k) * (cosi * cosi) + (cost * cost);
	const Spectrum Rparl2 = (tmp - (2.f * cosi * cost) * eta) /
		(tmp + (2.f * cosi * cost) * eta);
	const Spectrum tmp_f = (eta * eta + k * k) * (cost * cost) + Spectrum(cosi * cosi);
	const Spectrum Rperp2 = (tmp_f - (2.f * cosi * cost) * eta) /
		(tmp_f + (2.f * cosi * cost) * eta);
	return (Rparl2 + Rperp2) * .5f;
}

Spectrum slg::FresnelGeneral_Evaluate(const Spectrum &eta, const Spectrum &k, const float cosi) {
	Spectrum sint2(Max(0.f, 1.f - cosi * cosi));
	if (cosi > 0.f)
		sint2 /= eta * eta;
	else
		sint2 *= eta * eta;
	sint2 = sint2.Clamp();

	const Spectrum cost2 = (Spectrum(1.f) - sint2);
	if (cosi > 0.f) {
		const Spectrum a(2.f * k * k * sint2);
		return FrFull(cosi, Sqrt((cost2 + Sqrt(cost2 * cost2 + a * a)) / 2.f), eta, k);
	} else {
		const Spectrum a(2.f * k * k * sint2);
		const Spectrum d2 = eta * eta + k * k;
		return FrFull(-cosi, Sqrt((cost2 + Sqrt(cost2 * cost2 + a * a)) / 2.f), eta / d2, -k / d2);
	}
}

Spectrum slg::FresnelCauchy_Evaluate(const float eta, const float cosi) {
	// Compute indices of refraction for dielectric
	const bool entering = (cosi > 0.f);

	// Compute _sint_ using Snell's law
	const float eta2 = eta * eta;
	const float sint2 = (entering ? 1.f / eta2 : eta2) *
		Max(0.f, 1.f - cosi * cosi);
	// Handle total internal reflection
	if (sint2 >= 1.f)
		return Spectrum(1.f);
	else
		return FrDiel2(fabsf(cosi), Spectrum(sqrtf(Max(0.f, 1.f - sint2))),
			entering ? eta : Spectrum(1.f / eta));
}
