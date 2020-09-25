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

#include "slg/materials/roughmattetranslucent.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RoughMatteTranslucent material
//------------------------------------------------------------------------------

RoughMatteTranslucentMaterial::RoughMatteTranslucentMaterial(
		const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Texture *refl, const Texture *trans, const Texture *s) :
			Material(frontTransp, backTransp, emitted, bump), Kr(refl), Kt(trans), sigma(s) {
}

Spectrum RoughMatteTranslucentMaterial::Albedo(const HitPoint &hitPoint) const {
	const Spectrum r = Kr->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const Spectrum t = Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * 
		// Energy conservation
		(Spectrum(1.f) - r);

	return r + t;
}

Spectrum RoughMatteTranslucentMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const Spectrum r = Kr->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const Spectrum t = Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * 
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
			threshold = 1.f;
	} else {
		if (!isKtBlack)
			threshold = 0.f;
		else {
			if (directPdfW)
				*directPdfW = 0.f;
			if (reversePdfW)
				*reversePdfW = 0.f;
			return Spectrum();
		}
	}
	
	const bool relfected = (CosTheta(localLightDir) * CosTheta(localEyeDir) > 0.f);
	const float weight = relfected ? threshold : (1.f - threshold);

	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? CosTheta(localEyeDir) : CosTheta(localLightDir)) * (weight * INV_PI));

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? CosTheta(localLightDir) : CosTheta(localEyeDir)) * (weight * INV_PI));

	const float s = sigma->GetFloatValue(hitPoint);
	const float sigma2 = s * s;
	const float A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
	const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	const float sinthetai = SinTheta(localEyeDir);
	const float sinthetao = SinTheta(localLightDir);
	float maxcos = 0.f;
	if (sinthetai > 1e-4f && sinthetao > 1e-4f) {
		const float dcos = CosPhi(localLightDir) * CosPhi(localEyeDir) +
			SinPhi(localLightDir) * SinPhi(localEyeDir);
		maxcos = max(0.f, dcos);
	}

	const Spectrum result = (INV_PI * fabsf(localLightDir.z) *
		(A + B * maxcos * sinthetai * sinthetao / max(fabsf(CosTheta(localLightDir)), fabsf(CosTheta(localEyeDir)))));

	if (localLightDir.z * localEyeDir.z > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * result;
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * result;
	}
}

Spectrum RoughMatteTranslucentMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	if (fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*localSampledDir = CosineSampleHemisphere(u0, u1, pdfW);
	const float absCosSampledDir = fabsf(localSampledDir->z);
	if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * 
		// Energy conservation
		(Spectrum(1.f) - kr);

	const bool isKrBlack = kr.Black();
	const bool isKtBlack = kt.Black();

	// Decide to transmit or reflect
	float threshold;
	if (!isKrBlack) {
		if (!isKtBlack)
			threshold = .5f;
		else
			threshold = 1.f;
	} else {
		if (!isKtBlack)
			threshold = 0.f;
		else
			return Spectrum();
	}

	const float s = sigma->GetFloatValue(hitPoint);
	const float sigma2 = s * s;
	const float A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
	const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	const float sinthetai = SinTheta(localFixedDir);
	const float sinthetao = SinTheta(*localSampledDir);
	float maxcos = 0.f;
	if (sinthetai > 1e-4f && sinthetao > 1e-4f) {
		const float dcos = CosPhi(*localSampledDir) * CosPhi(localFixedDir) +
			SinPhi(*localSampledDir) * SinPhi(localFixedDir);
		maxcos = max(0.f, dcos);
	}
	const float coef = (A + B * maxcos * sinthetai * sinthetao / max(fabsf(CosTheta(*localSampledDir)), fabsf(CosTheta(localFixedDir))));

	if (passThroughEvent < threshold) {
		*localSampledDir *= Sgn(localFixedDir.z);
		*event = DIFFUSE | REFLECT;
		*pdfW *= threshold;
		if (hitPoint.fromLight)
			return kr * (coef * fabsf(localFixedDir.z / (absCosSampledDir * threshold)));
		else
			return kr * (coef / threshold);
	} else {
		*localSampledDir *= -Sgn(localFixedDir.z);
		*event = DIFFUSE | TRANSMIT;
		*pdfW *= (1.f - threshold);
		if (hitPoint.fromLight)
			return kt * (coef * fabsf(localFixedDir.z / (absCosSampledDir * (1.f - threshold))));
		else
			return kt * (coef / (1.f - threshold));
	}
}

void RoughMatteTranslucentMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * 
		// Energy conservation
		(Spectrum(1.f) - kr);

	const bool isKrBlack = kr.Black();
	const bool isKtBlack = kt.Black();

	// Calculate the weight of reflecting/transmitting
	float weight;
	if (!isKrBlack) {
		if (!isKtBlack)
			weight = .5f;
		else
			weight = 1.f;
	} else {
		if (!isKtBlack)
			weight = 0.f;
		else {
			if (directPdfW)
				*directPdfW = 0.f;

			if (reversePdfW)
				*reversePdfW = 0.f;
			return;
		}
	}

	const bool relfected = (Sgn(CosTheta(localLightDir)) == Sgn(CosTheta(localEyeDir)));
	weight = relfected ? weight : (1.f - weight);

	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * (weight * INV_PI));

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * (weight * INV_PI));
}

void RoughMatteTranslucentMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
	sigma->AddReferencedTextures(referencedTexs);
}

void RoughMatteTranslucentMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (Kt == oldTex)
		Kt = newTex;
	if (sigma == oldTex)
		sigma = newTex;
}

Properties RoughMatteTranslucentMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("roughmattetranslucent"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".sigma")(sigma->GetSDLValue()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
