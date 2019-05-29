/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
	const float subsurface,
	const float roughness,
	const float metallic,
	const float specular,
	const float specularTint,
	const float clearcoat,
	const float clearcoatGloss,
	const float anisotropic,
	const float sheen,
	const float sheenTint
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
	SheenTint(sheenTint)
{
	float metallicBRDF = Metallic;
	float dielectricBRDF = (1.0f - Metallic);

	float specularWeight = metallicBRDF + dielectricBRDF;
	float diffuseWeight = dielectricBRDF;
	float clearcoatWeight = Clearcoat;

	float norm = 1.0f / (specularWeight + diffuseWeight + clearcoatWeight);

	RatioGlossy = specularWeight * norm;
	RatioDiffuse = diffuseWeight * norm;
	RatioClearcoat = clearcoatWeight * norm;
}

Spectrum DisneyMaterial::Albedo(const HitPoint &hitPoint) const 
{
	return BaseColor->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
}

Spectrum DisneyMaterial::Evaluate(
	const HitPoint &hitPoint,
	const Vector &localLightDir,
	const Vector &localEyeDir,
	BSDFEvent *event,
	float *directPdfW,
	float *reversePdfW
) const 
{
	Vector wo = Normalize(localEyeDir); 
	Vector wi = Normalize(localLightDir);

	Spectrum color = BaseColor->GetSpectrumValue(hitPoint).Clamp(0.0f, 1.0f);

	float NdotL = CosTheta(wi);
	float NdotV = CosTheta(wo);

	if (NdotL <= 0.0f || NdotV <= 0.0f) return Spectrum();

	Vector H = Normalize(wo + wi);

	float NdotH = CosTheta(H);
	float LdotH = Dot(wi, H);
	float VdotH = Dot(wo, H);

	Spectrum diffuse = DisneyDiffuse(color, NdotL, NdotV, LdotH);
	Spectrum subsurface = DisneySubsurface(color, NdotL, NdotV, LdotH);
	Spectrum glossy = DisneyMetallic(color, NdotL, NdotV, NdotH, LdotH, VdotH, wi, wo, H);
	float clearcoat = DisneyClearCoat(color, NdotL, NdotV, NdotH, LdotH);
	Spectrum sheen = DisneySheen(color, LdotH);

	if (directPdfW) *directPdfW = DisneyPdf(localLightDir, localEyeDir);
	if (directPdfW) *directPdfW = DisneyPdf(localEyeDir, localLightDir);

	glossy += clearcoat;

	Spectrum f = (Lerp(Subsurface, diffuse, subsurface) + sheen) * (1.0f - Metallic) + glossy;

	return f * abs(NdotL);
}

Spectrum DisneyMaterial::DisneyDiffuse(Spectrum &color, float NdotL, float NdotV, float LdotH) const
{
	float FL = Schlick_Weight(NdotL);
	float FV = Schlick_Weight(NdotV);

	float Fd90 = 0.5f + 2.0f * (LdotH * LdotH) * (Roughness * Roughness); //0.5f + 2.0f * (LdotH * LdotH) * Roughness
	float Fd = Lerp(FL, 1.0f, Fd90) * Lerp(FV, 1.0f, Fd90);

	return INV_PI * Fd * color;
}

Spectrum DisneyMaterial::DisneySubsurface(Spectrum &color, float NdotL, float NdotV, float LdotH) const
{

	float FL = Schlick_Weight(NdotL);
	float FV = Schlick_Weight(NdotV);

	float Fss90 = LdotH * LdotH * Roughness;
	float Fss = Lerp(FL, 1.0f, Fss90) * Lerp(FV, 1.0f, Fss90);
	float ss = 1.25f * (Fss * (1.0f / (NdotL + NdotV) - 0.5f) + 0.5f);

	return INV_PI * ss * color;
}

Spectrum DisneyMaterial::DisneyMetallic(Spectrum &color, float NdotL, float NdotV, float NdotH, float LdotH, float VdotH, Vector wi, Vector wo, Vector H) const
{
	Spectrum Ctint = CalculateTint(color);

	Spectrum CSpecTint = Specular * 0.08f * Lerp(SpecularTint, Spectrum(1.0f), Ctint);
	Spectrum Cspec0 = Lerp(Metallic, CSpecTint, color);

	float ax, ay;
	Anisotropic_Params(ax, ay);

	float Ds = GTR2_Aniso(NdotH, H.x, H.y, ax, ay);

	float FH = Schlick_Weight(LdotH);

	Spectrum Fs = Lerp(FH, Cspec0, Spectrum(1.0f));

	float Gl = SmithG_GGX_Aniso(NdotL, wi.x, wi.y, ax, ay);
	float Gv = SmithG_GGX_Aniso(NdotV, wo.x, wo.y, ax, ay);

	float Gs = Gl * Gv;

	return Gs * Fs * Ds;
}

float DisneyMaterial::DisneyClearCoat(Spectrum &color, float NdotL, float NdotV, float NdotH, float LdotH) const
{
	float gloss = Lerp(ClearcoatGloss, 0.1f, 0.001f);

	float Dr = GTR1(abs(NdotH), gloss);
	float FH = Schlick_Weight(LdotH);
	float Fr = Lerp(FH, 0.04f, 1.0f);
	float Gr = SmithG_GGX(NdotL, 0.25f) * SmithG_GGX(NdotV, 0.25f);

	return Clearcoat * Fr * Gr * Dr;
}

Spectrum DisneyMaterial::DisneySheen(Spectrum &color, float LdotH) const
{
	float FH = Schlick_Weight(LdotH);

	Spectrum Ctint = CalculateTint(color);
	Spectrum Csheen = Lerp(SheenTint, Spectrum(1.0f), Ctint);

	return FH * Sheen * Csheen;
}

Spectrum DisneyMaterial::Sample(
	const HitPoint &hitPoint,
	const Vector &localFixedDir,
	Vector *localSampledDir,
	const float u0,
	const float u1,
	const float passThroughEvent,
	float *pdfW,
	float *absCosSampledDir,
	BSDFEvent *event
) const
{
	Vector wo = Normalize(localFixedDir);

	if (passThroughEvent <= RatioGlossy) 
	{
		*localSampledDir = DisneyMetallicSample(wo, u0, u1);
		*event = GLOSSY | REFLECT;
	}
	else if (passThroughEvent > RatioGlossy &&  passThroughEvent <= RatioGlossy + RatioClearcoat)
	{
		*localSampledDir = DisneyClearcoatSample(wo, u0, u1);
		*event = GLOSSY | REFLECT;
	}
	else if (passThroughEvent > RatioGlossy + RatioClearcoat && passThroughEvent <= RatioGlossy + RatioClearcoat + RatioDiffuse)
	{
		*localSampledDir = DisneyDiffuseSample(wo, u0, u1);
		*event = DIFFUSE | REFLECT;
	}
	else
	{
		return Spectrum();
	}
	
	const Vector &localLightDir = hitPoint.fromLight ? localFixedDir : *localSampledDir;
	const Vector &localEyeDir = hitPoint.fromLight ? *localSampledDir : localFixedDir;

	Spectrum f = Evaluate(hitPoint, localLightDir, localEyeDir, event);

	*pdfW = DisneyPdf(localLightDir, localEyeDir);

	if (*pdfW < 0.0001f) return Spectrum();

	*absCosSampledDir = abs(CosTheta(*localSampledDir));
	
	return f / *pdfW;
}

Vector DisneyMaterial::DisneyDiffuseSample(Vector &wo, float u0, float u1) const
{
	return Sgn(CosTheta(wo)) * CosineSampleHemisphere(u0, u1);
}

Vector DisneyMaterial::DisneyMetallicSample(Vector &wo, float u0, float u1) const
{
	float ax, ay;
	Anisotropic_Params(ax, ay);

	float phi = atan(ay / ax * tan(2.0f * M_PI * u1 + 0.5f * M_PI));

	if (u1 > 0.5f) phi += M_PI;

	float sinPhi = sin(phi), cosPhi = cos(phi);
	float ax2 = ax * ax, ay2 = ay * ay;
	float alpha2 = 1.0f / (cosPhi * cosPhi / ax2 + sinPhi * sinPhi / ay2);
	float tanTheta2 = alpha2 * u0 / (1.0f - u0);
	float cosTheta = 1.0f / sqrt(1.0f + tanTheta2);

	float sinTheta = sqrt(Max(0.0f, 1.0f - cosTheta * cosTheta));
	Vector wh = Vector(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);

	if (CosTheta(wo) * CosTheta(wh) <= 0.0f)
	{
		wh *= -1.;
	}

	return Normalize(2.0f * Dot(wh, wo) * wh - wo);
}

Vector DisneyMaterial::DisneyClearcoatSample(Vector &wo, float u0, float u1) const
{
	float gloss = Lerp(ClearcoatGloss, 0.1f, 0.001f);
	float alpha2 = gloss * gloss;
	float cosTheta = sqrt(Max(0.0001f, (1.0f - pow(alpha2, 1.0f - u0)) / (1.0f - alpha2)));
	float sinTheta = sqrt(Max(0.0001f, 1.0f - cosTheta * cosTheta));
	float phi = 2.0f * M_PI * u1;

	Vector wh = Vector(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	if (CosTheta(wo) * CosTheta(wh) <= 0.0f)
	{
		wh = -wh;
	}

	return Normalize(2.0f * Dot(wh, wo) * wh - wo);
}

void DisneyMaterial::Pdf(
	const HitPoint &hitPoint,
	const Vector &localLightDir, 
	const Vector &localEyeDir,
	float *directPdfW, 
	float *reversePdfW
) const 
{
	if (directPdfW) *directPdfW = DisneyPdf(localLightDir, localEyeDir);
	if (directPdfW) *directPdfW = DisneyPdf(localEyeDir, localLightDir);
}

float DisneyMaterial::DisneyPdf(const Vector &localLightDir, const Vector localEyeDir) const
{
	Vector wi = Normalize(localLightDir);
	Vector wo = Normalize(localEyeDir);

	float pdfDiffuse = RatioDiffuse * LambertianReflectionPdf(wi, wo);
	float pdfMicrofacet = RatioGlossy * MetallicPdf(wi, wo);
	float pdfClearcoat = RatioClearcoat * ClearcoatPdf(wi, wo);

	return pdfDiffuse + pdfMicrofacet + pdfClearcoat;
}

float DisneyMaterial::LambertianReflectionPdf(Vector &wi, Vector &wo) const
{
	return abs(CosTheta(wi)) * INV_PI;
}

float DisneyMaterial::MetallicPdf(Vector &wi, Vector &wo) const
{
	if (CosTheta(wo) * CosTheta(wi) <= 0.0f) return 0.0f;

	Vector wh = Normalize(wo + wi);

	float ax, ay;
	Anisotropic_Params(ax, ay);

	float ax2 = ax * ax;
	float ay2 = ay * ay;

	float HdotX = wh.x;
	float HdotY = wh.y;
	float NdotH = CosTheta(wh);

	float denom = HdotX * HdotX / ax2 + HdotY * HdotY / ay2 + NdotH * NdotH;
	
	if (denom == 0.0f) return 0.0f;

	float pdfDistribution = NdotH / (M_PI * ax * ay * denom * denom);

	return pdfDistribution / (4.0f * Dot(wo, wh));
}

float DisneyMaterial::ClearcoatPdf(Vector &wi, Vector &wo) const
{
	if (CosTheta(wo) * CosTheta(wi) <= 0.0f)
	{
		return 0.0f;
	}

	Vector wh = Normalize(wi + wo);

	float NdotH = abs(CosTheta(wh));
	float Dr = GTR1(NdotH, Lerp(ClearcoatGloss, 0.1f, 0.001f));

	return Dr * NdotH / (4.0f * Dot(wo, wh));
}

Spectrum DisneyMaterial::CalculateTint(Spectrum &color) const
{
	float luminance = color.c[0] * 0.3f + color.c[1] * 0.6f + color.c[2] * 1.0f;
	return (luminance > 0.0f) ? color * (1.0f / luminance) : Spectrum(1.0f);
}

float DisneyMaterial::GTR1(float NdotH, float a) const 
{
	if (a >= 1.0f) return INV_PI;

	float a2 = a * a;
	float t = 1.0f + (a2 - 1.0f) * (NdotH * NdotH);

	return (a2 - 1.0f) / (M_PI * log(a2) * t);
}

float DisneyMaterial::GTR2_Aniso(float NdotH, float HdotX, float HdotY, float ax, float ay) const
{
	return 1.0f / (M_PI * ax * ay * Sqr(Sqr(HdotX / ax) + Sqr(HdotY / ay) + NdotH * NdotH));
}

float DisneyMaterial::SmithG_GGX_Aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) const
{
	return 1.0f / (NdotV + sqrt(Sqr(VdotX * ax) + Sqr(VdotY * ay) + Sqr(NdotV)));
}

float DisneyMaterial::SmithG_GGX(float NdotV, float alphaG) const 
{
	float a = alphaG * alphaG;
	float b = NdotV * NdotV;

	return 1.0f / (abs(NdotV) + max(sqrt(a + b - a * b), 0.0001f));
}

float DisneyMaterial::Schlick_Weight(float cosi) const
{
	return powf(1.f - cosi, 5.f);
}

void DisneyMaterial::Anisotropic_Params(float &ax, float &ay) const
{
	float aspect = sqrtf(1.0f - 0.9f * Anisotropic);
	ax = Max(0.001f, Sqr(Roughness) / aspect);
	ay = Max(0.001f, Sqr(Roughness) * aspect);
}
