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
		MATERIALS_PARAM_DECL) {
	const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->glossy2.kdTexIndex,
			hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void Glossy2Material_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void Glossy2Material_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float3 kdVal = Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint TEXTURES_PARAM);

	const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * fabs(lightDir.z);

	// Front face: coating+base
	const BSDFEvent event = GLOSSY | REFLECT;

	const float3 ksVal = Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint TEXTURES_PARAM);
	float3 ks = ksVal;
	const float i = Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint TEXTURES_PARAM);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = Spectrum_Clamp(ks);

	const float nuVal = Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint TEXTURES_PARAM);
	const float nvVal = Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint TEXTURES_PARAM);

	const float u = clamp(nuVal, 1e-9f, 1.f);
	const float v = clamp(nvVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	// Direct pdf
	const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
	const float wBase = 1.f - wCoating;
	const float directPdfW = wBase * fabs(sampledDir.z * M_1_PI_F) +
		wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);

	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

	const float3 kaVal = Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint TEXTURES_PARAM);
	const float3 alpha = Spectrum_Clamp(kaVal);
	const float d = Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

	const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, material->glossy2.multibounce,
			fixedDir, sampledDir); 

	// Blend in base layer Schlick style
	// assumes coating bxdf takes fresnel factor S into account

	const float3 result = coatingF + absorption * (WHITE - S) * baseF;

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

OPENCL_FORCE_INLINE void Glossy2Material_Sample(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	const float3 kdVal = Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint TEXTURES_PARAM);

	const float3 ksVal = Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint TEXTURES_PARAM);
	float3 ks = ksVal;
	const float i = Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint TEXTURES_PARAM);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = Spectrum_Clamp(ks);

	const float nuVal = Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint TEXTURES_PARAM);
	const float nvVal = Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint TEXTURES_PARAM);

	const float u = clamp(nuVal, 1e-5f, 1.f);
	const float v = clamp(nvVal, 1e-5f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	// Coating is used only on the front face
	const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
	const float wBase = 1.f - wCoating;

	float3 sampledDir;
	float basePdf, coatingPdf;
	float3 baseF, coatingF;
	if (passThroughEvent < wBase) {
		
		sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &basePdf);
		if (fabs(sampledDir.z) < DEFAULT_COS_EPSILON_STATIC) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		baseF = Spectrum_Clamp (kdVal) * M_1_PI_F * fabs(sampledDir.z);

		// Evaluate coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, material->glossy2.multibounce,
				fixedDir, sampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				material->glossy2.multibounce, fixedDir, &sampledDir, u0, u1, &coatingPdf);
		if (Spectrum_IsBlack(coatingF)) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		const float absCosSampledDir = fabs(sampledDir.z);
		if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		coatingF *= coatingPdf;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = absCosSampledDir * M_1_PI_F;
		baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * absCosSampledDir; 
	}

	const BSDFEvent event = GLOSSY | REFLECT;

	const float pdfW = coatingPdf * wCoating + basePdf * wBase;

	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

	const float3 kaVal = Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint TEXTURES_PARAM);
	const float3 alpha = Spectrum_Clamp(kaVal);
	const float d = Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

	// Blend in base layer Schlick style
	// coatingF already takes fresnel factor S into account

	const float3 result = (coatingF + absorption * (WHITE - S) * baseF) / pdfW;

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void Glossy2Material_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			Glossy2Material_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			Glossy2Material_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			Glossy2Material_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			Glossy2Material_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			Glossy2Material_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			Glossy2Material_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			Glossy2Material_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
