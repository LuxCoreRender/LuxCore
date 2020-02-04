#line 2 "materialdefs_funcs_roughmatte_translucent.cl"

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

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 RoughMatteTranslucentMaterial_Albedo(const float3 krVal, const float3 ktVal) {
	const float3 r = Spectrum_Clamp(krVal);
	const float3 t = Spectrum_Clamp(ktVal) * 
		// Energy conservation
		(1.f - r);
	
	return r + t;
}

OPENCL_FORCE_NOT_INLINE float3 RoughMatteTranslucentMaterial_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 krVal, const float3 ktVal, const float sigma) {
	const float3 r = Spectrum_Clamp(krVal);
	const float3 t = Spectrum_Clamp(ktVal) * 
		// Energy conservation
		(1.f - r);

	const bool isKtBlack = Spectrum_IsBlack(t);
	const bool isKrBlack = Spectrum_IsBlack(r);

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
			return BLACK;
		}
	}

	const bool relfected = (CosTheta(lightDir) * CosTheta(eyeDir) > 0.f);
	const float weight = (lightDir.z * eyeDir.z > 0.f) ? threshold : (1.f - threshold);

	if (directPdfW)
		*directPdfW = weight * fabs(lightDir.z * M_1_PI_F);

	const float sigma2 = sigma * sigma;
	const float A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
	const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	const float sinthetai = SinTheta(eyeDir);
	const float sinthetao = SinTheta(lightDir);
	float maxcos = 0.f;
	if (sinthetai > 1e-4f && sinthetao > 1e-4f) {
		const float dcos = CosPhi(lightDir) * CosPhi(eyeDir) +
			SinPhi(lightDir) * SinPhi(eyeDir);
		maxcos = fmax(0.f, dcos);
	}

	const float3 result = (M_1_PI_F * fabs(lightDir.z) *
		(A + B * maxcos * sinthetai * sinthetao / fmax(fabs(CosTheta(lightDir)), fabs(CosTheta(eyeDir)))));

	if (lightDir.z * eyeDir.z > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * result;
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * result;
	}
}

OPENCL_FORCE_NOT_INLINE float3 RoughMatteTranslucentMaterial_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float3 krVal, const float3 ktVal, const float sigma) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, pdfW);
	if (fabs(CosTheta(*sampledDir)) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float3 kr = Spectrum_Clamp(krVal);
	const float3 kt = Spectrum_Clamp(ktVal) * 
		// Energy conservation
		(1.f - kr);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

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
			return BLACK;
	}

	const float sigma2 = sigma * sigma;
	const float A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
	const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	const float sinthetai = SinTheta(fixedDir);
	const float sinthetao = SinTheta(*sampledDir);
	float maxcos = 0.f;
	if (sinthetai > 1e-4f && sinthetao > 1e-4f) {
		const float dcos = CosPhi(*sampledDir) * CosPhi(fixedDir) +
			SinPhi(*sampledDir) * SinPhi(fixedDir);
		maxcos = max(0.f, dcos);
	}
	const float coef = (A + B * maxcos * sinthetai * sinthetao / max(fabs(CosTheta(*sampledDir)), fabs(CosTheta(fixedDir))));

	if (passThroughEvent < threshold) {
		*sampledDir *= (signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | REFLECT;
		*pdfW *= threshold;

		return kr * (coef / threshold);
	} else {
		*sampledDir *= -(signbit(fixedDir.z) ? -1.f : 1.f);
		*event = DIFFUSE | TRANSMIT;
		*pdfW *= (1.f - threshold);

		return kt * (coef / (1.f - threshold));
	}
}
