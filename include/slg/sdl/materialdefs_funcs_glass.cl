#line 2 "materialdefs_funcs_glass.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLASS)

BSDFEvent GlassMaterial_GetEventTypes() {
	return SPECULAR | REFLECT | TRANSMIT;
}

bool GlassMaterial_IsDelta() {
	return true;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 GlassMaterial_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 GlassMaterial_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 ktTexVal, const float3 krTexVal,
		const float nc, const float nt) {
	return BLACK;
}

float3 GlassMaterial_ConstSample(
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
	const float eta = entering ? (nc / nt) : ntc;
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

		const float cost = sqrt(fmax(0.f, 1.f - sint2)) * (entering ? -1.f : 1.f);
		*localSampledDir = (float3)(-eta * localFixedDir.x, -eta * localFixedDir.y, cost);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;

		//if (!hitPoint.fromLight)
			result = (1.f - FresnelCauchy_Evaluate(ntc, cost)) * eta2;
		//else
		//	result = (1.f - FresnelCauchy_Evaluate(ntc, costheta));

		result *= kt;
	} else {
		// Reflect
		*localSampledDir = (float3)(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = kr * FresnelCauchy_Evaluate(ntc, costheta);
	}

	return result / *pdfW;
}

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)
float3 GlassMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	return BLACK;
}

float3 GlassMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	const float3 kt = Texture_GetSpectrumValue(material->glass.ktTexIndex, hitPoint
		TEXTURES_PARAM);
	const float3 kr = Texture_GetSpectrumValue(material->glass.krTexIndex, hitPoint
		TEXTURES_PARAM);

	const float nc = ExtractExteriorIors(hitPoint,
			material->glass.exteriorIorTexIndex
			TEXTURES_PARAM);
	const float nt = ExtractInteriorIors(hitPoint,
			material->glass.interiorIorTexIndex
			TEXTURES_PARAM);

	return GlassMaterial_ConstSample(hitPoint, localFixedDir, localSampledDir,
			u0, u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			pdfW, absCosSampledDir, event, requestedEvent,
			kt, kr, nc, nt);
}
#endif

#endif
