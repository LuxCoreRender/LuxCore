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

#include "slg/materials/glass.h"

using namespace std;
using namespace luxrays;
using namespace slg;

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

	const float nc = ExtractExteriorIors(hitPoint, exteriorIor);
	const float nt = ExtractInteriorIors(hitPoint, interiorIor);
	const float ntc = nt / nc;
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
		const float eta = entering ? (nc / nt) : ntc;
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
			result = (Spectrum(1.f) - FresnelTexture::CauchyEvaluate(ntc, cost)) * eta2;
		else
			result = (Spectrum(1.f) - FresnelTexture::CauchyEvaluate(ntc, costheta)) * fabsf(localFixedDir.z / *absCosSampledDir);

		result *= kt;
	} else {
		// Reflect
		*localSampledDir = Vector(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabsf(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelTexture::CauchyEvaluate(ntc, costheta);
	}

	return result / *pdfW;
}

void GlassMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
	if (exteriorIor)
		exteriorIor->AddReferencedTextures(referencedTexs);
	if (interiorIor)
		interiorIor->AddReferencedTextures(referencedTexs);
}

void GlassMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (Kt == oldTex)
		Kt = newTex;
	if (exteriorIor == oldTex)
		exteriorIor = newTex;
	if (interiorIor == oldTex)
		interiorIor = newTex;
}

Properties GlassMaterial::ToProperties(const ImageMapCache &imgMapCache) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("glass"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetName()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetName()));
	if (exteriorIor)
		props.Set(Property("scene.materials." + name + ".exteriorior")(exteriorIor->GetName()));
	if (interiorIor)
		props.Set(Property("scene.materials." + name + ".interiorior")(interiorIor->GetName()));
	props.Set(Material::ToProperties(imgMapCache));

	return props;
}
