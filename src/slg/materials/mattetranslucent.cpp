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

#include "slg/materials/mattetranslucent.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

MatteTranslucentMaterial::MatteTranslucentMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Texture *refl, const Texture *trans) :
			Material(frontTransp, backTransp, emitted, bump),
			Kr(refl), Kt(trans) {
}

Spectrum MatteTranslucentMaterial::Albedo(const HitPoint &hitPoint) const {
	const Spectrum r = Kr->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const Spectrum t = Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * 
		// Energy conservation
		(Spectrum(1.f) - r);

	return r + t;
}

Spectrum MatteTranslucentMaterial::Evaluate(const HitPoint &hitPoint,
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

	if (localLightDir.z * localEyeDir.z > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * INV_PI * fabsf(CosTheta(localLightDir));
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * INV_PI * fabsf(CosTheta(localLightDir));
	}
}

Spectrum MatteTranslucentMaterial::Sample(const HitPoint &hitPoint,
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

	if (passThroughEvent < threshold) {
		*localSampledDir *= Sgn(localFixedDir.z);
		*event = DIFFUSE | REFLECT;
		*pdfW *= threshold;
		if (hitPoint.fromLight)
			return kr * fabsf(localFixedDir.z / (absCosSampledDir * threshold));
		else
			return kr / threshold;
	} else {
		*localSampledDir *= -Sgn(localFixedDir.z);
		*event = DIFFUSE | TRANSMIT;
		*pdfW *= (1.f - threshold);
		if (hitPoint.fromLight)
			return kt * fabsf(localFixedDir.z / (absCosSampledDir * (1.f - threshold)));
		else
			return kt / (1.f - threshold);
	}
}

void MatteTranslucentMaterial::Pdf(const HitPoint &hitPoint,
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

Properties MatteTranslucentMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("mattetranslucent"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetSDLValue()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
