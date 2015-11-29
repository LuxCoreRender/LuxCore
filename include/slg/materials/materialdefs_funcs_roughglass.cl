#line 2 "materialdefs_funcs_roughglass.cl"

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

//------------------------------------------------------------------------------
// RoughGlass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)

BSDFEvent RoughGlassMaterial_GetEventTypes() {
	return GLOSSY | REFLECT | TRANSMIT;
}

bool RoughGlassMaterial_IsDelta() {
	return false;
}

float3 RoughGlassMaterial_Evaluate(
		__global HitPoint *hitPoint, const float3 localLightDir, const float3 localEyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 ktVal, const float3 krVal,
		const float nuVal,
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
		const float nvVal,
#endif
		const float nc, const float nt
		) {
	const float3 kt = Spectrum_Clamp(ktVal);
	const float3 kr = Spectrum_Clamp(ktVal);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;
	
	const float ntc = nt / nc;

	const float u = clamp(nuVal, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
	const float v = clamp(nvVal, 0.f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (localLightDir.z * localEyeDir.z < 0.f) {
		// Transmit

		const bool entering = (CosTheta(localLightDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		float3 wh = eta * localLightDir + localEyeDir;
		if (wh.z < 0.f)
			wh = -wh;

		const float lengthSquared = dot(wh, wh);
		if (!(lengthSquared > 0.f))
			return BLACK;
		wh /= sqrt(lengthSquared);
		const float cosThetaI = fabs(CosTheta(localEyeDir));
		const float cosThetaIH = fabs(dot(localEyeDir, wh));
		const float cosThetaOH = dot(localLightDir, wh);

		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaOH);

		if (directPdfW)
			*directPdfW = threshold * specPdf * (fabs(cosThetaOH) * eta * eta) / lengthSquared;

		//if (reversePdfW)
		//	*reversePdfW = threshold * specPdf * cosThetaIH / lengthSquared;

		float3 result = (fabs(cosThetaOH) * cosThetaIH * D *
			G / (cosThetaI * lengthSquared)) *
			kt * (1.f - F);

        *event = DIFFUSE | TRANSMIT;

		return result;
	} else {
		// Reflect
		const float cosThetaO = fabs(CosTheta(localLightDir));
		const float cosThetaI = fabs(CosTheta(localEyeDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f)
			return BLACK;
		float3 wh = localLightDir + localEyeDir;
		if (all(isequal(wh, BLACK)))
			return BLACK;
		wh = normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		float cosThetaH = dot(localEyeDir, wh);
		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, localLightDir, localEyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaH);

		if (directPdfW)
			*directPdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(localLightDir, wh)));

		//if (reversePdfW)
		//	*reversePdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(localLightDir, wh));

		float3 result = (D * G / (4.f * cosThetaI)) * kr * F;

        *event = DIFFUSE | REFLECT;

		return result;
	}
}

float3 RoughGlassMaterial_Sample(
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 ktVal, const float3 krVal,
		const float nuVal,
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
		const float nvVal,
#endif
		const float nc, const float nt
		) {
	if (!(requestedEvent & (GLOSSY | REFLECT | TRANSMIT)) ||
			(fabs(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	const float3 kt = Spectrum_Clamp(ktVal);
	const float3 kr = Spectrum_Clamp(ktVal);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const float u = clamp(nuVal, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
	const float v = clamp(nvVal, 0.f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	float3 wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	if (wh.z < 0.f)
		wh = -wh;
	const float cosThetaOH = dot(localFixedDir, wh);

	const float ntc = nt / nc;

	const float coso = fabs(localFixedDir.z);

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

		const bool entering = (CosTheta(localFixedDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;
		const float eta2 = eta * eta;
		const float sinThetaIH2 = eta2 * fmax(0.f, 1.f - cosThetaOH * cosThetaOH);
		if (sinThetaIH2 >= 1.f)
			return BLACK;
		float cosThetaIH = sqrt(1.f - sinThetaIH2);
		if (entering)
			cosThetaIH = -cosThetaIH;
		const float length = eta * cosThetaOH + cosThetaIH;
		*localSampledDir = length * wh - eta * localFixedDir;

		const float lengthSquared = length * length;
		*pdfW = specPdf * fabs(cosThetaIH) / lengthSquared;
		if (*pdfW <= 0.f)
			return BLACK;

		const float cosi = fabs((*localSampledDir).z);
		*absCosSampledDir = cosi;

		const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
		float factor = (d / specPdf) * G * fabs(cosThetaOH) / threshold;

		//if (!hitPoint.fromLight) {
			const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaIH);
			result = (factor / coso) * kt * (1.f - F);
		//} else {
		//	const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//	result = (factor / cosi) * kt * (Spectrum(1.f) - F);
		//}

		*pdfW *= threshold;
		*event = GLOSSY | TRANSMIT;
	} else {
		// Reflect
		*pdfW = specPdf / (4.f * fabs(cosThetaOH));
		if (*pdfW <= 0.f)
			return BLACK;

		*localSampledDir = 2.f * cosThetaOH * wh - localFixedDir;

		const float cosi = fabs((*localSampledDir).z);
		*absCosSampledDir = cosi;
		if ((*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) || (localFixedDir.z * (*localSampledDir).z < 0.f))
			return BLACK;

		const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
		float factor = (d / specPdf) * G * fabs(cosThetaOH) / (1.f - threshold);

		const float3 F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//factor /= (!hitPoint.fromLight) ? coso : cosi;
		factor /= coso;
		result = factor * F * kr;

		*pdfW *= (1.f - threshold);
		*event = GLOSSY | REFLECT;
	}

	return result;
}

#endif
