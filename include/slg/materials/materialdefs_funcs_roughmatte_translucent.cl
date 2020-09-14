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

OPENCL_FORCE_INLINE void RoughMatteTranslucentMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const float3 r = Spectrum_Clamp(Texture_GetSpectrumValue(material->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM));
	const float3 t = Spectrum_Clamp(Texture_GetSpectrumValue(material->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM)) * 
		// Energy conservation
		(1.f - r);

    const float3 albedo = r + t;

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void RoughMatteTranslucentMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughMatteTranslucentMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughMatteTranslucentMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughMatteTranslucentMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void RoughMatteTranslucentMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	const float3 krVal = Texture_GetSpectrumValue(material->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM);
	const float3 ktVal = Texture_GetSpectrumValue(material->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM);
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
			MATERIAL_EVALUATE_RETURN_BLACK;
		}
	}

	const bool relfected = (CosTheta(lightDir) * CosTheta(eyeDir) > 0.f);
	const float weight = (lightDir.z * eyeDir.z > 0.f) ? threshold : (1.f - threshold);

	const float directPdfW = weight * fabs(lightDir.z * M_1_PI_F);

	const float sigma = Texture_GetFloatValue(material->roughmatteTranslucent.sigmaTexIndex, hitPoint TEXTURES_PARAM);
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

	float3 result = TO_FLOAT3(M_1_PI_F * fabs(lightDir.z) *
		(A + B * maxcos * sinthetai * sinthetao / fmax(fabs(CosTheta(lightDir)), fabs(CosTheta(eyeDir)))));

	BSDFEvent event;
	if (lightDir.z * eyeDir.z > 0.f) {
		event = DIFFUSE | REFLECT;
		result *= r;
	} else {
		event = DIFFUSE | TRANSMIT;
		result *= t;
	}

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

OPENCL_FORCE_INLINE void RoughMatteTranslucentMaterial_Sample(__global const Material* restrict material,
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

	float pdfW;
	float3 sampledDir = CosineSampleHemisphereWithPdf(u0, u1, &pdfW);
	if (fabs(CosTheta(sampledDir)) < DEFAULT_COS_EPSILON_STATIC) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	const float3 krVal = Texture_GetSpectrumValue(material->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM);
	const float3 ktVal = Texture_GetSpectrumValue(material->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kr = Spectrum_Clamp(krVal);
	const float3 kt = Spectrum_Clamp(ktVal) * 
		// Energy conservation
		(1.f - kr);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

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
			MATERIAL_SAMPLE_RETURN_BLACK;
		}
	}

	const float sigma = Texture_GetFloatValue(material->roughmatteTranslucent.sigmaTexIndex, hitPoint TEXTURES_PARAM);
	const float sigma2 = sigma * sigma;
	const float A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
	const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	const float sinthetai = SinTheta(fixedDir);
	const float sinthetao = SinTheta(sampledDir);
	float maxcos = 0.f;
	if (sinthetai > 1e-4f && sinthetao > 1e-4f) {
		const float dcos = CosPhi(sampledDir) * CosPhi(fixedDir) +
			SinPhi(sampledDir) * SinPhi(fixedDir);
		maxcos = max(0.f, dcos);
	}
	const float coef = (A + B * maxcos * sinthetai * sinthetao / max(fabs(CosTheta(sampledDir)), fabs(CosTheta(fixedDir))));

	BSDFEvent event;
	float3 result;
	if (passThroughEvent < threshold) {
		sampledDir *= (signbit(fixedDir.z) ? -1.f : 1.f);
		event = DIFFUSE | REFLECT;
		pdfW *= threshold;

		result = kr * (coef / threshold);
	} else {
		sampledDir *= -(signbit(fixedDir.z) ? -1.f : 1.f);
		event = DIFFUSE | TRANSMIT;
		pdfW *= (1.f - threshold);

		result = kt * (coef / (1.f - threshold));
	}

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void RoughMatteTranslucentMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			RoughMatteTranslucentMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			RoughMatteTranslucentMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			RoughMatteTranslucentMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			RoughMatteTranslucentMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			RoughMatteTranslucentMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			RoughMatteTranslucentMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			RoughMatteTranslucentMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
