#line 2 "materialdefs_funcs_glossy2.cl"

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
// Glossy2 material
//
// LuxRender Glossy2 material porting.
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void Glossy2Material_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->glossy2.kdTexIndex,
			hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_NOT_INLINE float3 Glossy2Material_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float i,
		const float nuVal,
		const float nvVal,
		const float3 kaVal,
		const float d,
		const int multibounceVal,
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
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = Spectrum_Clamp(ks);

	const float u = clamp(nuVal, 1e-9f, 1.f);
	const float v = clamp(nvVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * fabs(sampledDir.z * M_1_PI_F) +
			wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);
	}

	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

	const float3 alpha = Spectrum_Clamp(kaVal);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

	const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounceVal,
			fixedDir, sampledDir);

	// Blend in base layer Schlick style
	// assumes coating bxdf takes fresnel factor S into account

	return coatingF + absorption * (WHITE - S) * baseF;
}

OPENCL_FORCE_NOT_INLINE float3 Glossy2Material_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float i,
		const float nuVal,
		const float nvVal,
		const float3 kaVal,
		const float d,
		const int multibounceVal,
		const float3 kdVal, const float3 ksVal) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	if (fixedDir.z <= 0.f) {
		// Back Face
		*sampledDir = -1.f * CosineSampleHemisphereWithPdf(u0, u1, pdfW);
		if (fabs(CosTheta(*sampledDir)) < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;
		*event = DIFFUSE | REFLECT;
		return Spectrum_Clamp(kdVal);
	}

	float3 ks = ksVal;
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = Spectrum_Clamp(ks);

	const float u = clamp(nuVal, 1e-9f, 1.f);
	const float v = clamp(nvVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	// Coating is used only on the front face
	const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
	const float wBase = 1.f - wCoating;

	float basePdf, coatingPdf;
	float3 baseF, coatingF;
	if (passThroughEvent < wBase) {
		// Sample base BSDF (Matte BSDF)
		baseF = Spectrum_Clamp(kdVal);
		if (Spectrum_IsBlack(baseF))
			return BLACK;
		
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &basePdf);
		if (fabs(CosTheta(*sampledDir)) < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		baseF *= basePdf;

		// Evaluate coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounceVal,
				fixedDir, *sampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				multibounceVal, fixedDir, sampledDir, u0, u1, &coatingPdf);
		if (Spectrum_IsBlack(coatingF))
			return BLACK;

		const float absCosSampledDir = fabs(CosTheta(*sampledDir));
		if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		coatingF *= coatingPdf;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = absCosSampledDir * M_1_PI_F;
		baseF = Spectrum_Clamp(kdVal) * basePdf;
	}

	*event = GLOSSY | REFLECT;

	*pdfW = coatingPdf * wCoating + basePdf * wBase;

	// Absorption
	const float cosi = fabs((*sampledDir).z);
	const float coso = fabs(fixedDir.z);

	const float3 alpha = Spectrum_Clamp(kaVal);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + *sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(*sampledDir, H)));

	// Blend in base layer Schlick style
	// coatingF already takes fresnel factor S into account

	return (coatingF + absorption * (WHITE - S) * baseF) / *pdfW;
}
