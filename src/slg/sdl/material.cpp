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

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Material
//------------------------------------------------------------------------------

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

	const std::string name = GetName();
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

void MaterialDefinitions::DefineMaterial(const std::string &name, Material *newMat) {
	if (IsMaterialDefined(name)) {
		const Material *oldMat = GetMaterial(name);

		// Update name/material definition
		const u_int index = GetMaterialIndex(name);
		mats[index] = newMat;
		matsByName.erase(name);
		matsByName.insert(std::make_pair(name, newMat));

		// Update all possible reference to old material with the new one
		BOOST_FOREACH(Material *mat, mats)
			mat->UpdateMaterialReferences(oldMat, newMat);

		// Delete old material
		delete oldMat;
	} else {
		// Add the new material
		mats.push_back(newMat);
		matsByName.insert(std::make_pair(name, newMat));
	}
}

void MaterialDefinitions::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	BOOST_FOREACH(Material *mat, mats)
		mat->UpdateTextureReferences(oldTex, newTex);
}

Material *MaterialDefinitions::GetMaterial(const std::string &name) {
	// Check if the material has been already defined
	std::map<std::string, Material *>::const_iterator it = matsByName.find(name);

	if (it == matsByName.end())
		throw std::runtime_error("Reference to an undefined material: " + name);
	else
		return it->second;
}

u_int MaterialDefinitions::GetMaterialIndex(const std::string &name) {
	return GetMaterialIndex(GetMaterial(name));
}

u_int MaterialDefinitions::GetMaterialIndex(const Material *m) const {
	for (u_int i = 0; i < mats.size(); ++i) {
		if (m == mats[i])
			return i;
	}

	throw std::runtime_error("Reference to an undefined material: " + boost::lexical_cast<std::string>(m));
}

std::vector<std::string> MaterialDefinitions::GetMaterialNames() const {
	std::vector<std::string> names;
	names.reserve(mats.size());
	for (std::map<std::string, Material *>::const_iterator it = matsByName.begin(); it != matsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void MaterialDefinitions::DeleteMaterial(const std::string &name) {
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
	return Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI;
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
	return Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI;
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

	const std::string name = GetName();
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
	// The absCosSampledDir is used to compensate the other one used inside the integrator
	return Kr->GetSpectrumValue(hitPoint).Clamp() / (*absCosSampledDir);
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

	const std::string name = GetName();
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
			result = (Spectrum(1.f) - FresnelCauchy_Evaluate(ntc, costheta));

		result *= kt;
	} else {
		// Reflect
		*localSampledDir = Vector(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabsf(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	}

	// The absCosSampledDir is used to compensate the other one used inside the integrator
	return result / (*absCosSampledDir);
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

	const std::string name = GetName();
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

	// The absCosSampledDir is used to compensate the other one used inside the integrator
	return result / (*absCosSampledDir);
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

	const std::string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("archglass"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetName()));
	props.Set(Property("scene.materials." + name + ".ioroutside")(ousideIor->GetName()));
	props.Set(Property("scene.materials." + name + ".iorinside")(ior->GetName()));
	props.Set(Material::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

Spectrum MetalMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Vector MetalMaterial::GlossyReflection(const Vector &localFixedDir, const float exponent,
		const float u0, const float u1) {
	// Ray from outside going in ?
	const bool into = (localFixedDir.z > 0.f);
	const Vector shadeN(0.f, 0.f, into ? 1.f : -1.f);

	const float phi = 2.f * M_PI * u0;
	const float cosTheta = powf(1.f - u1, exponent);
	const float sinTheta = sqrtf(Max(0.f, 1.f - cosTheta * cosTheta));
	const float x = cosf(phi) * sinTheta;
	const float y = sinf(phi) * sinTheta;
	const float z = cosTheta;

	const Vector dir = -localFixedDir;
	const float dp = Dot(shadeN, dir);
	const Vector w = dir - (2.f * dp) * Vector(shadeN);

	const Vector u = Normalize(Cross(
			(fabsf(shadeN.x) > .1f) ? Vector(0.f, 1.f, 0.f) :  Vector(1.f, 0.f, 0.f),
			w));
	const Vector v = Cross(w, u);

	return x * u + y * v + z * w;
}

Spectrum MetalMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (GLOSSY | REFLECT)))
		return Spectrum();

	const float e = 1.f / (Max(exponent->GetFloatValue(hitPoint), 0.f) + 1.f);
	*localSampledDir = GlossyReflection(localFixedDir, e, u0, u1);

	if (localSampledDir->z * localFixedDir.z > 0.f) {
		*event = GLOSSY | REFLECT;
		*pdfW = 1.f;
		*absCosSampledDir = fabsf(localSampledDir->z);
		// The absCosSampledDir is used to compensate the other one used inside the integrator
		return Kr->GetSpectrumValue(hitPoint).Clamp() / (*absCosSampledDir);
	} else
		return Spectrum();
}

void MetalMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	exponent->AddReferencedTextures(referencedTexs);
}

void MetalMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (exponent == oldTex)
		exponent = newTex;
}

Properties MetalMaterial::ToProperties() const  {
	Properties props;

	const std::string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("metal"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Property("scene.materials." + name + ".exp")(exponent->GetName()));
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
	result *= weightFirst;

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

	return result;
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

	const std::string name = GetName();
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

	//throw std::runtime_error("Internal error, called NullMaterial::Sample()");

	*localSampledDir = -localFixedDir;
	*absCosSampledDir = 1.f;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return Spectrum(1.f);
}

Properties NullMaterial::ToProperties() const  {
	Properties props;

	const std::string name = GetName();
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
	const float absCosSampledDir = Dot(localLightDir, localEyeDir);

	const Spectrum r = Kr->GetSpectrumValue(hitPoint).Clamp();
	const Spectrum t = Kt->GetSpectrumValue(hitPoint).Clamp() * 
		// Energy conservation
		(Spectrum(1.f) - r);

	if (directPdfW)
		*directPdfW = .5f * fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * (.5f * INV_PI));

	if (reversePdfW)
		*reversePdfW = .5f * fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * (.5f * INV_PI));

	if (absCosSampledDir > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * INV_PI;
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * INV_PI;
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
		return kr * INV_PI;
	} else {
		*localSampledDir *= -Sgn(localFixedDir.z);
		*event = DIFFUSE | TRANSMIT;
		*pdfW *= (1.f - threshold);
		return kt * INV_PI;
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

	const std::string name = GetName();
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

	// The cosi is used to compensate the other one used inside the integrator
	factor /= cosi;
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
		S *= d * G / (4.f * coso) + 
				(multibounce ? cosi * Clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);
	else
		//CoatingF(sw, wo, *wi, f_);
		S *= d * G / (4.f * cosi) + 
				(multibounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);

	// The cosi is used to compensate the other one used inside the integrator
	return S / cosi;
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

	const Spectrum baseF = Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI;
	if (localEyeDir.z <= 0.f) {
		// Back face: no coating

		if (directPdfW)
			*directPdfW = fabsf(localSampledDir.z * INV_PI);

		if (reversePdfW)
			*reversePdfW = fabsf(localFixedDir.z * INV_PI);

		*event = GLOSSY | REFLECT;
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
	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
			(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

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
	const float wCoating = (localFixedDir.z <= 0.f) ? 0.f : SchlickBSDF_CoatingWeight(ks, localFixedDir);
	const float wBase = 1.f - wCoating;

	float basePdf, coatingPdf;
	Spectrum baseF, coatingF;

	if (passThroughEvent < wBase) {
		// Sample base BSDF (Matte BSDF)
		*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, &basePdf);

		*absCosSampledDir = fabsf(localSampledDir->z);
		if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return Spectrum();

		baseF = Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI;

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

		// Evaluate base BSDF (Matte BSDF)
		basePdf = fabsf((hitPoint.fromLight ? localFixedDir.z : localSampledDir->z) * INV_PI);
		baseF = Kd->GetSpectrumValue(hitPoint).Clamp() * INV_PI;

		*event = GLOSSY | REFLECT;
	}

	*pdfW = coatingPdf * wCoating + basePdf * wBase;
	if (localFixedDir.z > 0.f) {
		// Front face reflection: coating+base

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

		return coatingF + absorption * (Spectrum(1.f) - S) * baseF;
	} else {
		// Back face reflection: base

		return baseF;
	}
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

	const std::string name = GetName();
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
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	const Vector wh(Normalize(localFixedDir + localSampledDir));
	const float cosWH = Dot(localFixedDir, wh);

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localFixedDir, wh));

	if (reversePdfW)
		*reversePdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localSampledDir, wh));

	const Spectrum etaVal = n->GetSpectrumValue(hitPoint);
	const Spectrum kVal = k->GetSpectrumValue(hitPoint);
	const Spectrum F = FresnelGeneral_Evaluate(etaVal, kVal, cosWH);

	const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);

	const float coso = fabsf(localFixedDir.z);
	const float cosi = fabsf(localSampledDir.z);
	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;
	if (!hitPoint.fromLight)
		factor /= 4.f * coso;
	else
		factor /= 4.f * cosi;

	*event = GLOSSY | REFLECT;

	// The cosi is used to compensate the other one used inside the integrator
	factor /= cosi;
	return factor * F;
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

	*pdfW = specPdf / (4.f * cosWH);
	if (*pdfW <= 0.f)
		return Spectrum();

	const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);

	const Spectrum etaVal = n->GetSpectrumValue(hitPoint);
	const Spectrum kVal = k->GetSpectrumValue(hitPoint);
	const Spectrum F = FresnelGeneral_Evaluate(etaVal, kVal, cosWH);

	float factor = d * G;
	if (!hitPoint.fromLight)
		factor /= 4.f * coso;
	else
		factor /= 4.f * cosi;

	*event = GLOSSY | REFLECT;

	// The cosi is used to compensate the other one used inside the integrator
	factor /= cosi;
	return factor * F;
}

void Metal2Material::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;

	const float u = Clamp(nu->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;

	const Vector wh(Normalize(localFixedDir + localSampledDir));

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localFixedDir, wh));

	if (reversePdfW)
		*reversePdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localSampledDir, wh));
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

	const std::string name = GetName();
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

	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;

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
	if (Dot(localFixedDir, localSampledDir) < 0.f) {
		// Transmit

		const bool entering = (CosTheta(localFixedDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		Vector wh = eta * localFixedDir + localSampledDir;
		if (wh.z < 0.f)
			wh = -wh;

		const float lengthSquared = wh.LengthSquared();
		if (!(lengthSquared > 0.f))
			return Spectrum();
		wh /= sqrtf(lengthSquared);
		const float cosThetaI = fabsf(CosTheta(localSampledDir));
		const float cosThetaIH = AbsDot(localSampledDir, wh);
		const float cosThetaOH = Dot(localFixedDir, wh);

		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);

		if (directPdfW)
			*directPdfW = threshold * specPdf * fabsf(cosThetaOH) / lengthSquared;

		if (reversePdfW)
			*reversePdfW = threshold * specPdf * cosThetaIH * eta * eta / lengthSquared;

		Spectrum result = (fabsf(cosThetaOH) * cosThetaIH * D *
			G / (cosThetaI * lengthSquared)) *
			kt * (Spectrum(1.f) - F);

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wl). So I have to remove that term.
		result /= fabsf(CosTheta(localLightDir));
		return result;
	} else {
		// Reflect
		const float cosThetaO = fabsf(CosTheta(localFixedDir));
		const float cosThetaI = fabsf(CosTheta(localSampledDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f)
			return Spectrum();
		Vector wh = localFixedDir + localSampledDir;
		if (wh == Vector(0.f))
			return Spectrum();
		wh = Normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		float cosThetaH = Dot(localEyeDir, wh);
		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaH);

		if (directPdfW)
			*directPdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localFixedDir, wh));

		if (reversePdfW)
			*reversePdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localSampledDir, wh));

		Spectrum result = (D * G / (4.f * cosThetaI)) * kr * F;

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wl). So I have to remove that term.
		result /= fabsf(CosTheta(localLightDir));
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
		float factor = d * G * fabsf(cosThetaOH) / specPdf;

		if (!hitPoint.fromLight) {
			const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaIH);
			result = (factor / coso) * kt * (Spectrum(1.f) - F);
		} else {
			const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
			result = (factor / cosi) * kt * (Spectrum(1.f) - F);
		}

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wi)/pdf if reverse==true and by Dot(ns. wo)/pdf if reverse==false.
		// So I have to remove that terms.
		result *= *pdfW / ((!hitPoint.fromLight) ? cosi : coso);
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
		float factor = d * G * fabsf(cosThetaOH) / specPdf;

		const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		factor /= (!hitPoint.fromLight) ? coso : cosi;
		result = factor * F * kr;

		// This is a porting of LuxRender code and there, the result is multiplied
		// by Dot(ns, wi)/pdf if reverse==true and by Dot(ns. wo)/pdf if reverse==false.
		// So I have to remove that terms.
		result *= *pdfW / ((!hitPoint.fromLight) ? cosi : coso);
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

	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;

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
	if (Dot(localFixedDir, localSampledDir) < 0.f) {
		// Transmit

		const bool entering = (CosTheta(localFixedDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		Vector wh = eta * localFixedDir + localSampledDir;
		if (wh.z < 0.f)
			wh = -wh;

		const float lengthSquared = wh.LengthSquared();
		if (!(lengthSquared > 0.f))
			return;

		wh /= sqrtf(lengthSquared);
		const float cosThetaIH = AbsDot(localSampledDir, wh);
		const float cosThetaOH = Dot(localFixedDir, wh);

		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);

		if (directPdfW)
			*directPdfW = threshold * specPdf * fabsf(cosThetaOH) / lengthSquared;

		if (reversePdfW)
			*reversePdfW = threshold * specPdf * cosThetaIH * eta * eta / lengthSquared;
	} else {
		// Reflect
		const float cosThetaO = fabsf(CosTheta(localFixedDir));
		const float cosThetaI = fabsf(CosTheta(localSampledDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f)
			return;

		Vector wh = localFixedDir + localSampledDir;
		if (wh == Vector(0.f))
			return;
		wh = Normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);

		if (directPdfW)
			*directPdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localFixedDir, wh));

		if (reversePdfW)
			*reversePdfW = (1.f - threshold) * specPdf / (4.f * AbsDot(localSampledDir, wh));
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

	const std::string name = GetName();
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
// SchlickDistribution
//------------------------------------------------------------------------------

float slg::SchlickDistribution_SchlickZ(const float roughness, const float cosNH) {
	const float d = 1.f + (roughness - 1) * cosNH * cosNH;
	return (roughness > 0.f) ? (roughness / (d * d)) : INFINITY;
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
	const Spectrum tmp_f = (eta * eta + k * k) * (cost * cost) + (cosi * cosi);
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
			entering ? eta : Spectrum(1.f) / eta);
}
