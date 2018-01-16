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

#include "slg/materials/metal2.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Metal2 material
//
// LuxRender Metal2 material porting.
//------------------------------------------------------------------------------
			
Spectrum Metal2Material::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const float u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	const Vector wh(Normalize(localLightDir + localEyeDir));
	const float cosWH = Dot(localLightDir, wh);

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * cosWH);

	if (reversePdfW)
		*reversePdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * cosWH);

	Spectrum F;
	if (fresnelTex)
		F = fresnelTex->Evaluate(hitPoint, cosWH);
	else {
		// For compatibility with the past
		const Spectrum etaVal = n->GetSpectrumValue(hitPoint).Clamp(.001f);
		const Spectrum kVal = k->GetSpectrumValue(hitPoint).Clamp(.001f);
		F = FresnelTexture::GeneralEvaluate(etaVal, kVal, cosWH);
	}

	const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);

	*event = GLOSSY | REFLECT;
	return (SchlickDistribution_D(roughness, wh, anisotropy) * G / (4.f * fabsf(localEyeDir.z))) * F;
}

Spectrum Metal2Material::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event) const {
	if (fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const float u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
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

	Spectrum F;
	if (fresnelTex)
		F = fresnelTex->Evaluate(hitPoint, cosWH);
	else {
		// For compatibility with the past
		const Spectrum etaVal = n->GetSpectrumValue(hitPoint).Clamp(.001f);
		const Spectrum kVal = k->GetSpectrumValue(hitPoint).Clamp(.001f);
		F = FresnelTexture::GeneralEvaluate(etaVal, kVal, cosWH);
	}

	float factor = (d / specPdf) * G * fabsf(cosWH);
	if (!hitPoint.fromLight)
		factor /= coso;
	else
		factor /= cosi;

	*event = GLOSSY | REFLECT;

	return factor * F;
}

void Metal2Material::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const float u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	const Vector wh(Normalize(localLightDir + localEyeDir));

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localLightDir, wh));

	if (reversePdfW)
		*reversePdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localLightDir, wh));
}

void Metal2Material::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	if (fresnelTex)
		fresnelTex->AddReferencedTextures(referencedTexs);
	if (n)
		n->AddReferencedTextures(referencedTexs);
	if (k)
		k->AddReferencedTextures(referencedTexs);

	nu->AddReferencedTextures(referencedTexs);
	nv->AddReferencedTextures(referencedTexs);
}

void Metal2Material::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (fresnelTex == oldTex)
		fresnelTex = (FresnelTexture *)newTex;
	if (n == oldTex)
		n = newTex;
	if (k == oldTex)
		k = newTex;
	if (nu == oldTex)
		nu = newTex;
	if (nv == oldTex)
		nv = newTex;
}

Properties Metal2Material::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("metal2"));
	if (fresnelTex)
		props.Set(Property("scene.materials." + name + ".fresnel")(fresnelTex->GetName()));
	if (n)
		props.Set(Property("scene.materials." + name + ".n")(n->GetName()));
	if (k)
		props.Set(Property("scene.materials." + name + ".k")(k->GetName()));
	props.Set(Property("scene.materials." + name + ".uroughness")(nu->GetName()));
	props.Set(Property("scene.materials." + name + ".vroughness")(nv->GetName()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
