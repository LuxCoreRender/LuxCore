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

#include "slg/materials/glossy2.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Glossy2 material
//
// LuxRender Glossy2 material porting.
//------------------------------------------------------------------------------

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
		if (localFixedDir.z < 0.f) {
			// Backface
			*directPdfW = fabsf(localSampledDir.z * INV_PI);
		} else {
			const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
			const float wBase = 1.f - wCoating;

			*directPdfW = wBase * fabsf(localSampledDir.z * INV_PI) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, localSampledDir);
		}
	}

	if (reversePdfW) {
		if (localSampledDir.z < 0.f) {
			// Backface
			*reversePdfW = fabsf(localFixedDir.z * INV_PI);
		} else {
			const float wCoatingR = SchlickBSDF_CoatingWeight(ks, localSampledDir);
			const float wBaseR = 1.f - wCoatingR;

			*reversePdfW = wBaseR * fabsf(localFixedDir.z * INV_PI) +
				wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localSampledDir, localFixedDir);
		}
	}

	if (localFixedDir.z < 0.f) {
		// Backface, no coating
		return baseF;
	}

	// Absorption
	const float cosi = fabsf(localSampledDir.z);
	const float coso = fabsf(localFixedDir.z);
	const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp();
	const float d = depth->GetFloatValue(hitPoint);
	const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const Vector H(Normalize(localFixedDir + localSampledDir));
	const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(localSampledDir, H));

	const Spectrum coatingF = SchlickBSDF_CoatingF(hitPoint.fromLight, ks, roughness, anisotropy, multibounce, localFixedDir, localSampledDir);

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
		*localSampledDir = -CosineSampleHemisphere(u0, u1, pdfW);

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
		coatingF = SchlickBSDF_CoatingF(hitPoint.fromLight, ks, roughness, anisotropy, multibounce, localFixedDir, *localSampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, *localSampledDir);

		*event = GLOSSY | REFLECT;
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(hitPoint.fromLight, ks, roughness, anisotropy, multibounce,
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
	const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const Vector H(Normalize(localFixedDir + *localSampledDir));
	const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(*localSampledDir, H));

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
		if (localFixedDir.z < 0.f) {
			// Backface
			*directPdfW = fabsf(localSampledDir.z * INV_PI);
		} else {
			const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
			const float wBase = 1.f - wCoating;

			*directPdfW = wBase * fabsf(localSampledDir.z * INV_PI) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, localSampledDir);
		}
	}

	if (reversePdfW) {
		if (localSampledDir.z < 0.f) {
			// Backface
			*reversePdfW = fabsf(localFixedDir.z * INV_PI);
		} else {
			const float wCoatingR = SchlickBSDF_CoatingWeight(ks, localSampledDir);
			const float wBaseR = 1.f - wCoatingR;

			*reversePdfW = wBaseR * fabsf(localFixedDir.z * INV_PI) +
				wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localSampledDir, localFixedDir);
		}
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
