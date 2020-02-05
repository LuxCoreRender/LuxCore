#line 2 "materialdefs_funcs_mix.cl"

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
// Mix material
//
// One instance of this file for each Mix material is used in Compiled scene
// class after having expanded the following parameters.
//
// Preprocessing parameters:
//  <<CS_MIX_MATERIAL_INDEX>>
//  <<CS_MAT_A_MATERIAL_INDEX>>
//  <<CS_MAT_A_PREFIX>>
//  <<CS_MAT_A_POSTFIX>>
//  <<CS_MAT_B_MATERIAL_INDEX>>
//  <<CS_MAT_B_PREFIX>>
//  <<CS_MAT_B_POSTFIX>>
//  <<CS_FACTOR_TEXTURE>>
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_GetPassThroughTransparency(__global const Material *material,
		__global const HitPoint *hitPoint, const float3 localFixedDir,
		const float passThroughEvent, const bool backTracing
		MATERIALS_PARAM_DECL) {
	const uint transpTexIndex = (hitPoint->intoObject != backTracing) ?
		material->frontTranspTexIndex : material->backTranspTexIndex;

	if (transpTexIndex != NULL_INDEX) {
		return DefaultMaterial_GetPassThroughTransparency(material, hitPoint, localFixedDir, passThroughEvent, backTracing
			TEXTURES_PARAM);
	} else {
		const float factor = <<CS_FACTOR_TEXTURE>>;
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (passThroughEvent < weight1) {
			return <<CS_MAT_A_PREFIX>>_GetPassThroughTransparency<<CS_MAT_A_POSTFIX>>(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
				hitPoint, localFixedDir, passThroughEvent / weight1, backTracing MATERIALS_PARAM);
		} else {
			return <<CS_MAT_B_PREFIX>>_GetPassThroughTransparency<<CS_MAT_B_POSTFIX>>(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
				hitPoint, localFixedDir, (passThroughEvent - weight1) / weight2, backTracing MATERIALS_PARAM);
		}
	}
}

OPENCL_FORCE_NOT_INLINE float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_Evaluate(__global const Material *material,
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
/*#if defined(PARAM_HAS_BUMPMAPS)
	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
	Frame frame;
	Frame_Set_Private(&frame, dpdu, dpdv, shadeN);
#endif*/

	float3 result = BLACK;
	
	// This test is usually done by BSDF_Evaluate() and must be repeated in
	// material referencing other materials
	const float isTransmitEval = (signbit(lightDir.z) != signbit(eyeDir.z));

	const float factor = <<CS_FACTOR_TEXTURE>>;
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (directPdfW)
		*directPdfW = 0.f;

	//--------------------------------------------------------------------------
	// Evaluate material A
	//--------------------------------------------------------------------------

	BSDFEvent eventMatA = Material_GetEventTypes(<<CS_MAT_A_MATERIAL_INDEX>>
			MATERIALS_PARAM);
	if ((weight1 > 0.f) &&
			((!isTransmitEval && (eventMatA & REFLECT)) ||
			(isTransmitEval && (eventMatA & TRANSMIT)))) {
		const float3 lightDirA = lightDir;
		const float3 eyeDirA = eyeDir;
		float directPdfWMatA;

		const float3 matAResult = <<CS_MAT_A_PREFIX>>_Evaluate<<CS_MAT_A_POSTFIX>>(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
				hitPoint, lightDirA, eyeDirA, &eventMatA, &directPdfWMatA
				MATERIALS_PARAM);

		if (!Spectrum_IsBlack(matAResult)) {
			result += weight1 * matAResult;
			if (directPdfW)
				*directPdfW += weight1 * directPdfWMatA;
		}
	}
	
	//--------------------------------------------------------------------------
	// Evaluate material B
	//--------------------------------------------------------------------------
	
	BSDFEvent eventMatB = Material_GetEventTypes(<<CS_MAT_B_MATERIAL_INDEX>>
			MATERIALS_PARAM);
	if ((weight2 > 0.f) &&
			((!isTransmitEval && (eventMatB & REFLECT)) ||
			(isTransmitEval && (eventMatB & TRANSMIT)))) {
		const float3 lightDirB = lightDir;
		const float3 eyeDirB = eyeDir;
		float directPdfWMatB;

		const float3 matBResult = <<CS_MAT_B_PREFIX>>_Evaluate<<CS_MAT_B_POSTFIX>>(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
				hitPoint, lightDirB, eyeDirB, &eventMatB, &directPdfWMatB
				MATERIALS_PARAM);

		if (!Spectrum_IsBlack(matBResult)) {
			result += weight2 * matBResult;
			if (directPdfW)
				*directPdfW += weight2 * directPdfWMatB;
		}
	}

	*event = eventMatA | eventMatB;

	return result;
}

OPENCL_FORCE_NOT_INLINE float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_Sample(__global const Material *material,
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir, const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	const float factor = <<CS_FACTOR_TEXTURE>>;
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const bool sampleMatA = (passThroughEvent < weight1);
	const float weightFirst = sampleMatA ? weight1 : weight2;
	const float weightSecond = sampleMatA ? weight2 : weight1;

	const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

	__global const Material *matA = &mats[<<CS_MAT_A_MATERIAL_INDEX>>];
	__global const Material *matB = &mats[<<CS_MAT_B_MATERIAL_INDEX>>];

	const float3 fixedDirFirst = fixedDir;

	float3 result = sampleMatA ?
			<<CS_MAT_A_PREFIX>>_Sample<<CS_MAT_A_POSTFIX>>(matA, hitPoint, fixedDirFirst, sampledDir,
				u0, u1,
				passThroughEventFirst,
				pdfW, event MATERIALS_PARAM):
			<<CS_MAT_B_PREFIX>>_Sample<<CS_MAT_B_POSTFIX>>(matB, hitPoint, fixedDirFirst, sampledDir,
				u0, u1,
				passThroughEventFirst,
				pdfW, event MATERIALS_PARAM);

	if (Spectrum_IsBlack(result))
		return BLACK;

	*pdfW *= weightFirst;

	if ((*event) & SPECULAR)
		return result;
	
	result *= *pdfW;

	const float3 fixedDirSecond = fixedDir;
	const float3 sampledDirSecond = *sampledDir;

	// This test is usually done by BSDF_Evaluate() and must be repeated in
	// material referencing other materials
	const float isTransmitEval = (signbit(fixedDirSecond.z) != signbit(sampledDirSecond.z));

	BSDFEvent eventSecond =  sampleMatA ?
		Material_GetEventTypes(<<CS_MAT_B_MATERIAL_INDEX>>
			MATERIALS_PARAM) :
		Material_GetEventTypes(<<CS_MAT_A_MATERIAL_INDEX>>
			MATERIALS_PARAM);

	if ((!isTransmitEval && (eventSecond & REFLECT)) ||
			(isTransmitEval && (eventSecond & TRANSMIT))) {
		float pdfWSecond;
		const float3 evalSecond = sampleMatA ?
				<<CS_MAT_B_PREFIX>>_Evaluate<<CS_MAT_B_POSTFIX>>(matB, hitPoint,
						sampledDirSecond, fixedDirSecond, &eventSecond, &pdfWSecond
						MATERIALS_PARAM) :
				<<CS_MAT_A_PREFIX>>_Evaluate<<CS_MAT_A_POSTFIX>>(matA, hitPoint,
						sampledDirSecond, fixedDirSecond, &eventSecond, &pdfWSecond
						MATERIALS_PARAM);
		if (!Spectrum_IsBlack(evalSecond)) {
			result += weightSecond * evalSecond;
			*pdfW += weightSecond * pdfWSecond;
		}
	}

	return result / *pdfW;
}

OPENCL_FORCE_NOT_INLINE float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_GetEmittedRadiance(__global const Material *material,
		__global const HitPoint *hitPoint, const float oneOverPrimitiveArea
		MATERIALS_PARAM_DECL) {
	if (material->emitTexIndex != NULL_INDEX)
		return Material_GetEmittedRadianceWithoutDynamic(material, hitPoint, oneOverPrimitiveArea MATERIALS_PARAM);
	else {
		float3 result = BLACK;
		const float factor = <<CS_FACTOR_TEXTURE>>;
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (weight1 > 0.f)
		   result += weight1 * <<CS_MAT_A_PREFIX>>_GetEmittedRadiance<<CS_MAT_A_POSTFIX>>(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
				   hitPoint, oneOverPrimitiveArea
				   MATERIALS_PARAM);
		if (weight2 > 0.f)
		   result += weight2 * <<CS_MAT_B_PREFIX>>_GetEmittedRadiance<<CS_MAT_B_POSTFIX>>(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
				   hitPoint, oneOverPrimitiveArea
				   MATERIALS_PARAM);

		return result;
	}
}
