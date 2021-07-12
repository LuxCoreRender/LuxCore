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

#include "slg/materials/disney.h"
#include "slg/materials/thinfilmcoating.h"
#include "slg/textures/fresnel/fresneltexture.h"

using namespace std;
using namespace luxrays;
using namespace slg;

DisneyMaterial::DisneyMaterial(
	const Texture *frontTransp,
	const Texture *backTransp,
	const Texture *emitted,
	const Texture *bump,
	const Texture *baseColor,
	const Texture *subsurface,
	const Texture *roughness,
	const Texture *metallic,
	const Texture *specular,
	const Texture *specularTint,
	const Texture *clearcoat,
	const Texture *clearcoatGloss,
	const Texture *anisotropic,
	const Texture *sheen,
	const Texture *sheenTint,
	const Texture *filmAmount,
	const Texture *filmThickness,
	const Texture *filmIor
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
	filmAmount(filmAmount),
	filmThickness(filmThickness),
	filmIor(filmIor) {
	UpdateGlossiness();
}

void DisneyMaterial::UpdateGlossiness() {
	// Disney glossiness requires a very special care because he material can be
	// matte, glossy or specular and everything in between

	// I first decide if it is going to look like a glossy/specular material
	const float metallicFitler = Metallic->Filter();
	const float specularFitler = Specular->Filter();
	if ((metallicFitler >= .5f) || (specularFitler >= .5f)) {
		// I use the sqrt() because the difference between Disney microfacet model
		// and Glossy2/Metal/etc. models
		const float g = ComputeGlossiness(Roughness);
		
		if (g > 0.f)
			glossiness = sqrt(ComputeGlossiness(Roughness));
		else
			glossiness = 0.f;
	} else
		glossiness = 1.f;
}

Spectrum DisneyMaterial::Albedo(const HitPoint &hitPoint) const {
	return BaseColor->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
}

Spectrum DisneyMaterial::Evaluate(
		const HitPoint &hitPoint,
		const Vector &localLightDir,
		const Vector &localEyeDir,
		BSDFEvent *event,
		float *directPdfW,
		float *reversePdfW) const  {
	const Spectrum color = BaseColor->GetSpectrumValue(hitPoint).Clamp(0.0f, 1.0f);
	const float subsurface = Clamp(Subsurface->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float roughness = Clamp(Roughness->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float metallic = Clamp(Metallic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specular = Clamp(Specular->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specularTint = Clamp(SpecularTint->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoat = Clamp(Clearcoat->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoatGloss = Clamp(ClearcoatGloss->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float anisotropicGloss = Clamp(Anisotropic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	// I allow sheen values > 1.0 to accentuate the effect. The result is still
	// clamped between 0.0 and 1.0 to not break the energy conservation law.
	const float sheen = Sheen->GetFloatValue(hitPoint);
	const float sheenTint = Clamp(SheenTint->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float localFilmAmount = filmAmount ? Clamp(filmAmount->GetFloatValue(hitPoint), 0.0f, 1.0f) : 1.f;
	const float localFilmThickness = filmThickness ? filmThickness->GetFloatValue(hitPoint) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && filmIor) ? filmIor->GetFloatValue(hitPoint) : 1.f;

	return DisneyEvaluate(color, subsurface, roughness, metallic, specular, specularTint,
			clearcoat, clearcoatGloss, anisotropicGloss, sheen, sheenTint, localFilmAmount, localFilmThickness, 
			localFilmIor, localLightDir, localEyeDir, event, directPdfW, reversePdfW);
}

Spectrum DisneyMaterial::DisneyEvaluate(
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
		const float localFilmAmount,
		const float localFilmThickness,
		const float localFilmIor,
		const Vector &localLightDir,
		const Vector &localEyeDir,
		BSDFEvent *event,
		float *directPdfW,
		float *reversePdfW) const {
	const Vector wo = Normalize(localEyeDir); 
	const Vector wi = Normalize(localLightDir);

	const float NdotL = fabsf(CosTheta(wi));
	const float NdotV = fabsf(CosTheta(wo));

	if (NdotL < DEFAULT_COS_EPSILON_STATIC || NdotV < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const Vector H = Normalize(wo + wi);

	const float NdotH = fabsf(CosTheta(H));
	const float LdotH = Dot(wi, H);
	const float VdotH = Dot(wo, H);

	const Spectrum diffuseEval = DisneyDiffuse(color, roughness, NdotL, NdotV, LdotH);

	const Spectrum subsurfaceEval = DisneySubsurface(color, roughness, NdotL, NdotV, LdotH);

	Spectrum metallicEval = DisneyMetallic(color, specular, specularTint, metallic, anisotropicGloss, roughness,
										   NdotL, NdotV, NdotH, LdotH, VdotH, wi, wo, H);
	
	if (localFilmThickness > 0.f) {
		const Spectrum metallicWithFilmColor = metallicEval * CalcFilmColor(wo, localFilmThickness, localFilmIor);
		metallicEval = Lerp(localFilmAmount, metallicEval, metallicWithFilmColor);
	}
	
	const Spectrum clearCoatEval = Spectrum(DisneyClearCoat(clearcoat, clearcoatGloss,
											NdotL, NdotV, NdotH, LdotH));
	const Spectrum glossyEval = metallicEval + clearCoatEval;

	const Spectrum sheenEval = DisneySheen(color, sheen, sheenTint, LdotH);

	if (directPdfW || reversePdfW) {
		const float pdf = DisneyPdf(roughness, metallic, clearcoat, clearcoatGloss,
				anisotropicGloss, localLightDir, localEyeDir);

		if (directPdfW)
			*directPdfW = pdf;
		if (reversePdfW)
			*reversePdfW = pdf;
	}

	*event = GLOSSY | REFLECT;
	
	const Spectrum f = (Lerp(subsurface, diffuseEval, subsurfaceEval) + sheenEval) * (1.0f - metallic) + glossyEval;

	return f * abs(NdotL);
}

Spectrum DisneyMaterial::DisneyDiffuse(const Spectrum &color, const float roughness,
		const float NdotL, const float NdotV, const float LdotH) const {
	const float FL = Schlick_Weight(NdotL);
	const float FV = Schlick_Weight(NdotV);

	const float Fd90 = 0.5f + 2.0f * (LdotH * LdotH) * (roughness * roughness);
	const float Fd = Lerp(FL, 1.0f, Fd90) * Lerp(FV, 1.0f, Fd90);

	return INV_PI * Fd * color;
}

Spectrum DisneyMaterial::DisneySubsurface(const Spectrum &color, const float roughness,
		const float NdotL, const float NdotV, const float LdotH) const {
	const float FL = Schlick_Weight(NdotL);
	const float FV = Schlick_Weight(NdotV);

	const float Fss90 = LdotH * LdotH * roughness;
	const float Fss = Lerp(FL, 1.0f, Fss90) * Lerp(FV, 1.0f, Fss90);
	const float ss = 1.25f * (Fss * (1.0f / (NdotL + NdotV) - 0.5f) + 0.5f);

	return INV_PI * ss * color;
}

Spectrum DisneyMaterial::DisneyMetallic(const Spectrum &color,
		const float specular, const float specularTint, const float metallic,
		const float anisotropic, const float roughness,
		const float NdotL, const float NdotV, const float NdotH,
		const float LdotH, const float VdotH,
		const Vector &wi, const Vector &wo, const Vector &H) const {
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

	return Gs * Fs * Ds;
}

float DisneyMaterial::DisneyClearCoat(const float clearcoat, const float clearcoatGloss,
		float NdotL, float NdotV, float NdotH, float LdotH) const {
	const float gloss = Lerp(clearcoatGloss, 0.1f, 0.001f);

	const float Dr = GTR1(abs(NdotH), gloss);
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

	return (FH * sheen * Csheen).Clamp(0.f, 1.f);
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
	const Spectrum color = BaseColor->GetSpectrumValue(hitPoint).Clamp(0.0f, 1.0f);
	const float subsurface = Clamp(Subsurface->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float roughness = Clamp(Roughness->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float metallic = Clamp(Metallic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specular = Clamp(Specular->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float specularTint = Clamp(SpecularTint->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoat = Clamp(Clearcoat->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float clearcoatGloss = Clamp(ClearcoatGloss->GetFloatValue(hitPoint), 0.0f, 1.0f);
	const float anisotropicGloss = Clamp(Anisotropic->GetFloatValue(hitPoint), 0.0f, 1.0f);
	// I allow sheen values > 1.0 to accentuate the effect. The result is still
	// clamped between 0.0 and 1.0 to not break the energy conservation law.
	const float sheen = Sheen->GetFloatValue(hitPoint);
	const float sheenTint = Clamp(SheenTint->GetFloatValue(hitPoint), 0.0f, 1.0f);

	const Vector wo = Normalize(localFixedDir);

	float ratioGlossy, ratioDiffuse, ratioClearcoat;
	ComputeRatio(metallic, clearcoat, ratioGlossy, ratioDiffuse, ratioClearcoat);

	if (passThroughEvent <= ratioGlossy)
		*localSampledDir = DisneyMetallicSample(anisotropicGloss, roughness, wo, u0, u1);
	else if (passThroughEvent > ratioGlossy &&  passThroughEvent <= ratioGlossy + ratioClearcoat)
		*localSampledDir = DisneyClearcoatSample(clearcoatGloss, wo, u0, u1);
	else if (passThroughEvent > ratioGlossy + ratioClearcoat && passThroughEvent <= ratioGlossy + ratioClearcoat + ratioDiffuse)
		*localSampledDir = DisneyDiffuseSample(wo, u0, u1);
	else
		return Spectrum();
	
	*event = GLOSSY | REFLECT;
	
	const Vector &localLightDir = hitPoint.fromLight ? localFixedDir : *localSampledDir;
	const Vector &localEyeDir = hitPoint.fromLight ? *localSampledDir : localFixedDir;

	*pdfW = DisneyPdf(roughness, metallic, clearcoat, clearcoatGloss, anisotropicGloss,
			localLightDir, localEyeDir);

	if (*pdfW < 0.0001f)
		return Spectrum();
		
	const float localFilmAmount = filmAmount ? Clamp(filmAmount->GetFloatValue(hitPoint), 0.0f, 1.0f) : 1.f;
	const float localFilmThickness = filmThickness ? filmThickness->GetFloatValue(hitPoint) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && filmIor) ? filmIor->GetFloatValue(hitPoint) : 1.f;

	const Spectrum f = DisneyEvaluate(color, subsurface, roughness,
			metallic, specular, specularTint, clearcoat, clearcoatGloss,
			anisotropicGloss, sheen, sheenTint, localFilmAmount, localFilmThickness, localFilmIor,
			localLightDir, localEyeDir, event, nullptr, nullptr);

	return f / *pdfW;
}

Vector DisneyMaterial::DisneyDiffuseSample(const Vector &wo, float u0, float u1) const {
	return Sgn(CosTheta(wo)) * CosineSampleHemisphere(u0, u1);
}

Vector DisneyMaterial::DisneyMetallicSample(const float anisotropic, const float roughness,
		const Vector &wo, float u0, float u1) const {
	float ax, ay;
	Anisotropic_Params(anisotropic, roughness, ax, ay);

	float phi = atan(ay / ax * tan(2.0f * M_PI * u1 + 0.5f * M_PI));
	if (u1 > 0.5f)
		phi += M_PI;

	const float sinPhi = sin(phi), cosPhi = cos(phi);
	const float ax2 = ax * ax, ay2 = ay * ay;
	const float alpha2 = 1.0f / (cosPhi * cosPhi / ax2 + sinPhi * sinPhi / ay2);
	const float tanTheta2 = alpha2 * u0 / (1.0f - u0);
	const float cosTheta = 1.0f / sqrt(1.0f + tanTheta2);

	const float sinTheta = sqrt(Max(0.0f, 1.0f - cosTheta * cosTheta));
	Vector wh = Vector(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);

	if (CosTheta(wo) * CosTheta(wh) <= 0.0f)
		wh *= -1.f;

	return Normalize(2.0f * Dot(wh, wo) * wh - wo);
}

Vector DisneyMaterial::DisneyClearcoatSample(const float clearcoatGloss,
		const Vector &wo, float u0, float u1) const {
	const float gloss = Lerp(clearcoatGloss, 0.1f, 0.001f);
	const float alpha2 = gloss * gloss;
	const float cosTheta = sqrt(Max(0.0001f, (1.0f - pow(alpha2, 1.0f - u0)) / (1.0f - alpha2)));
	const float sinTheta = sqrt(Max(0.0001f, 1.0f - cosTheta * cosTheta));
	const float phi = 2.0f * M_PI * u1;

	Vector wh = Vector(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	if (CosTheta(wo) * CosTheta(wh) <= 0.0f)
		wh *= -1.f;

	return Normalize(2.0f * Dot(wh, wo) * wh - wo);
}

void DisneyMaterial::Pdf(
		const HitPoint &hitPoint,
		const Vector &localLightDir, 
		const Vector &localEyeDir,
		float *directPdfW, 
		float *reversePdfW) const {
	if (directPdfW || reversePdfW) {
		const float roughness = Clamp(Roughness->GetFloatValue(hitPoint), 0.0f, 1.0f);
		const float metallic = Clamp(Metallic->GetFloatValue(hitPoint), 0.0f, 1.0f);
		const float clearcoat = Clamp(SpecularTint->GetFloatValue(hitPoint), 0.0f, 1.0f);
		const float clearcoatGloss = Clamp(ClearcoatGloss->GetFloatValue(hitPoint), 0.0f, 1.0f);
		const float anisotropicGloss = Clamp(Anisotropic->GetFloatValue(hitPoint), 0.0f, 1.0f);

		const float pdf = DisneyPdf(roughness, metallic, clearcoat, clearcoatGloss,
				anisotropicGloss, localLightDir, localEyeDir);

		if (directPdfW)
			*directPdfW = pdf;
		if (reversePdfW)
			*reversePdfW = pdf;
	}
}

float DisneyMaterial::DisneyPdf(const float roughness, const float metallic,
		const float clearcoat, const float clearcoatGloss, const float anisotropic,
		const Vector &localLightDir, const Vector &localEyeDir) const {
	if (CosTheta(localLightDir) * CosTheta(localEyeDir) <= 0.0f)
		return 0.0f;

	const Vector wi = Normalize(localLightDir);
	const Vector wo = Normalize(localEyeDir);

	float ratioGlossy, ratioDiffuse, ratioClearcoat;
	ComputeRatio(metallic, clearcoat, ratioGlossy, ratioDiffuse, ratioClearcoat);

	const float pdfDiffuse = ratioDiffuse * DiffusePdf(wi, wo);
	const float pdfMicrofacet = ratioGlossy * MetallicPdf(anisotropic, roughness, wi, wo);
	const float pdfClearcoat = ratioClearcoat * ClearcoatPdf(clearcoatGloss, wi, wo);

	return pdfDiffuse + pdfMicrofacet + pdfClearcoat;	
}

float DisneyMaterial::DiffusePdf(const Vector &wi, const Vector &wo) const {
	return fabsf(CosTheta(wi)) * INV_PI;
}

float DisneyMaterial::MetallicPdf(const float anisotropic, const float roughness,
		const Vector &wi, const Vector &wo) const {
	const Vector wh = Normalize(wo + wi);

	float ax, ay;
	Anisotropic_Params(anisotropic, roughness, ax, ay);

	const float ax2 = ax * ax;
	const float ay2 = ay * ay;

	const float HdotX = wh.x;
	const float HdotY = wh.y;
	const float NdotH = fabsf(CosTheta(wh));

	const float denom = HdotX * HdotX / ax2 + HdotY * HdotY / ay2 + NdotH * NdotH;
	if (denom == 0.0f)
		return 0.0f;

	const float pdfDistribution = NdotH / (M_PI * ax * ay * denom * denom);

	return pdfDistribution / (4.0f * Dot(wo, wh));
}

float DisneyMaterial::ClearcoatPdf(const float clearcoatGloss, const Vector &wi,
		const Vector &wo) const {
	const Vector wh = Normalize(wi + wo);

	const float NdotH = fabsf(CosTheta(wh));
	const float Dr = GTR1(NdotH, Lerp(clearcoatGloss, 0.1f, 0.001f));

	return Dr * NdotH / (4.0f * Dot(wo, wh));
}

Spectrum DisneyMaterial::CalculateTint(const Spectrum &color) const {
	const float luminance = color.c[0] * 0.3f + color.c[1] * 0.6f + color.c[2] * 1.0f;

	return (luminance > 0.0f) ? color * (1.0f / luminance) : Spectrum(1.0f);
}

float DisneyMaterial::GTR1(const float NdotH, const float a) const  {
	if (a >= 1.0f)
		return INV_PI;

	const float a2 = a * a;
	const float t = 1.0f + (a2 - 1.0f) * (NdotH * NdotH);

	return (a2 - 1.0f) / (M_PI * log(a2) * t);
}

float DisneyMaterial::GTR2_Aniso(const float NdotH, const float HdotX, const float HdotY,
		const float ax, const float ay) const {
	return 1.0f / (M_PI * ax * ay * Sqr(Sqr(HdotX / ax) + Sqr(HdotY / ay) + NdotH * NdotH));
}

float DisneyMaterial::SmithG_GGX_Aniso(const float NdotV, const float VdotX, const float VdotY,
		const float ax, const float ay) const {
	return 1.0f / (NdotV + sqrt(Sqr(VdotX * ax) + Sqr(VdotY * ay) + Sqr(NdotV)));
}

float DisneyMaterial::SmithG_GGX(const float NdotV, const float alphaG) const {
	const float a = alphaG * alphaG;
	const float b = NdotV * NdotV;

	return 1.0f / (abs(NdotV) + Max(sqrt(a + b - a * b), 0.0001f));
}

float DisneyMaterial::Schlick_Weight(const float cosi) const {
	return powf(1.f - cosi, 5.f);
}

void DisneyMaterial::Anisotropic_Params(const float anisotropic, const float roughness,
		float &ax, float &ay) const {
	const float aspect = sqrtf(1.0f - 0.9f * anisotropic);
	ax = Max(0.001f, Sqr(roughness) / aspect);
	ay = Max(0.001f, Sqr(roughness) * aspect);
}

void DisneyMaterial::ComputeRatio(const float metallic, const float clearcoat,
		float &ratioGlossy, float &ratioDiffuse, float &ratioClearcoat) const {
	const float metallicBRDF = metallic;
	const float dielectricBRDF = (1.0f - metallic);

	const float specularWeight = metallicBRDF + dielectricBRDF;
	const float diffuseWeight = dielectricBRDF;
	const float clearcoatWeight = clearcoat;

	const float norm = 1.0f / (specularWeight + diffuseWeight + clearcoatWeight);

	ratioGlossy = specularWeight * norm;
	ratioDiffuse = diffuseWeight * norm;
	ratioClearcoat = clearcoatWeight * norm;
}

Properties DisneyMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("disney"));
	props.Set(Property("scene.materials." + name + ".basecolor")(BaseColor->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".subsurface")(Subsurface->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".roughness")(Roughness->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".metallic")(Metallic->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".specular")(Specular->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".speculartint")(SpecularTint->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".clearcoat")(Clearcoat->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".clearcoatgloss")(ClearcoatGloss->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".anisotropic")(Anisotropic->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".sheen")(Sheen->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".sheentint")(SheenTint->GetSDLValue()));
	if (filmAmount)
		props.Set(Property("scene.materials." + name + ".filmamount")(filmAmount->GetSDLValue()));
	if (filmThickness)
		props.Set(Property("scene.materials." + name + ".filmthickness")(filmThickness->GetSDLValue()));
	if (filmIor)
		props.Set(Property("scene.materials." + name + ".filmior")(filmIor->GetSDLValue()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}

void DisneyMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	bool updateGlossiness = false;
	if (BaseColor == oldTex) BaseColor = newTex;
	if (Subsurface == oldTex) Subsurface = newTex;
	if (Roughness == oldTex) {
		Roughness = newTex;
		updateGlossiness = true;
	}
	if (Metallic == oldTex) Metallic = newTex;
	if (Specular == oldTex) Specular = newTex;
	if (SpecularTint == oldTex) SpecularTint = newTex;
	if (Clearcoat == oldTex) Clearcoat = newTex;
	if (ClearcoatGloss == oldTex) ClearcoatGloss = newTex;
	if (Anisotropic == oldTex) Anisotropic = newTex;
	if (Sheen == oldTex) Sheen = newTex;
	if (SheenTint == oldTex) SheenTint = newTex;
	if (filmAmount == oldTex) filmAmount = newTex;
	if (filmThickness == oldTex) filmThickness = newTex;
	if (filmIor == oldTex) filmIor = newTex;

	if (updateGlossiness)
		UpdateGlossiness();
}

void DisneyMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	BaseColor->AddReferencedTextures(referencedTexs);
	Subsurface->AddReferencedTextures(referencedTexs);
	Roughness->AddReferencedTextures(referencedTexs);
	Metallic->AddReferencedTextures(referencedTexs);
	Specular->AddReferencedTextures(referencedTexs);
	SpecularTint->AddReferencedTextures(referencedTexs);
	Clearcoat->AddReferencedTextures(referencedTexs);
	ClearcoatGloss->AddReferencedTextures(referencedTexs);
	Anisotropic->AddReferencedTextures(referencedTexs);
	Sheen->AddReferencedTextures(referencedTexs);
	SheenTint->AddReferencedTextures(referencedTexs);
	if (filmAmount)
		filmAmount->AddReferencedTextures(referencedTexs);
	if (filmThickness)
		filmThickness->AddReferencedTextures(referencedTexs);
	if (filmIor)
		filmIor->AddReferencedTextures(referencedTexs);
}
