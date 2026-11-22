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

#include "slg/materials/disney.h"
#include "slg/materials/thinfilmcoating.h"
#include "slg/textures/fresnel/fresneltexture.h"

using namespace std;
using namespace luxrays;
using namespace slg;

namespace {

inline Spectrum SanitizeSpectrum(const Spectrum &s) {
	if (s.IsNaN() || s.IsInf())
		return Spectrum(0.0f);
	return s;
}

// Medium IOR on the side of dir for interface at z = 0:
// exterior (cosTheta >= 0) has IOR 1.0f; interior (cosTheta < 0) has IOR safeEta >= 1.0f.
inline float MediumIor(const Vector &dir, const float safeEta) {
	return (CosTheta(dir) >= 0.0f) ? 1.0f : safeEta;
}

// Refracted half-vector h (oriented with h.z >= 0) for transition from dir1 (in medium eta1)
// to dir2 (in medium eta2) on opposite hemispheres: -(eta1 * dir1 + eta2 * dir2) normalized.
inline bool ComputeRefractedHalfVector(const Vector &dir1, const Vector &dir2,
		const float safeEta, Vector &htOut, float &eta1Out, float &eta2Out) {
	eta1Out = MediumIor(dir1, safeEta);
	eta2Out = MediumIor(dir2, safeEta);

	const Vector sum = -(eta1Out * dir1 + eta2Out * dir2);
	const float len2 = Dot(sum, sum);
	if (len2 <= 1e-12f)
		return false;

	Vector ht = sum / sqrtf(len2);
	if (CosTheta(ht) < 0.0f)
		ht = -ht;

	htOut = ht;
	return true;
}

}

DisneyMaterial::DisneyMaterial(
	TextureConstPtr frontTransp,
	TextureConstPtr backTransp,
	TextureConstPtr emitted,
	TextureConstPtr bump,
	TextureConstPtr baseColor,
	TextureConstPtr subsurface,
	TextureConstPtr roughness,
	TextureConstPtr metallic,
	TextureConstPtr specular,
	TextureConstPtr specularTint,
	TextureConstPtr clearcoat,
	TextureConstPtr clearcoatGloss,
	TextureConstPtr anisotropic,
	TextureConstPtr sheen,
	TextureConstPtr sheenTint,
    TextureConstPtr specTrans,
	TextureConstPtr ior,
	TextureConstPtr diffTrans,
	TextureConstPtr flatness,
	TextureConstPtr filmAmount,
	TextureConstPtr filmThickness,
	TextureConstPtr filmIor
) : Material(frontTransp, backTransp, emitted, bump), 
	BaseColor(baseColor), 
	Subsurface(subsurface),
	Roughness(roughness),
	Metallic(metallic),
	Specular(specular),
	SpecularTint(specularTint),
	Clearcoat(clearcoat),
	ClearcoatGloss(clearcoatGloss),
	Anisotropic(anisotropic),
	Sheen(sheen),
	SheenTint(sheenTint),
    SpecTrans(specTrans),
	Ior(ior),
	DiffTrans(diffTrans),
	Flatness(flatness),
	filmAmount(filmAmount),
	filmThickness(filmThickness),
	filmIor(filmIor) {
	UpdateGlossiness();
}

void DisneyMaterial::UpdateGlossiness() {
	// Disney glossiness requires a very special care because the material can be
	// matte, glossy, transmessive or specular and everything in between
    
	const float metallicFilter = Metallic ? Metallic->Filter() : 0.f;
	const float specularFilter = Specular ? Specular->Filter() : 0.f;
	const float specTransFilter = SpecTrans ? SpecTrans->Filter() : 0.f;
	if ((metallicFilter >= .5f) || (specularFilter >= .5f) || (specTransFilter >= .5f)) {
		const float g = Roughness ? ComputeGlossiness(Roughness) : 0.f;
		
		if (g > 0.f)
			glossiness = sqrtf(g);
		else
			glossiness = 0.f;
	} else
		glossiness = 1.f;
}

Spectrum DisneyMaterial::Albedo(const HitPoint &hitPoint) const {
	return BaseColor ? SanitizeSpectrum(BaseColor->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
}

Spectrum DisneyMaterial::Evaluate(
		const HitPoint &hitPoint,
		const Vector &localLightDir,
		const Vector &localEyeDir,
		BSDFEvent *event,
		float *directPdfW,
		float *reversePdfW) const  {
	const Spectrum color = BaseColor ? SanitizeSpectrum(BaseColor->GetSpectrumValue(hitPoint).Clamp(0.0f, 1.0f);
	const float subsurface = Subsurface ? Clamp(Subsurface->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float roughness = Roughness ? Clamp(Roughness->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float metallic = Metallic ? Clamp(Metallic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specular = Specular ? Clamp(Specular->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specularTint = SpecularTint ? Clamp(SpecularTint->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoat = Clearcoat ? Clamp(Clearcoat->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoatGloss = ClearcoatGloss ? Clamp(ClearcoatGloss->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float anisotropicGloss = Anisotropic ? Clamp(Anisotropic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	// I allow sheen values > 1.0 to accentuate the effect. The result is still
	// clamped between 0.0 and 1.0 to not break the energy conservation law.
	const float sheen = Sheen ? Max(0.0f, Sheen->GetFloatValue(hitPoint)) : 0.0f;
	const float sheenTint = SheenTint ? Clamp(SheenTint->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.5f;
	const float specTrans = SpecTrans ? Clamp(SpecTrans->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float eta = Ior ? Max(1.0f, Ior->GetFloatValue(hitPoint)) : 1.5f;
	const float diffTrans = DiffTrans ? Clamp(DiffTrans->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float flatness = Flatness ? Clamp(Flatness->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float localFilmAmount = filmAmount ? Clamp(filmAmount->GetFloatValue(hitPoint), 0.0f, 1.0f) : 1.f;
	const float localFilmThickness = filmThickness ? Max(0.0f, filmThickness->GetFloatValue(hitPoint) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && filmIor) ? Max(1.0f, filmIor->GetFloatValue(hitPoint)) : 1.f;

	const Spectrum bsdf = DisneyEvaluate(hitPoint.fromLight, color, subsurface, roughness, metallic, specular, specularTint,
			clearcoat, clearcoatGloss, anisotropicGloss, sheen, sheenTint, specTrans, eta, diffTrans,
			flatness, localFilmAmount, localFilmThickness, 
			localFilmIor, localLightDir, localEyeDir, event, directPdfW, reversePdfW);
			// Evaluate() follows LuxRender habit to return the result BSDF multiplied by cosThetaToLight once
			return SanitizeSpectrum(bsdf * fabsf(CosTheta(localLightDir)));
}

Spectrum DisneyMaterial::DisneyEvaluate(
		const bool fromLight,
		const Spectrum &color,
		const float subsurface,
		const float roughness,
		const float metallic,
		const float specular,
		const float specularTint,
		const float clearcoat,
		const float clearcoatGloss,
		const float anisotropicGloss,
		const float sheen,
		const float sheenTint,
        const float specTrans,
		const float eta,
		const float diffTrans,
		const float flatness,
		const float localFilmAmount,
		const float localFilmThickness,
		const float localFilmIor,
		const Vector &localLightDir,
		const Vector &localEyeDir,
		BSDFEvent *event,
		float *directPdfW,
		float *reversePdfW) const {
	if (Dot(localEyeDir, localEyeDir) <= 0.0f || Dot(localLightDir, localLightDir) <= 0.0f) {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
		return Spectrum();
	}
	const Vector wo = Normalize(localEyeDir); 
	const Vector wi = Normalize(localLightDir);

	const float NdotL = fabsf(CosTheta(wwo);
	const float cosThetaI = CosTheta(wi);
	const float NdotL = fabsf(cosThetaI);
	const float NdotV = fabsf(CosThetaO);

	if (NdotL < DEFAULT_COS_EPSILON_STATIC || NdotV < DEFAULT_COS_EPSILON_STATIC)
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
		return Spectrum();
    }

	const bool isTransmit = (cosThetaO * cosThetaI) < 0.0f;

	DisneyPdf(fromLight, roughness, metallic, clearcoat, clearcoatGloss,
			anisotropicGloss, specTrans, eta, localLightDir, localEyeDir, directPdfW, reversePdfW);

	const float nonMetallic = 1.0f - metallic;
	const float dielectricWeight = nonMetallic * (1.0f - specTrans);

	if (isTransmit) {
		if (event)
			*event = GLOSSY | TRANSMIT;

		Spectrum f(0.0f);
		if (nonMetallic > 0.0f && specTrans > 0.0f)
			f += nonMetallic * specTrans * SanitizeSpectrum(DisneyGlassTransmit(fromLight, color, roughness, eta, wi, wo));
		if (dielectricWeight > 0.0f && diffTrans > 0.0f)
			f += dielectricWeight * diffTrans * INV_PI * color;

		return SanitizeSpectrum(f);
	}

	const Vector sumH = wo + wi;
	if (Dot(sumH, sumH) <= 1e-10f)
		return Spectrum();

	const Vector H = Normalize(sumH);

	const float NdotH = fabsf(CosTheta(H));
	const float LdotH = Clamp(Dot(wi, H), 0.0f, 1.0f);
	const float VdotH = Clamp(Dot(wo, H), 0.0f, 1.0f);

	const Spectrum diffuseEval = SanitizeSpectrum(DisneyDiffuse(color, roughness, flatness, NdotL, NdotV, LdotH));

	const Spectrum subsurfaceEval = SanitizeSpectrum(DisneySubsurface(color, roughness, NdotL, NdotV, LdotH));

	Spectrum metallicEval = SanitizeSpectrum(DisneyMetallic(color, specular, specularTint, metallic, anisotropicGloss, roughness,
										   NdotL, NdotV, NdotH, LdotH, VdotH, wi, wo, H));
	
	if (localFilmThickness > 0.f) {
		const Spectrum filmCol = SanitizeSpectrum(CalcFilmColor(wo, localFilmThickness, localFilmIor));
		const Spectrum metallicWithFilmColor = SanitizeSpectrum(metallicEval * filmCol);
		metallicEval = Lerp(localFilmAmount, metallicEval, metallicWithFilmColor);
	}
	
	const Spectrum glassReflEval = (nonMetallic > 0.0f && specTrans > 0.0f) ?
			SanitizeSpectrum(DisneyGlassReflect(roughness, eta, wi, wo, NdotL, NdotV, NdotH, VdotH)) :
			Spectrum(0.0f);

	const Spectrum clearCoatEval = SanitizeSpectrum(Spectrum(DisneyClearCoat(clearcoat, clearcoatGloss,
			NdotL, NdotV, NdotH, LdotH)));

	const Spectrum sheenEval = SanitizeSpectrum(DisneySheen(color, sheen, sheenTint, LdotH));

	if (event)
		*event = GLOSSY | REFLECT;

	const Spectrum baseDiffuse = Lerp(subsurface, diffuseEval, subsurfaceEval);
	const Spectrum f = dielectricWeight * ((1.0f - diffTrans) * baseDiffuse + sheenEval) +
			metallic * metallicEval +
			nonMetallic * specTrans * glassReflEval +
			clearCoatEval;

	return SanitizeSpectrum(f);
}

Spectrum DisneyMaterial::DisneyDiffuse(const Spectrum &color, const float roughness,
		const float flatness, const float NdotL, const float NdotV, const float LdotH) const {
	const float FL = Schlick_Weight(NdotL);
	const float FV = Schlick_Weight(NdotV);

	const float Fd90 = 0.5f + 2.0f * Sqr(LdotH) * roughness;
	const float Fd = Lerp(FL, 1.0f, Fd90) * Lerp(FV, 1.0f, Fd90);

	// Disney 2015 retro-reflection term
	const float retro = 2.0f * roughness * Sqr(LdotH) * (FL + FV + FL * FV * (2.0f * roughness * Sqr(LdotH) - 1.0f));
	const float burley = INV_PI * (Fd * (1.0f - 0.5f * FL) * (1.0f - 0.5f * FV) + retro);
	const float lambert = INV_PI;

	return SanitizeSpectrum(Lerp(flatness, burley, lambert) * color);
}

Spectrum DisneyMaterial::DisneySubsurface(const Spectrum &color, const float roughness,
		const float NdotL, const float NdotV, const float LdotH) const {
	const float FL = Schlick_Weight(NdotL);
	const float FV = Schlick_Weight(NdotV);

	const float Fss90 = Sqr(LdotH) * roughness;
	const float Fss = Lerp(FL, 1.0f, Fss90) * Lerp(FV, 1.0f, Fss90);
	const float denom = Max(NdotL + NdotV, 1e-4f);
	const float ss = 1.25f * (Fss * (1.0f / denom - 0.5f) + 0.5f);

	return SanitizeSpectrum(INV_PI * ss * color);
}

Spectrum DisneyMaterial::DisneyMetallic(const Spectrum &color,
		const float specular, const float specularTint, const float metallic,
		const float anisotropic, const float roughness,
		const float NdotL, const float NdotV, const float NdotH,
		const float LdotH, const Vector &wi, const Vector &wo, const Vector &H) const {
	const Spectrum Ctint = CalculateTint(color);

	const Spectrum CSpecTint = specular * 0.08f * Lerp(specularTint, Spectrum(1.0f), Ctint);
	const Spectrum Cspec0 = Lerp(metallic, CSpecTint, color);

	float ax, ay;
	Anisotropic_Params(anisotropic, roughness, ax, ay);

	const float Ds = GTR2_Aniso(NdotH, H.x, H.y, ax, ay);

	const float FH = Schlick_Weight(LdotH);

	const Spectrum Fs = Lerp(FH, Cspec0, Spectrum(1.0f));

	const float Gl = SmithG_GGX_Aniso(NdotL, wi.x, wi.y, ax, ay);
	const float Gv = SmithG_GGX_Aniso(NdotV, wo.x, wo.y, ax, ay);

	const float Gs = Gl * Gv;

	return SanitizeSpectrum(Gs * Fs * Ds);
}

Spectrum DisneyMaterial::DisneyGlassReflect(const float roughness, const float eta,
		const Vector &wi, const Vector &wo,
		const float NdotL, const float NdotV, const float NdotH, const float VdotH) const {
	const float safeEta = Max(1.0f, eta);
	const float eta1 = MediumIor(wo, safeEta);
	const float eta2 = MediumIor(wi, safeEta);
	const float alpha = Max(0.001f, Sqr(roughness));
	const float D = GTR2_Aniso(NdotH, 0.0f, 0.0f, alpha, alpha);
	const float G = SmithG_GGX(NdotL, alpha) * SmithG_GGX(NdotV, alpha);
	const float F = DielectricFresnel(Clamp(fabsf(VdotH), 0.0f, 1.0f), eta1, eta2);

	return SanitizeSpectrum(Spectrum(F * D * G / Max(4.0f * NdotL * NdotV, 1e-5f)));
}

Spectrum DisneyMaterial::DisneyGlassTransmit(const bool fromLight, const Spectrum &color,
		const float roughness, const float eta,
		const Vector &wi, const Vector &wo) const {
	const float safeEta = Max(1.0f, eta);

	Vector ht;
	float etaO, etaI;
	if (!ComputeRefractedHalfVector(wo, wi, safeEta, ht, etaO, etaI))
		return Spectrum(0.0f);

	const float OdotH = Dot(wo, ht);
	const float IdotH = Dot(wi, ht);
	if ((OdotH * IdotH) >= 0.0f)
		return Spectrum(0.0f);

	const float denom = etaO * OdotH + etaI * IdotH;
	const float denom2 = denom * denom;
	if (denom2 <= 1e-10f)
		return Spectrum(0.0f);

	const float NdotO = Max(fabsf(CosTheta(wo)), 1e-5f);
	const float NdotI = Max(fabsf(CosTheta(wi)), 1e-5f);
	const float NdotH = fabsf(CosTheta(ht));
	const float alpha = Max(0.001f, Sqr(roughness));

	const float D = GTR2_Aniso(NdotH, 0.0f, 0.0f, alpha, alpha);
	const float G = SmithG_GGX(NdotI, alpha) * SmithG_GGX(NdotO, alpha);
	const float F = DielectricFresnel(Clamp(fabsf(OdotH), 0.0f, 1.0f), etaO, etaI);

	// Radiance / solid-angle refractive factor follows light propagation direction:
	// light travels from wi (medium etaI) to wo (medium etaO) when !fromLight,
	// and from wo (medium etaO) to wi (medium etaI) when fromLight.
	const float etaScale = fromLight ? (etaI * etaI) : (etaO * etaO);
	const float microGeom = (fabsf(IdotH) * fabsf(OdotH)) / (NdotI * NdotO);
	const float bsdfScalar = microGeom * etaScale * (1.0f - F) * D * G / denom2;

	return SanitizeSpectrum(color * bsdfScalar);
}

float DisneyMaterial::DisneyClearCoat(const float clearcoat, const float clearcoatGloss,
		float NdotL, float NdotV, float NdotH, float LdotH) const {
	const float gloss = Lerp(clearcoatGloss, 0.1f, 0.001f);

	const float Dr = GTR1(fabsf(NdotH), gloss);
	const float FH = Schlick_Weight(LdotH);
	const float Fr = Lerp(FH, 0.04f, 1.0f);
	const float Gr = SmithG_GGX(NdotL, 0.25f) * SmithG_GGX(NdotV, 0.25f);

	return clearcoat * Fr * Gr * Dr;
}

Spectrum DisneyMaterial::DisneySheen(const Spectrum &color, const float sheen,
		const float sheenTint, float LdotH) const {
	const float FH = Schlick_Weight(LdotH);

	const Spectrum Ctint = CalculateTint(color);
	const Spectrum Csheen = Lerp(sheenTint, Spectrum(1.0f), Ctint);

	return SanitizeSpectrum(FH * sheen * Csheen);
}

Spectrum DisneyMaterial::Sample(
		const HitPoint &hitPoint,
		const Vector &localFixedDir,
		Vector *localSampledDir,
		const float u0,
		const float u1,
		const float passThroughEvent,
		float *pdfW,
		BSDFEvent *event) const {
	if (!localSampledDir || !pdfW || Dot(localFixedDir, localFixedDir) <= 0.0f) {
		if (pdfW)
			*pdfW = 0.f;
		return Spectrum();
	}

	const Spectrum color = BaseColor ? SanitizeSpectrum(BaseColor->GetSpectrumValue(hitPoint).Clamp(0.0f, 1.0f);
	const float subsurface = Subsurface ? Clamp(Subsurface->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float roughness = Roughness ? Clamp(Roughness->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float metallic = Metallic ? Clamp(Metallic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specular = Specular ? Clamp(Specular->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specularTint = SpecularTint ? Clamp(SpecularTint->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoat = ClearCoat ? Clamp(Clearcoat->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoatGloss = ClearcoatGloss ? Clamp(ClearcoatGloss->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float anisotropicGloss = Anisotropic ? Clamp(Anisotropic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	// I allow sheen values > 1.0 to accentuate the effect. The result is still
	// clamped between 0.0 and 1.0 to not break the energy conservation law.
	const float sheen = Sheen ? Max(0.0f, Sheen->GetFloatValue(hitPoint);
	const float sheenTint = SheenTint ? Clamp(SheenTint->GetFloatValue(hitPoint), 0.0f, 1.0f);
    const float specTrans = SpecTrans ? Clamp(SpecTrans->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float eta = Ior ? Max(1.0f, Ior->GetFloatValue(hitPoint)) : 1.5f;
	const float diffTrans = DiffTrans ? Clamp(DiffTrans->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float flatness = Flatness ? Clamp(Flatness->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;

	const Vector wo = Normalize(localFixedDir);
	bool isTransmit = false;

	float ratioGlossy, ratioDiffuse, ratioClearcoat, ratioTrans;
	ComputeRatio(metallic, clearcoat, specTrans, ratioGlossy, ratioDiffuse, ratioClearcoat, ratioTrans);

	const float e = Clamp(passThroughEvent, 0.0f, 0.999999f);
	if (e < ratioGlossy)
		*localSampledDir = DisneyMetallicSample(anisotropicGloss, roughness, wo, u0, u1);
	else if (e < ratioGlossy + ratioClearcoat)
		*localSampledDir = DisneyClearcoatSample(clearcoatGloss, wo, u0, u1);
	else if (e < ratioGlossy + ratioClearcoat + ratioDiffuse)
		*localSampledDir = DisneyDiffuseSample(wo, u0, u1);
	else if (e < ratioGlossy + ratioClearcoat + ratioDiffuse + ratioTrans)
		*localSampledDir = DisneyGlassSample(roughness, eta, wo, u0, u1, &isTransmit);
	else {
		*pdfW = 0.f;
		return Spectrum();
	}
	
	*localSampledDir = Normalize(*localSampledDir);

	const Vector &localLightDir = hitPoint.fromLight ? wo : *localSampledDir;
	const Vector &localEyeDir = hitPoint.fromLight ? *localSampledDir : wo;

	const float NdotL = fabsf(CosTheta(localLightDir));
	const float NdotV = fabsf(CosTheta(localEyeDir));
	if (NdotL < DEFAULT_COS_EPSILON_STATIC || NdotV < DEFAULT_COS_EPSILON_STATIC) {
		*pdfW = 0.f;
		return Spectrum();

	
	const bool sampledTransmit = (CosTheta(localLightDir) * CosTheta(localEyeDir)) < 0.0f;
	if (event)
		*event = sampledTransmit ? (GLOSSY | TRANSMIT) : (GLOSSY | REFLECT);
	
	DisneyPdf(hitPoint.fromLight, roughness, metallic, clearcoat, clearcoatGloss, anisotropicGloss,
			specTrans, eta, localLightDir, localEyeDir, pdfW, nullptr);
    if (*pdfW <= 1e-5f || !(*pdfW < 1e20f)) {
		*pdfW = 0.f;
		return Spectrum();
	}

	const float localFilmAmount = filmAmount ? Clamp(filmAmount->GetFloatValue(hitPoint), 0.0f, 1.0f) : 1.f;
	const float localFilmThickness = filmThickness ? Max(0.0f, filmThickness->GetFloatValue(hitPoint)) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && filmIor) ? Max(1.0f, filmIor->GetFloatValue(hitPoint)) : 1.f;

	const Spectrum f = DisneyEvaluate(hitPoint.fromLight, color, subsurface, roughness,
			metallic, specular, specularTint, clearcoat, clearcoatGloss,
			anisotropicGloss, sheen, sheenTint, specTrans, eta, diffTrans, flatness,
			localFilmAmount, localFilmThickness, localFilmIor,
			localLightDir, localEyeDir, nullptr, nullptr, nullptr);

	const Spectrum contrib = SanitizeSpectrum((f * NdotL) / *pdfW);
	if (contrib.IsNaN() || contrib.IsInf()) {
		*pdfW = 0.f;
		return Spectrum();
	}

	return contrib;
}

Vector DisneyMaterial::DisneyDiffuseSample(const Vector &wo, float u0, float u1) const {
	return Sgn(CosTheta(wo)) * CosineSampleHemisphere(u0, u1);
}

Vector DisneyMaterial::DisneyMetallicSample(const float anisotropic, const float roughness,
		const Vector &wo, float u0, float u1) const {
	float ax, ay;
	Anisotropic_Params(anisotropic, roughness, ax, ay);

	const float clampedU0 = Clamp(u0, 0.0f, 0.999999f);
	const float clampedU1 = Clamp(u1, 0.0f, 0.999999f);

	const float angle = 2.0f * M_PI * clampedU1;
	const float phi = atan2f(ay * sinf(angle), ax * cosf(angle));

	const float sinPhi = sinf(phi), cosPhi = cosf(phi);
	const float ax2 = ax * ax, ay2 = ay * ay;
	const float alpha2 = 1.0f / Max(cosPhi * cosPhi / ax2 + sinPhi * sinPhi / ay2, 1e-8f);
	const float oneMinusU0 = Max(1.0f - clampedU0, 1e-7f);
	const float tanTheta2 = alpha2 * clampedU0 / oneMinusU0;
	const float cosTheta = 1.0f / sqrtf(1.0f + tanTheta2);

	const float sinTheta = sqrtf(Max(0.0f, 1.0f - cosTheta * cosTheta));
	Vector wh = Vector(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);

	const float denom = Dot(wh, wo);
	if (fabsf(denom) <= 1e-6f)
		return Vector();

	const Vector reflected = 2.0f * denom * wh - wo;
	if (Dot(reflected, reflected) <= 1e-10f)
		return Vector();

	return Normalize(reflected);
}

Vector DisneyMaterial::DisneyClearcoatSample(const float clearcoatGloss,
		const Vector &wo, float u0, float u1) const {
	const float a = Clamp(Lerp(clearcoatGloss, 0.1f, 0.001f), 0.001f, 0.999f);
	const float a2 = a * a;
	const float clampedU0 = Clamp(u0, 0.0f, 0.999999f);
	const float clampedU1 = Clamp(u1, 0.0f, 0.999999f);

	// Exact GTR1 microfacet normal elevation: cos^2(theta_h) = (a^(2*u0) - 1) / (a^2 - 1)
	const float denom = a2 - 1.0f;
	float cos2Theta = 0.0f;
	if (fabsf(denom) > 1e-7f)
		cos2Theta = Clamp((powf(a2, clampedU0) - 1.0f) / denom, 0.0f, 1.0f);
	else
		cos2Theta = clampedU0;

	const float cosTheta = sqrtf(cos2Theta);
	const float sinTheta = sqrtf(Max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi = 2.0f * M_PI * clampedU1;

	Vector wh = Vector(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
	if (CosTheta(wo) * CosTheta(wh) <= 0.0f)
		wh *= -1.f;

	const Vector reflected = 2.0f * Dot(wh, wo) * wh - wo;
	if (Dot(reflected, reflected) <= 1e-10f)
		return Vector();

	return Normalize(reflected);
}

Vector DisneyMaterial::DisneyGlassSample(const float roughness, const float eta,
		const Vector &wo, float u0, float u1, bool *isTransmit) const {
	const float alpha = Max(0.001f, Sqr(roughness));
	const float clampedU0 = Clamp(u0, 0.0f, 0.999999f);
	const float clampedU1 = Clamp(u1, 0.0f, 0.999999f);

	const float safeEta = Max(1.0f, eta);
	const float cosThetaO = CosTheta(wo);
	const float eta1 = (cosThetaO >= 0.0f) ? 1.0f : safeEta;
	const float eta2 = (cosThetaO >= 0.0f) ? safeEta : 1.0f;

	// Sample microfacet normal wh from isotropic GGX: p(wh) = D(wh) * |wh.z|
	const float alpha2 = alpha * alpha;
	const float oneMinusU0 = Max(1.0f - clampedU0, 1e-7f);
	const float denomU0 = Max(oneMinusU0 + alpha2 * clampedU0, 1e-7f);
	const float cosTheta = sqrtf(Clamp(oneMinusU0 / denomU0, 0.0f, 1.0f));
	const float sinTheta = sqrtf(Max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi = 2.0f * M_PI * clampedU1;

	Vector wh = Vector(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
	if (Dot(wh, wh) <= 1e-10f)
		return Vector();
	wh = Normalize(wh);

	if (CosTheta(wo) * CosTheta(wh) < 0.0f)
		wh = -wh;

	const float cosThetaI = Clamp(fabsf(Dot(wo, wh)), 0.0f, 1.0f);
	const float f = DielectricFresnel(cosThetaI, eta1, eta2);

	// Partition u0 cleanly between reflection (u0 < f) and refraction (u0 >= f)
	if (clampedU0 < f) {
		if (isTransmit)
			*isTransmit = false;
		const Vector reflected = 2.0f * Dot(wo, wh) * wh - wo;
		return (Dot(reflected, reflected) > 1e-10f) ? Normalize(reflected) : Vector();
	}

	const float relEta = eta1 / eta2;
	const float sin2ThetaI = Max(0.0f, 1.0f - Sqr(cosThetaI));
	const float sin2ThetaT = Sqr(relEta) * sin2ThetaI;
	if (sin2ThetaT >= 1.0f) {
		if (isTransmit)
			*isTransmit = false;
		const Vector reflected = 2.0f * Dot(wo, wh) * wh - wo;
		return (Dot(reflected, reflected) > 1e-10f) ? Normalize(reflected) : Vector();
	}

	const float cosThetaT = sqrtf(Max(0.0f, 1.0f - sin2ThetaT));
	const Vector m = (Dot(wo, wh) >= 0.0f) ? wh : -wh;
	const Vector refracted = relEta * (-wo) + (relEta * Dot(wo, m) - cosThetaT) * m;
	if (Dot(refracted, refracted) <= 1e-10f)
		return Vector();

	if (isTransmit)
		*isTransmit = true;
	return Normalize(refracted);
}

void DisneyMaterial::Pdf(
		const HitPoint &hitPoint,
		const Vector &localLightDir, 
		const Vector &localEyeDir,
		float *directPdfW, 
		float *reversePdfW) const {
	const float roughness = Roughness ? Clamp(Roughness->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.5f;
	const float metallic = Metallic ? Clamp(Metallic->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float clearcoat = Clearcoat ? Clamp(Clearcoat->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float clearcoatGloss = ClearcoatGloss ? Clamp(ClearcoatGloss->GetFloatValue(hitPoint), 0.0f, 1.0f) : 1.0f;
	const float anisotropicGloss = Anisotropic ? Clamp(Anisotropic->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float specTrans = SpecTrans ? Clamp(SpecTrans->GetFloatValue(hitPoint), 0.0f, 1.0f) : 0.0f;
	const float eta = Ior ? Max(1.0f, Ior->GetFloatValue(hitPoint)) : 1.5f;

	DisneyPdf(hitPoint.fromLight, roughness, metallic, clearcoat, clearcoatGloss,
			anisotropicGloss, specTrans, eta, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

void DisneyMaterial::DisneyPdf(const bool fromLight,
		const float roughness, const float metallic,
		const float clearcoat, const float clearcoatGloss, const float anisotropic,
        const float specTrans, const float eta,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW,  float *reversePdfW) const {
	if (Dot(localLightDir, localLightDir) <= 0.0f || Dot(localEyeDir, localEyeDir) <= 0.0f) {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;

		return;
	}

	const bool isTransmit = (CosTheta(localLightDir) * CosTheta(localEyeDir)) < 0.0f;

	float ratioGlossy, ratioDiffuse, ratioClearcoat, ratioTrans;
	ComputeRatio(metallic, clearcoat, specTrans, ratioGlossy, ratioDiffuse, ratioClearcoat, ratioTrans);

	if (isTransmit) {
		float glassDirectPdfW = 0.f, glassReversePdfW = 0.f;
		GlassPdf(fromLight, roughness, eta, localLightDir, localEyeDir, &glassDirectPdfW, &glassReversePdfW);

		const float safeEta = Max(1.0f, eta);
		const Vector &sampledDir = fromLight ? localEyeDir : localLightDir;
		const Vector &fixedDir = fromLight ? localLightDir : localEyeDir;

		Vector htFwd;
		float eta1Fwd, eta2Fwd;
		float oneMinusFFwd = 1.0f;
		if (ComputeRefractedHalfVector(fixedDir, sampledDir, safeEta, htFwd, eta1Fwd, eta2Fwd)) {
			const float cosThetaI = Clamp(fabsf(Dot(fixedDir, htFwd)), 0.0f, 1.0f);
			oneMinusFFwd = Max(0.0f, 1.0f - DielectricFresnel(cosThetaI, eta1Fwd, eta2Fwd));
		}

		Vector htRev;
		float eta1Rev, eta2Rev;
		float oneMinusFRev = 1.0f;
		if (ComputeRefractedHalfVector(sampledDir, fixedDir, safeEta, htRev, eta1Rev, eta2Rev)) {
			const float cosThetaI = Clamp(fabsf(Dot(sampledDir, htRev)), 0.0f, 1.0f);
			oneMinusFRev = Max(0.0f, 1.0f - DielectricFresnel(cosThetaI, eta1Rev, eta2Rev));
		}

		if (directPdfW)
			*directPdfW = ratioTrans * oneMinusFFwd * glassDirectPdfW;
		if (reversePdfW)
			*reversePdfW = ratioTrans * oneMinusFRev * glassReversePdfW;
		return;
	}

	float diffuseDirectPdfW, diffuseReversePdfW;
	DiffusePdf(fromLight, localLightDir, localEyeDir, &diffuseDirectPdfW, &diffuseReversePdfW);

	float metallicDirectPdfW, metallicReversePdfW;
	MetallicPdf(fromLight, anisotropic, roughness, localLightDir, localEyeDir, &metallicDirectPdfW, &metallicReversePdfW);

	float clearcoatDirectPdfW, clearcoatReversePdfW;
	ClearcoatPdf(fromLight, clearcoatGloss, localLightDir, localEyeDir, &clearcoatDirectPdfW, &clearcoatReversePdfW);

	float glassReflDirectPdfW = 0.f, glassReflReversePdfW = 0.f;
	if (specTrans > 0.0f && (1.0f - metallic) > 0.0f) {
		const Vector sumWh = localEyeDir + localLightDir;
		if (Dot(sumWh, sumWh) > 1e-10f) {
			const Vector wh = Normalize(sumWh);
			const float safeEta = Max(1.0f, eta);
			const float alpha = Max(0.001f, Sqr(roughness));
			const float NdotH = fabsf(CosTheta(wh));
			const float d = GTR2_Aniso(NdotH, 0.0f, 0.0f, alpha, alpha);
			const float eyeDotWh = Dot(localEyeDir, wh);
			const float lightDotWh = Dot(localLightDir, wh);
			const float etaEye = MediumIor(localEyeDir, safeEta);
			const float etaLight = MediumIor(localLightDir, safeEta);
			const float fEye = DielectricFresnel(Clamp(fabsf(eyeDotWh), 0.0f, 1.0f), etaEye, etaLight);
			const float fLight = DielectricFresnel(Clamp(fabsf(lightDotWh), 0.0f, 1.0f), etaLight, etaEye);
			if (fabsf(eyeDotWh) > 1e-5f)
				glassReflDirectPdfW = fEye * d * NdotH / (4.0f * fabsf(eyeDotWh));
			if (fabsf(lightDotWh) > 1e-5f)
				glassReflReversePdfW = fLight * d * NdotH / (4.0f * fabsf(lightDotWh));
		}
	}

	if (directPdfW)
		*directPdfW = ratioDiffuse * diffuseDirectPdfW + ratioGlossy * metallicDirectPdfW +
				ratioClearcoat * clearcoatDirectPdfW + ratioTrans * glassReflDirectPdfW;
	if (reversePdfW)
		*reversePdfW = ratioDiffuse * diffuseReversePdfW + ratioGlossy * metallicReversePdfW +
				ratioClearcoat * clearcoatReversePdfW + ratioTrans * glassReflReversePdfW;
}

void DisneyMaterial::DiffusePdf(const bool fromLight,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {

	const Vector &localFixedDir = fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = fromLight ? localEyeDir : localLightDir;
	if (directPdfW)
		*reversePdfW = fabsf(CosTheta(localFixedDir)) * INV_PI;
}

void DisneyMaterial::MetallicPdf(const bool fromLight,
		const float anisotropic, const float roughness,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Vector sumWh = localEyeDir + localLightDir;
	if (Dot(sumWh, sumWh) <= 1e-10f) {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
		return;
	}

	const Vector wh = Normalize(sumWh);

	float ax, ay;
	Anisotropic_Params(anisotropic, roughness, ax, ay);

	const float ax2 = ax * ax;
	const float ay2 = ay * ay;

	const float HdotX = wh.x;
	const float HdotY = wh.y;
	const float NdotH = fabsf(CosTheta(wh));

	const float denom = HdotX * HdotX / ax2 + HdotY * HdotY / ay2 + NdotH * NdotH;
	if (denom <= 1e-8f) {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;

		return;
	}

	const float pdfDistribution = NdotH / (M_PI * ax * ay * denom * denom);

	const Vector &localFixedDir = fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = fromLight ? localEyeDir : localLightDir;
	const float fixedDotWh = Dot(localFixedDir, wh);
	const float sampledDotWh = Dot(localSampledDir, wh);
	if (directPdfW)
		*directPdfW = (fabsf(fixedDotWh) > 1e-5f) ? (pdfDistribution / (4.0f * fabsf(fixedDotWh))) : 0.f;
	if (reversePdfW)
		*reversePdfW = (fabsf(sampledDotWh) > 1e-5f) ? (pdfDistribution / (4.0f * fabsf(sampledDotWh))) : 0.f;
}

void DisneyMaterial::ClearcoatPdf(const bool fromLight, const float clearcoatGloss,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Vector sumWh = localLightDir + localEyeDir;
	if (Dot(sumWh, sumWh) <= 1e-10f) {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
		return;
	}

	const Vector wh = Normalize(sumWh);

	const float NdotH = fabsf(CosTheta(wh));
	const float Dr = GTR1(NdotH, Lerp(clearcoatGloss, 0.1f, 0.001f));

	const Vector &localFixedDir = fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = fromLight ? localEyeDir : localLightDir;
	const float fixedDotWh = Dot(localFixedDir, wh);
	const float sampledDotWh = Dot(localSampledDir, wh);

	if (directPdfW)
		*directPdfW = (fabsf(fixedDotWh) > 1e-5f) ? (Dr * NdotH / (4.0f * fabsf(fixedDotWh))) : 0.f;
	if (reversePdfW)
		*reversePdfW = (fabsf(sampledDotWh) > 1e-5f) ? (Dr * NdotH / (4.0f * fabsf(sampledDotWh))) : 0.f;
}

void DisneyMaterial::GlassPdf(const bool fromLight, const float roughness, const float eta,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = 0.f;
	if (reversePdfW)
		*reversePdfW = 0.f;

	const float safeEta = Max(1.0f, eta);
	const float alpha = Max(0.001f, Sqr(roughness));

	// Jacobian for refraction from d1 (medium eta1) to d2 (medium eta2):
	// |d(wh)/d(d2)| = (eta2^2 * |d2 . ht|) / (eta1 * (d1 . ht) + eta2 * (d2 . ht))^2
	auto evalRefractPdf = [&](const Vector &d1, const Vector &d2) -> float {
		Vector ht;
		float eta1, eta2;
		if (!ComputeRefractedHalfVector(d1, d2, safeEta, ht, eta1, eta2))
			return 0.0f;

		const float d1DotH = Dot(d1, ht);
		const float d2DotH = Dot(d2, ht);
		if ((d1DotH * d2DotH) >= 0.0f)
			return 0.0f;

		const float denom = eta1 * d1DotH + eta2 * d2DotH;
		const float denom2 = denom * denom;
		if (denom2 <= 1e-10f)
			return 0.0f;

		const float NdotH = fabsf(CosTheta(ht));
		const float d = GTR2_Aniso(NdotH, 0.0f, 0.0f, alpha, alpha);
		const float pdfH = d * NdotH;
		const float dwh_dw2 = (eta2 * eta2 * fabsf(d2DotH)) / denom2;
		const float pdfW = pdfH * dwh_dw2;
		return (pdfW >= 0.0f && pdfW <= 1e20f) ? pdfW : 0.0f;
	};

	const Vector &fixedDir = fromLight ? localLightDir : localEyeDir;
	const Vector &sampledDir = fromLight ? localEyeDir : localLightDir;

	const float pdfFwd = evalRefractPdf(fixedDir, sampledDir);
	const float pdfRev = evalRefractPdf(sampledDir, fixedDir);

	if (directPdfW)
		*directPdfW = pdfFwd;
	if (reversePdfW)
		*reversePdfW = pdfRev;
}

float DisneyMaterial::DielectricFresnel(const float cosThetaI, const float eta1, const float eta2) const {
	const float clampedCos = Clamp(cosThetaI, 0.0f, 1.0f);
	const float safeEta1 = Max(1.0f, eta1);
	const float safeEta2 = Max(1.0f, eta2);
	const float relEta = safeEta1 / safeEta2;

	const float sin2ThetaI = Max(0.0f, 1.0f - Sqr(clampedCos));
	const float sin2ThetaT = Sqr(relEta) * sin2ThetaI;
	if (sin2ThetaT >= 1.0f)
		return 1.0f;

	const float cosThetaT = sqrtf(Max(0.0f, 1.0f - sin2ThetaT));
	const float rPerpNum = safeEta1 * clampedCos - safeEta2 * cosThetaT;
	const float rPerpDen = Max(safeEta1 * clampedCos + safeEta2 * cosThetaT, 1e-6f);
	const float rParaNum = safeEta2 * clampedCos - safeEta1 * cosThetaT;
	const float rParaDen = Max(safeEta2 * clampedCos + safeEta1 * cosThetaT, 1e-6f);

	const float rPerp = rPerpNum / rPerpDen;
	const float rPara = rParaNum / rParaDen;

	return Clamp(0.5f * (Sqr(rPerp) + Sqr(rPara)), 0.0f, 1.0f);
}

Spectrum DisneyMaterial::CalculateTint(const Spectrum &color) const {
	const float luminance = color.c[0] * 0.3f + color.c[1] * 0.6f + color.c[2] * 0.1f;

	return (luminance > 0.0f) ? color * (1.0f / luminance) : Spectrum(1.0f);
}

float DisneyMaterial::GTR1(const float NdotH, const float a) const {
	const float clampedA = Clamp(a, 0.001f, 1.0f);
	if (clampedA >= 1.0f)
		return INV_PI;

	const float a2 = clampedA * clampedA;
	const float t = 1.0f + (a2 - 1.0f) * (NdotH * NdotH);
	const float logA2 = logf(a2);
	if (fabsf(logA2) <= 1e-7f || fabsf(t) <= 1e-7f)
		return INV_PI;

	return (a2 - 1.0f) / (M_PI * logA2 * t);
}

float DisneyMaterial::GTR2_Aniso(const float NdotH, const float HdotX, const float HdotY,
		const float ax, const float ay) const {
	const float safeAx = Max(0.001f, ax);
	const float safeAy = Max(0.001f, ay);
	const float denom = Sqr(HdotX / safeAx) + Sqr(HdotY / safeAy) + NdotH * NdotH;
	return 1.0f / (M_PI * safeAx * safeAy * Sqr(Max(denom, 1e-6f)));
}

float DisneyMaterial::SmithG_GGX_Aniso(const float NdotV, const float VdotX, const float VdotY,
		const float ax, const float ay) const {
	const float absNdotV = fabsf(NdotV);
	const float denom = absNdotV + sqrtf(Max(0.0f, Sqr(VdotX * ax) + Sqr(VdotY * ay) + Sqr(absNdotV)));
	return (denom > 1e-6f) ? ((2.0f * absNdotV) / denom) : 0.0f;
}

float DisneyMaterial::SmithG_GGX(const float NdotV, const float alphaG) const {
	const float absNdotV = fabsf(NdotV);
	const float a = alphaG * alphaG;
	const float b = absNdotV * absNdotV;

	return (2.0f * absNdotV) / (absNdotV + Max(sqrtf(Max(0.0f, a + b - a * b)), 0.0001f));
}

float DisneyMaterial::Schlick_Weight(const float cosi) const {
	return powf(Clamp(1.f - cosi, 0.f, 1.f), 5.f);
}

void DisneyMaterial::Anisotropic_Params(const float anisotropic, const float roughness,
		float &ax, float &ay) const {
	const float aspect = sqrtf(Max(0.1f, 1.0f - 0.9f * Clamp(anisotropic, 0.0f, 1.0f)));
	ax = Max(0.001f, Sqr(roughness) / aspect);
	ay = Max(0.001f, Sqr(roughness) * aspect);
}

void DisneyMaterial::ComputeRatio(const float metallic, const float clearcoat, const float specTrans,
		float &ratioGlossy, float &ratioDiffuse, float &ratioClearcoat, float &ratioTrans) const {
	const float nonMetallic = 1.0f - metallic;
	const float transWeight = nonMetallic * specTrans;
	const float dielectricWeight = nonMetallic - transWeight;

	const float specularWeight = metallic + dielectricWeight * 0.5f;
	const float diffuseWeight = dielectricWeight * 0.5f;
	const float clearcoatWeight = clearcoat;

	const float sum = specularWeight + diffuseWeight + clearcoatWeight + transWeight;
	if (sum <= 1e-6f) {
		ratioGlossy = 0.0f;
		ratioDiffuse = 1.0f;
		ratioClearcoat = 0.0f;
		ratioTrans = 0.0f;
		return;
	}

	const float norm = 1.0f / sum;
	ratioGlossy = specularWeight * norm;
	ratioDiffuse = diffuseWeight * norm;
	ratioClearcoat = clearcoatWeight * norm;
	ratioTrans = transWeight * norm;
}

PropertiesUPtr DisneyMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.materials." + name + ".type")("disney"));
	if (BaseColor)
		props->Set(Property("scene.materials." + name + ".basecolor")(BaseColor->GetSDLValue()));
	if (Subsurface)
		props->Set(Property("scene.materials." + name + ".subsurface")(Subsurface->GetSDLValue()));
	if (Roughness)
		props->Set(Property("scene.materials." + name + ".roughness")(Roughness->GetSDLValue()));
	if (Metallic)
		props->Set(Property("scene.materials." + name + ".metallic")(Metallic->GetSDLValue()));
	if (Specular)
		props->Set(Property("scene.materials." + name + ".specular")(Specular->GetSDLValue()));
	if (SpecularTint)
		props->Set(Property("scene.materials." + name + ".speculartint")(SpecularTint->GetSDLValue()));
	if (Clearcoat)
		props->Set(Property("scene.materials." + name + ".clearcoat")(Clearcoat->GetSDLValue()));
	if (ClearcoatGloss)
		props->Set(Property("scene.materials." + name + ".clearcoatgloss")(ClearcoatGloss->GetSDLValue()));
	if (Anisotropic)
		props->Set(Property("scene.materials." + name + ".anisotropic")(Anisotropic->GetSDLValue()));
	if (Sheen)
		props->Set(Property("scene.materials." + name + ".sheen")(Sheen->GetSDLValue()));
	if (SheenTint)
		props->Set(Property("scene.materials." + name + ".sheentint")(SheenTint->GetSDLValue()));
	if (SpecTrans)
		props->Set(Property("scene.materials." + name + ".spectrans")(SpecTrans->GetSDLValue()));
	if (Ior)
		props->Set(Property("scene.materials." + name + ".ior")(Ior->GetSDLValue()));
	if (DiffTrans)
		props->Set(Property("scene.materials." + name + ".difftrans")(DiffTrans->GetSDLValue()));
	if (Flatness)
		props->Set(Property("scene.materials." + name + ".flatness")(Flatness->GetSDLValue()));
	if (filmAmount)
		props->Set(Property("scene.materials." + name + ".filmamount")(filmAmount->GetSDLValue()));
	if (filmThickness)
		props->Set(Property("scene.materials." + name + ".filmthickness")(filmThickness->GetSDLValue()));
	if (filmIor)
		props->Set(Property("scene.materials." + name + ".filmior")(filmIor->GetSDLValue()));
	props->Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}

void DisneyMaterial::UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	bool updateGlossiness = false;

	if (BaseColor == &oldTex) BaseColor = &newTex;
	if (Subsurface == &oldTex) Subsurface = &newTex;
	if (Roughness == &oldTex) {
		Roughness = &newTex;
		updateGlossiness = true;
	}
	if (Metallic == &oldTex) Metallic = &newTex;
	if (Specular == &oldTex) Specular = &newTex;
	if (SpecularTint == &oldTex) SpecularTint = &newTex;
	if (Clearcoat == &oldTex) Clearcoat = &newTex;
	if (ClearcoatGloss == &oldTex) ClearcoatGloss = &newTex;
	if (Anisotropic == &oldTex) Anisotropic = &newTex;
	if (Sheen == &oldTex) Sheen = &newTex;
	if (SheenTint == &oldTex) SheenTint = &newTex;
	if (SpecTrans == &oldTex) {
		SpecTrans = &newTex;
		updateGlossiness = true;
	}
	if (Ior == &oldTex) Ior = &newTex;
	if (DiffTrans == &oldTex) DiffTrans = &newTex;
	if (Flatness == &oldTex) Flatness = &newTex;
	if (filmAmount == &oldTex) filmAmount = &newTex;
	if (filmThickness == &oldTex) filmThickness = &newTex;
	if (filmIor == &oldTex) filmIor = &newTex;

	if (updateGlossiness)
		UpdateGlossiness();
}

void DisneyMaterial::AddReferencedTextures(std::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	if (BaseColor)
		BaseColor->AddReferencedTextures(referencedTexs);
	if (Subsurface)
		Subsurface->AddReferencedTextures(referencedTexs);
	if (Roughness)
		Roughness->AddReferencedTextures(referencedTexs);
	if (Metallic)
		Metallic->AddReferencedTextures(referencedTexs);
	if (Specular)
		Specular->AddReferencedTextures(referencedTexs);
	if (SpecularTint)
		SpecularTint->AddReferencedTextures(referencedTexs);
	if (Clearcoat)
		Clearcoat->AddReferencedTextures(referencedTexs);
	if (ClearcoatGloss)
		ClearcoatGloss->AddReferencedTextures(referencedTexs);
	if (Anisotropic)
		Anisotropic->AddReferencedTextures(referencedTexs);
	if (Sheen)
		Sheen->AddReferencedTextures(referencedTexs);
	if (SheenTint)
		SheenTint->AddReferencedTextures(referencedTexs);
	if (SpecTrans)
		SpecTrans->AddReferencedTextures(referencedTexs);
	if (Ior)
		Ior->AddReferencedTextures(referencedTexs);
	if (DiffTrans)
		DiffTrans->AddReferencedTextures(referencedTexs);
	if (Flatness)
		Flatness->AddReferencedTextures(referencedTexs);
	if (filmAmount)
		filmAmount->AddReferencedTextures(referencedTexs);
	if (filmThickness)
		filmThickness->AddReferencedTextures(referencedTexs);
	if (filmIor)
		filmIor->AddReferencedTextures(referencedTexs);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
