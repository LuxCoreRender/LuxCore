#line 2 "materialdefs_funcs_roughglass.cl"

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
// RoughGlass material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void RoughGlassMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
    const float3 albedo = WHITE;

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void RoughGlassMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughGlassMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughGlassMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughGlassMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughGlassMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	const float3 ktVal = Texture_GetSpectrumValue(material->roughglass.ktTexIndex, hitPoint TEXTURES_PARAM);
	const float3 krVal = Texture_GetSpectrumValue(material->roughglass.krTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kt = Spectrum_Clamp(ktVal);
	const float3 kr = Spectrum_Clamp(krVal);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack) {
		MATERIAL_EVALUATE_RETURN_BLACK;
	}
	
	const float nc = ExtractExteriorIors(hitPoint, material->roughglass.exteriorIorTexIndex TEXTURES_PARAM);
	const float nt = ExtractInteriorIors(hitPoint, material->roughglass.interiorIorTexIndex TEXTURES_PARAM);
	const float ntc = nt / nc;

	const float nuVal = Texture_GetFloatValue(material->roughglass.nuTexIndex, hitPoint TEXTURES_PARAM);
	const float nvVal = Texture_GetFloatValue(material->roughglass.nvTexIndex, hitPoint TEXTURES_PARAM);
	const float u = clamp(nuVal, 1e-9f, 1.f);
	const float v = clamp(nvVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	float directPdfW;
	BSDFEvent event;
	float3 result;
	const float threshold = isKrBlack ? 1.f : (isKtBlack ? 0.f : .5f);
	if (lightDir.z * eyeDir.z < 0.f) {
		// Transmit

		const bool entering = (CosTheta(lightDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;

		float3 wh = eta * lightDir + eyeDir;
		if (wh.z > 0.f)
			wh = -wh;

		const float lengthSquared = dot(wh, wh);
		if (!(lengthSquared > 0.f)) {
			MATERIAL_EVALUATE_RETURN_BLACK;
		}
		wh /= sqrt(lengthSquared);
		const float cosThetaI = fabs(CosTheta(eyeDir));
		const float cosThetaIH = fabs(dot(eyeDir, wh));
		const float cosThetaOH = dot(lightDir, wh);

		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, lightDir, eyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float F = FresnelCauchy_Evaluate(ntc, cosThetaOH);

		directPdfW = threshold * specPdf * (fabs(cosThetaOH) * eta * eta) / lengthSquared;

		//if (reversePdfW)
		//	*reversePdfW = threshold * specPdf * cosThetaIH / lengthSquared;

		result = (fabs(cosThetaOH) * cosThetaIH * D *
			G / (cosThetaI * lengthSquared)) *
			kt * (1.f - F);

        event = GLOSSY | TRANSMIT;
	} else {
		// Reflect
		const float cosThetaO = fabs(CosTheta(lightDir));
		const float cosThetaI = fabs(CosTheta(eyeDir));
		if (cosThetaO == 0.f || cosThetaI == 0.f) {
			MATERIAL_EVALUATE_RETURN_BLACK;
		}
		float3 wh = lightDir + eyeDir;
		if (all(isequal(wh, BLACK))) {
			MATERIAL_EVALUATE_RETURN_BLACK;
		}
		wh = normalize(wh);
		if (wh.z < 0.f)
			wh = -wh;

		float cosThetaH = dot(eyeDir, wh);
		const float D = SchlickDistribution_D(roughness, wh, anisotropy);
		const float G = SchlickDistribution_G(roughness, lightDir, eyeDir);
		const float specPdf = SchlickDistribution_Pdf(roughness, wh, anisotropy);
		const float F = FresnelCauchy_Evaluate(ntc, cosThetaH);

		directPdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(lightDir, wh)));

		//if (reversePdfW)
		//	*reversePdfW = (1.f - threshold) * specPdf / (4.f * fabs(dot(lightDir, wh));

		result = (D * G / (4.f * cosThetaI)) * kr * F;
		
		const float localFilmThickness = (material->roughglass.filmThicknessTexIndex != NULL_INDEX) 
										 ? Texture_GetFloatValue(material->roughglass.filmThicknessTexIndex, hitPoint TEXTURES_PARAM) : 0.f;
		if (localFilmThickness > 0.f) {
			const float localFilmIor = (material->roughglass.filmIorTexIndex != NULL_INDEX) 
									   ? Texture_GetFloatValue(material->roughglass.filmIorTexIndex, hitPoint TEXTURES_PARAM) : 1.f;
			result *= CalcFilmColor(eyeDir, localFilmThickness, localFilmIor);
		}

        event = GLOSSY | REFLECT;
	}

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

OPENCL_FORCE_INLINE void RoughGlassMaterial_Sample(__global const Material* restrict material,
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

	const float3 ktVal = Texture_GetSpectrumValue(material->roughglass.ktTexIndex, hitPoint TEXTURES_PARAM);
	const float3 krVal = Texture_GetSpectrumValue(material->roughglass.krTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kt = Spectrum_Clamp(ktVal);
	const float3 kr = Spectrum_Clamp(krVal);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	const float nuVal = Texture_GetFloatValue(material->roughglass.nuTexIndex, hitPoint TEXTURES_PARAM);
	const float nvVal = Texture_GetFloatValue(material->roughglass.nvTexIndex, hitPoint TEXTURES_PARAM);
	const float u = clamp(nuVal, 1e-9f, 1.f);
	const float v = clamp(nvVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	float3 wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);

	const float nc = ExtractExteriorIors(hitPoint, material->roughglass.exteriorIorTexIndex TEXTURES_PARAM);
	const float nt = ExtractInteriorIors(hitPoint, material->roughglass.interiorIorTexIndex TEXTURES_PARAM);
	const float ntc = nt / nc;

	const float coso = fabs(fixedDir.z);

	// Decide to transmit or reflect
	float threshold;
	if (!isKrBlack) {
		if (!isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if (!isKtBlack)
			threshold = 1.f;
		else {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}
	}

	float pdfW;
	BSDFEvent event;
	float3 sampledDir;
	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit

		if (wh.z > 0.f)
			wh = -wh;
		const float cosThetaOH = dot(fixedDir, wh);

		const bool entering = (CosTheta(fixedDir) > 0.f);
		const float eta = entering ? (nc / nt) : ntc;
		const float eta2 = eta * eta;
		const float sinThetaIH2 = eta2 * fmax(0.f, 1.f - cosThetaOH * cosThetaOH);
		if (sinThetaIH2 >= 1.f) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}
		float cosThetaIH = sqrt(1.f - sinThetaIH2);
		if (entering)
			cosThetaIH = -cosThetaIH;
		const float length = eta * cosThetaOH + cosThetaIH;
		sampledDir = length * wh - eta * fixedDir;

		const float lengthSquared = length * length;
		pdfW = specPdf * fabs(cosThetaIH) / lengthSquared;
		if (pdfW <= 0.f) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		const float cosi = fabs(sampledDir.z);

		const float G = SchlickDistribution_G(roughness, fixedDir, sampledDir);
		float factor = (d / specPdf) * G * fabs(cosThetaOH) / threshold;

		//if (!hitPoint.fromLight) {
			const float F = FresnelCauchy_Evaluate(ntc, cosThetaIH);
			result = (factor / coso) * kt * (1.f - F);
		//} else {
		//	const Spectrum F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//	result = (factor / cosi) * kt * (Spectrum(1.f) - F);
		//}

		pdfW *= threshold;
		event = GLOSSY | TRANSMIT;
	} else {
		// Reflect

		if (wh.z < 0.f)
			wh = -wh;
		const float cosThetaOH = dot(fixedDir, wh);

		pdfW = specPdf / (4.f * fabs(cosThetaOH));
		if (pdfW <= 0.f) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		sampledDir = 2.f * cosThetaOH * wh - fixedDir;

		const float cosi = fabs(sampledDir.z);
		if ((cosi < DEFAULT_COS_EPSILON_STATIC) || (fixedDir.z * sampledDir.z < 0.f)) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		const float G = SchlickDistribution_G(roughness, fixedDir, sampledDir);
		float factor = (d / specPdf) * G * fabs(cosThetaOH) / (1.f - threshold);

		const float F = FresnelCauchy_Evaluate(ntc, cosThetaOH);
		//factor /= (!hitPoint.fromLight) ? coso : cosi;
		factor /= coso;
		result = factor * F * kr;
		
		const float localFilmThickness = (material->roughglass.filmThicknessTexIndex != NULL_INDEX) 
										 ? Texture_GetFloatValue(material->roughglass.filmThicknessTexIndex, hitPoint TEXTURES_PARAM) : 0.f;
		if (localFilmThickness > 0.f) {
			const float localFilmIor = (material->roughglass.filmIorTexIndex != NULL_INDEX) 
									   ? Texture_GetFloatValue(material->roughglass.filmIorTexIndex, hitPoint TEXTURES_PARAM) : 1.f;
			result *= CalcFilmColor(fixedDir, localFilmThickness, localFilmIor);
		}

		pdfW *= (1.f - threshold);
		event = GLOSSY | REFLECT;
	}

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void RoughGlassMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			RoughGlassMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			RoughGlassMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			RoughGlassMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			RoughGlassMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			RoughGlassMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			RoughGlassMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			RoughGlassMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
