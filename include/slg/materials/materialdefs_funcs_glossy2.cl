#line 2 "materialdefs_funcs_glossy2.cl"

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
// Glossy2 material
//
// LuxRender Glossy2 material porting.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLOSSY2)

BSDFEvent Glossy2Material_GetEventTypes() {
	return GLOSSY | REFLECT;
}

float3 Glossy2Material_Evaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
		const float i,
#endif
		const float nuVal,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
		const float nvVal,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
		const float3 kaVal,
		const float d,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
		const int multibounceVal,
#endif
		const float3 kdVal, const float3 ksVal) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * fabs(lightDir.z);
	if (eyeDir.z <= 0.f) {
		// Back face: no coating

		if (directPdfW)
			*directPdfW = fabs(sampledDir.z * M_1_PI_F);

		*event = DIFFUSE | REFLECT;
		return baseF;
	}

	// Front face: coating+base
	*event = GLOSSY | REFLECT;

	float3 ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
#endif
	ks = Spectrum_Clamp(ks);

	const float u = clamp(nuVal, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float v = clamp(nvVal, 0.f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * fabs(sampledDir.z * M_1_PI_F) +
			wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);
	}

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

	const float3 alpha = Spectrum_Clamp(kaVal);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);
#else
	const float3 absorption = WHITE;
#endif

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = multibounceVal;
#else
	const int multibounce = 0;
#endif
	const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,
			fixedDir, sampledDir);

	// Blend in base layer Schlick style
	// assumes coating bxdf takes fresnel factor S into account

	return coatingF + absorption * (WHITE - S) * baseF;
}

float3 Glossy2Material_Sample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
		const float i,
#endif
		const float nuVal,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
		const float nvVal,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
		const float3 kaVal,
		const float d,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
		const int multibounceVal,
#endif
		const float3 kdVal, const float3 ksVal) {
	if ((!(requestedEvent & (GLOSSY | REFLECT)) && fixedDir.z > 0.f) ||
		(!(requestedEvent & (DIFFUSE | REFLECT)) && fixedDir.z <= 0.f) ||
		(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	if (fixedDir.z <= 0.f) {
		// Back Face
		*sampledDir = -1.f * CosineSampleHemisphereWithPdf(u0, u1, pdfW);
		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;
		*event = DIFFUSE | REFLECT;
		return Spectrum_Clamp(kdVal);
	}

	float3 ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
#endif
	ks = Spectrum_Clamp(ks);

	const float u = clamp(nuVal, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float v = clamp(nvVal, 0.f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	// Coating is used only on the front face
	const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
	const float wBase = 1.f - wCoating;

	const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F;

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = multibounceVal;
#else
	const int multibounce = 0;
#endif

	float basePdf, coatingPdf;
	float3 coatingF;
	if (passThroughEvent < wBase) {
		// Sample base BSDF (Matte BSDF)
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &basePdf);

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		// Evaluate coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,
				fixedDir, *sampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);

		*event = GLOSSY | REFLECT;
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				multibounce, fixedDir, sampledDir, u0, u1, &coatingPdf);
		if (Spectrum_IsBlack(coatingF))
			return BLACK;

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		coatingF *= coatingPdf;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = *cosSampledDir * M_1_PI_F;

		*event = GLOSSY | REFLECT;
	}

	*pdfW = coatingPdf * wCoating + basePdf * wBase;

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
	// Absorption
	const float cosi = fabs((*sampledDir).z);
	const float coso = fabs(fixedDir.z);

	const float3 alpha = Spectrum_Clamp(kaVal);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);
#else
	const float3 absorption = WHITE;
#endif

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + *sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(*sampledDir, H)));

	// Blend in base layer Schlick style
	// coatingF already takes fresnel factor S into account

	return (coatingF + absorption * (WHITE - S) * baseF * *cosSampledDir) / *pdfW;
}

#endif
