#line 2 "materialdefs_funcs_glossytranslucent.cl"

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
// GlossyTranslucent material
//
// LuxRender GlossyTranslucent material porting.
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
    const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float cosi = fabs(lightDir.z);
	const float coso = fabs(eyeDir.z);

	const float i = Texture_GetFloatValue(material->glossytranslucent.indexTexIndex, hitPoint TEXTURES_PARAM);
	const float i_bf = Texture_GetFloatValue(material->glossytranslucent.indexbfTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kaVal = Texture_GetSpectrumValue(material->glossytranslucent.kaTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kaVal_bf = Texture_GetSpectrumValue(material->glossytranslucent.kabfTexIndex, hitPoint TEXTURES_PARAM);
	const float d = Texture_GetFloatValue(material->glossytranslucent.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float d_bf = Texture_GetFloatValue(material->glossytranslucent.depthbfTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kdVal = Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM);
	const float3 ksVal = Texture_GetSpectrumValue(material->glossytranslucent.ksTexIndex, hitPoint TEXTURES_PARAM);
	const float3 ksVal_bf = Texture_GetSpectrumValue(material->glossytranslucent.ksbfTexIndex, hitPoint TEXTURES_PARAM);

	// Note: this is the same side test used by matte translucent material and
	// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
	// are not available here without bump mapping.
	const float sideTest = CosTheta(lightDir) * CosTheta(eyeDir);

	float directPdfW;
	BSDFEvent event;
	float3 result;
	if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmission
		event = DIFFUSE | TRANSMIT;

		directPdfW = fabs(sampledDir.z) * (M_1_PI_F * .5f);

		const float3 H = normalize(MAKE_FLOAT3(lightDir.x + eyeDir.x, lightDir.y + eyeDir.y,
			lightDir.z - eyeDir.z));
		const float u = fabs(dot(lightDir, H));
		float3 ks = ksVal;

		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S1 = FresnelSchlick_Evaluate(ks, u);

		ks = ksVal_bf;

		if (i_bf > 0.f) {
			const float ti = (i_bf - 1.f) / (i_bf + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S2 = FresnelSchlick_Evaluate(ks, u);
		float3 S = Spectrum_Sqrt((WHITE - S1) * (WHITE - S2));

		if (lightDir.z > 0.f) {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / cosi) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / coso));
		} else {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / coso) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / cosi));
		}

		const float3 ktVal = Texture_GetSpectrumValue(material->glossytranslucent.ktTexIndex, hitPoint TEXTURES_PARAM);
		result = (M_1_PI_F * cosi) * S * Spectrum_Clamp(ktVal) *
			(WHITE - Spectrum_Clamp(kdVal));
	} else if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection
		event = GLOSSY | REFLECT;

		const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * cosi;
		float3 ks;
		float index;
		float u;
		float v;
		float3 alpha;
		float depth;
		int mbounce;

		if (eyeDir.z >= 0.f) {
			ks = ksVal;
			index = i;
			const float nuVal = Texture_GetFloatValue(material->glossytranslucent.nuTexIndex, hitPoint TEXTURES_PARAM);
			const float nvVal = Texture_GetFloatValue(material->glossytranslucent.nvTexIndex, hitPoint TEXTURES_PARAM);
			u = clamp(nuVal, 1e-9f, 1.f);
			v = clamp(nvVal, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal);
			depth = d;
			mbounce = material->glossytranslucent.multibounce;
		} else {
			ks = ksVal_bf;
			index = i_bf;
			const float nuVal_bf = Texture_GetFloatValue(material->glossytranslucent.nubfTexIndex, hitPoint TEXTURES_PARAM);
			const float nvVal_bf = Texture_GetFloatValue(material->glossytranslucent.nvbfTexIndex, hitPoint TEXTURES_PARAM);
			u = clamp(nuVal_bf, 1e-9f, 1.f);
			v = clamp(nvVal_bf, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal_bf);
			depth = d_bf;
			mbounce = material->glossytranslucent.multibouncebf;
		}

		if (index > 0.f) {
			const float ti = (index - 1.f) / (index + 1.f);
			ks *= ti * ti;
		}
		ks = Spectrum_Clamp(ks);

		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		directPdfW = .5f * (wBase * fabs(sampledDir.z * M_1_PI_F) +
			wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir));

		// Absorption
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, depth);

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + sampledDir);
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

		const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, mbounce, fixedDir, sampledDir);

		// Blend in base layer Schlick style
		// assumes coating bxdf takes fresnel factor S into account

		result = coatingF + absorption * (WHITE - S) * baseF;
	} else {
		MATERIAL_EVALUATE_RETURN_BLACK;
	}

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_Sample(__global const Material* restrict material,
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

	const float i = Texture_GetFloatValue(material->glossytranslucent.indexTexIndex, hitPoint TEXTURES_PARAM);
	const float i_bf = Texture_GetFloatValue(material->glossytranslucent.indexbfTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kaVal = Texture_GetSpectrumValue(material->glossytranslucent.kaTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kaVal_bf = Texture_GetSpectrumValue(material->glossytranslucent.kabfTexIndex, hitPoint TEXTURES_PARAM);
	const float d = Texture_GetFloatValue(material->glossytranslucent.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float d_bf = Texture_GetFloatValue(material->glossytranslucent.depthbfTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kdVal = Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM);
	const float3 ksVal = Texture_GetSpectrumValue(material->glossytranslucent.ksTexIndex, hitPoint TEXTURES_PARAM);
	const float3 ksVal_bf = Texture_GetSpectrumValue(material->glossytranslucent.ksbfTexIndex, hitPoint TEXTURES_PARAM);

	BSDFEvent event;
	float pdfW;
	float3 sampledDir;
	float3 result;
	if (passThroughEvent < .5f) {
		// Reflection
		float3 ks;
		float index;
		float u;
		float v;
		float3 alpha;
		float depth;
		int mbounce;

		if (fixedDir.z >= 0.f) {
			ks = ksVal;
			index = i;
			const float nuVal = Texture_GetFloatValue(material->glossytranslucent.nuTexIndex, hitPoint TEXTURES_PARAM);
			const float nvVal = Texture_GetFloatValue(material->glossytranslucent.nvTexIndex, hitPoint TEXTURES_PARAM);
			u = clamp(nuVal, 1e-9f, 1.f);
			v = clamp(nvVal, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal);
			depth = d;
			mbounce = material->glossytranslucent.multibounce;
		} else {
			ks = ksVal_bf;
			index = i_bf;
			const float nuVal_bf = Texture_GetFloatValue(material->glossytranslucent.nubfTexIndex, hitPoint TEXTURES_PARAM);
			const float nvVal_bf = Texture_GetFloatValue(material->glossytranslucent.nvbfTexIndex, hitPoint TEXTURES_PARAM);
			u = clamp(nuVal_bf, 1e-9f, 1.f);
			v = clamp(nvVal_bf, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal_bf);
			depth = d_bf;
			mbounce = material->glossytranslucent.multibouncebf;
		}

		if (index > 0.f) {
			const float ti = (index - 1.f) / (index + 1.f);
			ks *= ti * ti;
		}
		ks = Spectrum_Clamp(ks);

		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		float basePdf, coatingPdf;
		float3 baseF, coatingF;

		if (2.f * passThroughEvent < wBase) {
			// Sample base BSDF (Matte BSDF)
			sampledDir = CosineSampleHemisphereWithPdf(u0, u1, &basePdf);
			sampledDir *= signbit(fixedDir.z) ? -1.f : 1.f;

			const float absCosSampledDir = fabs(CosTheta(sampledDir));
			if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) {
				MATERIAL_SAMPLE_RETURN_BLACK;
			}

			baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * absCosSampledDir;

			// Evaluate coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, mbounce,
				fixedDir, sampledDir);
			coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);

		} else {
			// Sample coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy, mbounce,
				fixedDir, &sampledDir, u0, u1, &coatingPdf);
			if (Spectrum_IsBlack(coatingF)) {
				MATERIAL_SAMPLE_RETURN_BLACK;
			}

			const float absCosSampledDir = fabs(CosTheta(sampledDir));
			if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) {
				MATERIAL_SAMPLE_RETURN_BLACK;
			}

			coatingF *= coatingPdf;

			// Evaluate base BSDF (Matte BSDF)
			basePdf = absCosSampledDir * M_1_PI_F;
			baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * absCosSampledDir;
		}
		event = GLOSSY | REFLECT;

		// Note: this is the same side test used by matte translucent material and
		// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
		// are not available here without bump mapping.
		const float sideTest = CosTheta(fixedDir) * CosTheta(sampledDir);
		if (!(sideTest > DEFAULT_COS_EPSILON_STATIC)) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		pdfW = .5f * (coatingPdf * wCoating + basePdf * wBase);

		// Absorption
		const float cosi = fabs(sampledDir.z);
		const float coso = fabs(fixedDir.z);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + sampledDir);
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

		// Blend in base layer Schlick style
		// coatingF already takes fresnel factor S into account

		result = (coatingF + absorption * (WHITE - S) * baseF) / pdfW;
	} else {
		// Transmission
		sampledDir = CosineSampleHemisphereWithPdf(u0, u1, &pdfW);
		sampledDir *= signbit(fixedDir.z) ? 1.f : -1.f;

		// Note: this is the same side test used by matte translucent material and
		// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
		// are not available here without bump mapping.
		const float sideTest = CosTheta(fixedDir) * CosTheta(sampledDir);
		if (!(sideTest < -DEFAULT_COS_EPSILON_STATIC)) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		pdfW *= .5f;

		const float absCosSampledDir = fabs(CosTheta(sampledDir));
		if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		event = DIFFUSE | TRANSMIT;

		// This is an inline expansion of Evaluate(hitPoint, *localSampledDir,
		// localFixedDir, event, pdfW, NULL) / pdfW
		//
		// Note: pdfW and the pdf computed inside Evaluate() are exactly the same
		
		const float cosi = fabs(sampledDir.z);
		const float coso = fabs(fixedDir.z);

		const float3 H = normalize(MAKE_FLOAT3(sampledDir.x + fixedDir.x, sampledDir.y + fixedDir.y,
			sampledDir.z - fixedDir.z));
		const float u = fabs(dot(sampledDir, H));
		float3 ks = ksVal;

		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S1 = FresnelSchlick_Evaluate(ks, u);

		ks = ksVal_bf;

		if (i_bf > 0.f) {
			const float ti = (i_bf - 1.f) / (i_bf + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S2 = FresnelSchlick_Evaluate(ks, u);
		float3 S = Spectrum_Sqrt((WHITE - S1) * (WHITE - S2));

		if (sampledDir.z > 0.f) {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / cosi) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / coso));
		} else {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / coso) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / cosi));
		}

		const float3 ktVal = Texture_GetSpectrumValue(material->glossytranslucent.ktTexIndex, hitPoint TEXTURES_PARAM);
		result = (M_1_PI_F * cosi / pdfW) * S * Spectrum_Clamp(ktVal) *
			(WHITE - Spectrum_Clamp(kdVal));
	}

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void GlossyTranslucentMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			GlossyTranslucentMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			GlossyTranslucentMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			GlossyTranslucentMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			GlossyTranslucentMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			GlossyTranslucentMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			GlossyTranslucentMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			GlossyTranslucentMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
