#line 2 "materialdefs_funcs_carpaint.cl"

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
// CarPaint material
//
// LuxRender carpaint material porting.
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void CarPaintMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
    const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void CarPaintMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void CarPaintMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void CarPaintMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void CarPaintMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void CarPaintMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	float3 H = normalize(lightDir + eyeDir);
	if ((H.x == 0.f) && (H.y == 0.f) && (H.z == 0.f)) {
		MATERIAL_EVALUATE_RETURN_BLACK;
	}
	if (H.z < 0.f)
		H = -H;

	float pdf = 0.f;
	int n = 1; // already counts the diffuse layer

	// Absorption
	const float cosi = fabs(lightDir.z);
	const float coso = fabs(eyeDir.z);
	const float3 kaVal = Texture_GetSpectrumValue(material->carpaint.KaTexIndex, hitPoint TEXTURES_PARAM);
	const float3 alpha = Spectrum_Clamp(kaVal);
	const float d = Texture_GetFloatValue(material->carpaint.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Diffuse layer
	const float3 kdVal = Texture_GetSpectrumValue(material->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM);
	float3 result = absorption * Spectrum_Clamp(kdVal) * M_1_PI_F * fabs(lightDir.z);

	// 1st glossy layer
	const float3 ks1Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float3 ks1 = Spectrum_Clamp(ks1Val);
	const float m1 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	if (Spectrum_Filter(ks1) > 0.f && m1 > 0.f)
	{
		const float rough1 = m1 * m1;
		const float r1 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);
		result += (SchlickDistribution_D(rough1, H, 0.f) * SchlickDistribution_G(rough1, lightDir, eyeDir) / (4.f * coso)) * (ks1 * FresnelSchlick_Evaluate(TO_FLOAT3(r1), dot(eyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough1, H, 0.f);
		++n;
	}
	const float3 ks2Val = Texture_GetSpectrumValue(material->carpaint.Ks2TexIndex, hitPoint TEXTURES_PARAM);
	const float3 ks2 = Spectrum_Clamp(ks2Val);
	const float m2 = Texture_GetFloatValue(material->carpaint.M2TexIndex, hitPoint TEXTURES_PARAM);
	if (Spectrum_Filter(ks2) > 0.f && m2 > 0.f)
	{
		const float rough2 = m2 * m2;
		const float r2 = Texture_GetFloatValue(material->carpaint.R2TexIndex, hitPoint TEXTURES_PARAM);
		result += (SchlickDistribution_D(rough2, H, 0.f) * SchlickDistribution_G(rough2, lightDir, eyeDir) / (4.f * coso)) * (ks2 * FresnelSchlick_Evaluate(TO_FLOAT3(r2), dot(eyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough2, H, 0.f);
		++n;
	}
	const float3 ks3Val = Texture_GetSpectrumValue(material->carpaint.Ks3TexIndex, hitPoint TEXTURES_PARAM);
	const float3 ks3 = Spectrum_Clamp(ks3Val);
	const float m3 = Texture_GetFloatValue(material->carpaint.M3TexIndex, hitPoint TEXTURES_PARAM);
	if (Spectrum_Filter(ks3) > 0.f && m3 > 0.f)
	{
		const float rough3 = m3 * m3;
		const float r3 = Texture_GetFloatValue(material->carpaint.R3TexIndex, hitPoint TEXTURES_PARAM);
		result += (SchlickDistribution_D(rough3, H, 0.f) * SchlickDistribution_G(rough3, lightDir, eyeDir) / (4.f * coso)) * (ks3 * FresnelSchlick_Evaluate(TO_FLOAT3(r3), dot(eyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough3, H, 0.f);
		++n;
	}

	// Front face: coating+base
	const BSDFEvent event = GLOSSY | REFLECT;

	// Finish pdf computation
	pdf /= 4.f * fabs(dot(lightDir, H));
	const float directPdfW = (pdf + fabs(lightDir.z) * M_1_PI_F) / n;

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

OPENCL_FORCE_INLINE void CarPaintMaterial_Sample(__global const Material* restrict material,
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

	const float3 kaVal = Texture_GetSpectrumValue(material->carpaint.KaTexIndex, hitPoint TEXTURES_PARAM);
	const float d = Texture_GetFloatValue(material->carpaint.depthTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kdVal = Texture_GetSpectrumValue(material->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM);

	const float m1 = Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM);
	const float m2 = Texture_GetFloatValue(material->carpaint.M2TexIndex, hitPoint TEXTURES_PARAM);
	const float m3 = Texture_GetFloatValue(material->carpaint.M3TexIndex, hitPoint TEXTURES_PARAM);

	const float r1 = Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM);
	const float r2 = Texture_GetFloatValue(material->carpaint.R2TexIndex, hitPoint TEXTURES_PARAM);
	const float r3 = Texture_GetFloatValue(material->carpaint.R3TexIndex, hitPoint TEXTURES_PARAM);

	// Test presence of components
	int n = 1; // already count the diffuse layer
	int sampled = 0; // sampled layer
	float3 result = BLACK;
	float pdf = 0.f;
	bool l1 = false, l2 = false, l3 = false;
	// 1st glossy layer
	const float3 ks1Val = Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM);
	const float3 ks1 = Spectrum_Clamp(ks1Val);
	if (Spectrum_Filter(ks1) > 0.f && m1 > 0.f)
	{
		l1 = true;
		++n;
	}
	// 2nd glossy layer
	const float3 ks2Val = Texture_GetSpectrumValue(material->carpaint.Ks2TexIndex, hitPoint TEXTURES_PARAM);
	const float3 ks2 = Spectrum_Clamp(ks2Val);
	if (Spectrum_Filter(ks2) > 0.f && m2 > 0.f)
	{
		l2 = true;
		++n;
	}
	// 3rd glossy layer
	const float3 ks3Val = Texture_GetSpectrumValue(material->carpaint.Ks3TexIndex, hitPoint TEXTURES_PARAM);
	const float3 ks3 = Spectrum_Clamp(ks3Val);
	if (Spectrum_Filter(ks3) > 0.f && m3 > 0.f)
	{
		l3 = true;
		++n;
	}

	float3 sampledDir, wh;
	float cosWH;
	if (passThroughEvent < 1.f / n) {
		// Sample diffuse layer
		sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &pdf);

		if (fabs(CosTheta(sampledDir)) < DEFAULT_COS_EPSILON_STATIC) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		// Absorption
		const float cosi = fabs(fixedDir.z);
		const float coso = fabs(sampledDir.z);
		const float3 alpha = Spectrum_Clamp(kaVal);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Evaluate base BSDF
		result = absorption * Spectrum_Clamp(kdVal) * pdf;

		wh = normalize(sampledDir + fixedDir);
		if (wh.z < 0.f)
			wh = -wh;
		cosWH = fabs(dot(fixedDir, wh));
	} else if (passThroughEvent < 2.f / n && l1) {
		// Sample 1st glossy layer
		sampled = 1;
		const float rough1 = m1 * m1;
		float d;
		SchlickDistribution_SampleH(rough1, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = dot(fixedDir, wh);
		sampledDir = 2.f * cosWH * wh - fixedDir;
		cosWH = fabs(cosWH);

		if ((sampledDir.z < DEFAULT_COS_EPSILON_STATIC) ||
			(fixedDir.z * sampledDir.z < 0.f)) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		result = ks1 * FresnelSchlick_Evaluate(TO_FLOAT3(r1), cosWH);

		const float G = SchlickDistribution_G(rough1, fixedDir, sampledDir);
		result *= d * G / (4.f * fabs(fixedDir.z));
	} else if ((passThroughEvent < 2.f / n  ||
		(!l1 && passThroughEvent < 3.f / n)) && l2) {
		// Sample 2nd glossy layer
		sampled = 2;
		const float rough2 = m2 * m2;
		float d;
		SchlickDistribution_SampleH(rough2, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = dot(fixedDir, wh);
		sampledDir = 2.f * cosWH * wh - fixedDir;
		cosWH = fabs(cosWH);

		if ((sampledDir.z < DEFAULT_COS_EPSILON_STATIC) ||
			(fixedDir.z * sampledDir.z < 0.f)) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		result = ks2 * FresnelSchlick_Evaluate(TO_FLOAT3(r2), cosWH);

		const float G = SchlickDistribution_G(rough2, fixedDir, sampledDir);
		result *= d * G / (4.f * fabs(fixedDir.z));
	} else if (l3) {
		// Sample 3rd glossy layer
		sampled = 3;
		const float rough3 = m3 * m3;
		float d;
		SchlickDistribution_SampleH(rough3, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = dot(fixedDir, wh);
		sampledDir = 2.f * cosWH * wh - fixedDir;
		cosWH = fabs(cosWH);

		if ((sampledDir.z < DEFAULT_COS_EPSILON_STATIC) ||
			(fixedDir.z * sampledDir.z < 0.f)) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f) {
			MATERIAL_SAMPLE_RETURN_BLACK;
		}

		result = ks3 * FresnelSchlick_Evaluate(TO_FLOAT3(r3), cosWH);

		const float G = SchlickDistribution_G(rough3, fixedDir, sampledDir);
		result *= d * G / (4.f * fabs(fixedDir.z));
	} else {
		// Sampling issue
 		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	const BSDFEvent event = GLOSSY | REFLECT;

	// Add other components
	// Diffuse
	if (sampled != 0) {
		// Absorption
		const float cosi = fabs(fixedDir.z);
		const float coso = fabs(sampledDir.z);
		const float3 alpha = Spectrum_Clamp(kaVal);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		const float pdf0 = fabs(sampledDir.z) * M_1_PI_F;
		pdf += pdf0;
		result += absorption * Spectrum_Clamp(kdVal) * pdf0;
	}
	// 1st glossy
	if (l1 && sampled != 1) {
		const float rough1 = m1 * m1;
		const float d1 = SchlickDistribution_D(rough1, wh, 0.f);
		const float pdf1 = SchlickDistribution_Pdf(rough1, wh, 0.f) / (4.f * cosWH);
		if (pdf1 > 0.f) {
			result += ks1 * (d1 *
				SchlickDistribution_G(rough1, fixedDir, sampledDir) /
				(4.f * fabs(fixedDir.z))) *
				FresnelSchlick_Evaluate(TO_FLOAT3(r1), cosWH);
			pdf += pdf1;
		}
	}
	// 2nd glossy
	if (l2 && sampled != 2) {
		const float rough2 = m2 * m2;
		const float d2 = SchlickDistribution_D(rough2, wh, 0.f);
		const float pdf2 = SchlickDistribution_Pdf(rough2, wh, 0.f) / (4.f * cosWH);
		if (pdf2 > 0.f) {
			result += ks2 * (d2 *
				SchlickDistribution_G(rough2, fixedDir, sampledDir) /
				(4.f * fabs(fixedDir.z))) *
				FresnelSchlick_Evaluate(TO_FLOAT3(r2), cosWH);
			pdf += pdf2;
		}
	}
	// 3rd glossy
	if (l3 && sampled != 3) {
		const float rough3 = m3 * m3;
		const float d3 = SchlickDistribution_D(rough3, wh, 0.f);
		const float pdf3 = SchlickDistribution_Pdf(rough3, wh, 0.f) / (4.f * cosWH);
		if (pdf3 > 0.f) {
			result += ks3 * (d3 *
				SchlickDistribution_G(rough3, fixedDir, sampledDir) /
				(4.f * fabs(fixedDir.z))) *
				FresnelSchlick_Evaluate(TO_FLOAT3(r3), cosWH);
			pdf += pdf3;
		}
	}
	// Adjust pdf and result
	const float pdfW = pdf / n;
	result = result / pdfW;

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void CarPaintMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			CarPaintMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			CarPaintMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			CarPaintMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			CarPaintMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			CarPaintMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			CarPaintMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			CarPaintMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
