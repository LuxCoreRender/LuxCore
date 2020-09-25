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

#include "slg/materials/velvet.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Velvet material
//------------------------------------------------------------------------------

VelvetMaterial::VelvetMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Texture *kd, const Texture *p1, const Texture *p2, const Texture *p3,
		const Texture *thickness) :
			Material(frontTransp, backTransp, emitted, bump), Kd(kd),
			P1(p1), P2(p2), P3(p3), Thickness(thickness) {
	glossiness = 1.f;
}

Spectrum VelvetMaterial::Albedo(const HitPoint &hitPoint) const {
	return Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
}

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

	return Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * p;
}

Spectrum VelvetMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	if (fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);
	if (fabsf(CosTheta(*localSampledDir)) < DEFAULT_COS_EPSILON_STATIC)
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
	
	return Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * (p / *pdfW);
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

Properties VelvetMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const std::string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("velvet"));
	props.Set(Property("scene.materials." + name + ".kd")(Kd->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".p1")(P1->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".p2")(P2->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".p3")(P3->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".thickness")(Thickness->GetSDLValue()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
