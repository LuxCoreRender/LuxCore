#line 2 "materialdefs_funcs_archglass.cl"

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

#if defined(PARAM_HAS_VOLUMES)
float3 ExtractExteriorIors(__global HitPoint *hitPoint, const uint exteriorIorTexIndex
		TEXTURES_PARAM_DECL) {
	uint extIndex = NULL_INDEX;
	if (exteriorIorTexIndex != NULL_INDEX)
		extIndex = exteriorIorTexIndex;
	else {
		const uint hitPointExteriorIorTexIndex = hitPoint->exteriorIorTexIndex;
		if (hitPointExteriorIorTexIndex != NULL_INDEX)
			extIndex = hitPointExteriorIorTexIndex;
	}
	return (extIndex == NULL_INDEX) ? 1.f : Texture_GetSpectrumValue(extIndex, hitPoint
			TEXTURES_PARAM);
}

float3 ExtractInteriorIors(__global HitPoint *hitPoint, const uint interiorIorTexIndex
		TEXTURES_PARAM_DECL) {
	uint intIndex = NULL_INDEX;
	if (interiorIorTexIndex != NULL_INDEX)
		intIndex = interiorIorTexIndex;
	else {
		const uint hitPointInteriorIorTexIndex = hitPoint->interiorIorTexIndex;
		if (hitPointInteriorIorTexIndex != NULL_INDEX)
			intIndex = hitPointInteriorIorTexIndex;
	}
	return (intIndex == NULL_INDEX) ? 1.f : Texture_GetSpectrumValue(intIndex, hitPoint
			TEXTURES_PARAM);
}
#endif

//------------------------------------------------------------------------------
// ArchGlass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ARCHGLASS)

BSDFEvent ArchGlassMaterial_GetEventTypes() {
	return SPECULAR | REFLECT | TRANSMIT;
}

bool ArchGlassMaterial_IsDelta() {
	return true;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 ArchGlassMaterial_GetPassThroughTransparency(__global const Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	const float3 kt = Spectrum_Clamp(Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint
		TEXTURES_PARAM));
	const float3 kr = Spectrum_Clamp(Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint
		TEXTURES_PARAM));

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	
	const float nc = Spectrum_Filter(ExtractExteriorIors(hitPoint,
			material->archglass.exteriorIorTexIndex
			TEXTURES_PARAM));
	const float nt = Spectrum_Filter(ExtractInteriorIors(hitPoint,
			material->archglass.interiorIorTexIndex
			TEXTURES_PARAM));
	const float ntc = nt / nc;
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (passThroughEvent < threshold) {
		// Transmit

		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta = nc / nt;
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return BLACK;

		float3 result;
		//if (!hitPoint.fromLight) {
			if (entering)
				result = BLACK;
			else
				result = FresnelCauchy_Evaluate(ntc, -costheta);
		//} else {
		//	if (entering)
		//		result = FresnelCauchy_Evaluate(ntc, costheta);
		//	else
		//		result = BLACK;
		//}
		result *= 1.f + (1.f - result) * (1.f - result);
		result = 1.f - result;

		// The "2.f*" is there in place of "/threshold" (aka "/pdf")
		return 2.f * kt * result;
	} else
		return BLACK;
}
#endif

float3 ArchGlassMaterial_Evaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 ktTexVal, const float3 krTexVal,
		const float nc, const float nt) {
	return BLACK;
}

float3 ArchGlassMaterial_Sample(
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 ktTexVal, const float3 krTexVal,
		const float nc, const float nt) {
	if (!(requestedEvent & SPECULAR))
		return BLACK;

	const float3 kt = Spectrum_Clamp(ktTexVal);
	const float3 kr = Spectrum_Clamp(krTexVal);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
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
			return BLACK;
	}

	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit

		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;

		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return BLACK;

		*localSampledDir = -localFixedDir;
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;

		//if (!hitPoint.fromLight) {
			if (entering)
				result = BLACK;
			else
				result = FresnelCauchy_Evaluate(ntc, -costheta);
		//} else {
		//	if (entering)
		//		result = FresnelCauchy_Evaluate(ntc, costheta);
		//	else
		//		result = BLACK;
		//}
		result *= 1.f + (1.f - result) * (1.f - result);
		result = 1.f - result;

		result *= kt;
	} else {
		// Reflect
		if (costheta <= 0.f)
			return BLACK;

		*localSampledDir = (float3)(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	}

	return result / *pdfW;
}

#endif
