#line 2 "materialdefs_funcs_velvet.cl"

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
// Velvet material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void VelvetMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->velvet.kdTexIndex,
			hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void VelvetMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void VelvetMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void VelvetMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void VelvetMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void VelvetMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	const float directPdfW = fabs(lightDir.z * M_1_PI_F);

	const BSDFEvent event = DIFFUSE | REFLECT;

	const float3 kdVal = Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kd = Spectrum_Clamp(kdVal);
	const float cosv = -dot(lightDir, eyeDir);

	// Compute phase function

	const float B = 3.0f * cosv;

	const float A1 = Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint TEXTURES_PARAM);
	const float A2 = Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint TEXTURES_PARAM);
	const float A3 = Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint TEXTURES_PARAM);

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI_F);
 
	const float delta = Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint TEXTURES_PARAM);
	p = (p * delta) / fabs(eyeDir.z);

	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;

	const float3 result = p * kd;

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

OPENCL_FORCE_INLINE void VelvetMaterial_Sample(__global const Material* restrict material,
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
	const float3 sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &pdfW);
	if (fabs(CosTheta(sampledDir)) < DEFAULT_COS_EPSILON_STATIC) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	const BSDFEvent event = DIFFUSE | REFLECT;

	const float3 kdVal = Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint TEXTURES_PARAM);
	const float3 kd = Spectrum_Clamp(kdVal);
	const float cosv = dot(-fixedDir, sampledDir);

	// Compute phase function

	const float B = 3.0f * cosv;

	const float A1 = Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint TEXTURES_PARAM);
	const float A2 = Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint TEXTURES_PARAM);
	const float A3 = Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint TEXTURES_PARAM);

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI_F);
 
	const float delta = Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint TEXTURES_PARAM);
	p = (p * delta) / fabs(fixedDir.z);

	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;

	const float3 result = kd * (p / pdfW);

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void VelvetMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			VelvetMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			VelvetMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			VelvetMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			VelvetMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			VelvetMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			VelvetMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			VelvetMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
