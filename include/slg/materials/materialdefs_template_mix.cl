#line 2 "materialdefs_funcs_mix.cl"

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
// Mix material
//
// One instance of this file for each Mix material is used in Compiled scene
// class after having expanded the following parameters.
//
// Preprocessing parameters:
//  <<CS_MIX_MATERIAL_INDEX>>
//  <<CS_MAT_A_MATERIAL_INDEX>>
//  <<CS_MAT_B_MATERIAL_INDEX>>
//  <<CS_FACTOR_TEXTURE_INDEX>>
//------------------------------------------------------------------------------

BSDFEvent Material_Index<<CS_MIX_MATERIAL_INDEX>>_GetEventTypes(__global const Material *material
		MATERIALS_PARAM_DECL) {
	return
			Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_GetEventTypes(&mats[<<CS_MAT_A_MATERIAL_INDEX>>]
				MATERIALS_PARAM) |
			Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_GetEventTypes(&mats[<<CS_MAT_B_MATERIAL_INDEX>>]
				MATERIALS_PARAM);
}

bool Material_Index<<CS_MIX_MATERIAL_INDEX>>_IsDelta(__global const Material *material
		MATERIALS_PARAM_DECL) {
	return
			Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_IsDelta(&mats[<<CS_MAT_A_MATERIAL_INDEX>>]
				MATERIALS_PARAM) &&
			Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_IsDelta(&mats[<<CS_MAT_B_MATERIAL_INDEX>>]
				MATERIALS_PARAM);
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_GetPassThroughTransparency(__global const Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	const float factor = Texture_Index<<CS_FACTOR_TEXTURE_INDEX>>_EvaluateFloat(
			&texs[<<CS_FACTOR_TEXTURE_INDEX>>],
			hitPoint
			TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (passThroughEvent < weight1)
		return Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_GetPassThroughTransparency(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
			hitPoint, localFixedDir, passThroughEvent / weight1 MATERIALS_PARAM);
	else
		return Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_GetPassThroughTransparency(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
			hitPoint, localFixedDir, (passThroughEvent - weight1) / weight2 MATERIALS_PARAM);
}
#endif

float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_Evaluate(__global const Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
#if defined(PARAM_HAS_BUMPMAPS)
	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
	Frame frame;
	ExtMesh_GetFrame_Private(shadeN, dpdu, dpdv, &frame);
#endif

	float3 result = BLACK;
	const float factor = Texture_Index<<CS_FACTOR_TEXTURE_INDEX>>_EvaluateFloat(
			&texs[<<CS_FACTOR_TEXTURE_INDEX>>],
			hitPoint
			TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (directPdfW)
		*directPdfW = 0.f;

	//--------------------------------------------------------------------------
	// Evaluate material A
	//--------------------------------------------------------------------------

	BSDFEvent eventMatA = NONE;
	if (weight1 > 0.f) {
#if defined(PARAM_HAS_BUMPMAPS)
		Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_Bump(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
				hitPoint, 1.f
				MATERIALS_PARAM);

		const float3 shadeNA = VLOAD3F(&hitPoint->shadeN.x);
		const float3 dpduA = VLOAD3F(&hitPoint->dpdu.x);
		const float3 dpdvA = VLOAD3F(&hitPoint->dpdv.x);

		Frame frameA;
		ExtMesh_GetFrame_Private(shadeNA, dpduA, dpdvA, &frameA);

		const float3 lightDirA = Frame_ToLocal_Private(&frameA, Frame_ToWorld_Private(&frame, lightDir));
		const float3 eyeDirA = Frame_ToLocal_Private(&frameA, Frame_ToWorld_Private(&frame, eyeDir));
#else
		const float3 lightDirA = lightDir;
		const float3 eyeDirA = eyeDir;
#endif
		float directPdfWMatA;
		const float3 matAResult = Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_Evaluate(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
				hitPoint, lightDirA, eyeDirA, &eventMatA, &directPdfWMatA
				MATERIALS_PARAM);
		if (!Spectrum_IsBlack(matAResult)) {
			result += weight1 * matAResult;
			if (directPdfW)
				*directPdfW += weight1 * directPdfWMatA;
		}

#if defined(PARAM_HAS_BUMPMAPS)
		VSTORE3F(shadeN, &hitPoint->shadeN.x);
		VSTORE3F(dpdu, &hitPoint->dpdu.x);
		VSTORE3F(dpdv, &hitPoint->dpdv.x);
#endif
	}
	
	//--------------------------------------------------------------------------
	// Evaluate material B
	//--------------------------------------------------------------------------
	
	BSDFEvent eventMatB = NONE;
	if (weight2 > 0.f) {
#if defined(PARAM_HAS_BUMPMAPS)
		Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_Bump(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
				hitPoint, 1.f
				MATERIALS_PARAM);

		const float3 shadeNB = VLOAD3F(&hitPoint->shadeN.x);
		const float3 dpduB = VLOAD3F(&hitPoint->dpdu.x);
		const float3 dpdvB = VLOAD3F(&hitPoint->dpdv.x);

		Frame frameB;
		ExtMesh_GetFrame_Private(shadeNB, dpduB, dpdvB, &frameB);

		const float3 lightDirB = Frame_ToLocal_Private(&frameB, Frame_ToWorld_Private(&frame, lightDir));
		const float3 eyeDirB = Frame_ToLocal_Private(&frameB, Frame_ToWorld_Private(&frame, eyeDir));
#else
		const float3 lightDirB = lightDir;
		const float3 eyeDirB = eyeDir;
#endif
		float directPdfWMatB;
		const float3 matBResult = Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_Evaluate(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
				hitPoint, lightDirB, eyeDirB, &eventMatB, &directPdfWMatB
				MATERIALS_PARAM);
		if (!Spectrum_IsBlack(matBResult)) {
			result += weight2 * matBResult;
			if (directPdfW)
				*directPdfW += weight2 * directPdfWMatB;
		}
#if defined(PARAM_HAS_BUMPMAPS)
		VSTORE3F(shadeN, &hitPoint->shadeN.x);
		VSTORE3F(dpdu, &hitPoint->dpdu.x);
		VSTORE3F(dpdv, &hitPoint->dpdv.x);
#endif
	}

	*event = eventMatA | eventMatB;

	return result;
}

float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_Sample(__global const Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir, const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event, const BSDFEvent requestedEvent
		MATERIALS_PARAM_DECL) {
	const float factor = Texture_Index<<CS_FACTOR_TEXTURE_INDEX>>_EvaluateFloat(
			&texs[<<CS_FACTOR_TEXTURE_INDEX>>],
			hitPoint
			TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const bool sampleMatA = (passThroughEvent < weight1);
	const float weightFirst = sampleMatA ? weight1 : weight2;
	const float weightSecond = sampleMatA ? weight2 : weight1;

	const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

	__global const Material *matA = &mats[<<CS_MAT_A_MATERIAL_INDEX>>];
	__global const Material *matB = &mats[<<CS_MAT_B_MATERIAL_INDEX>>];

#if defined(PARAM_HAS_BUMPMAPS)
	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);

	Frame frame;
	ExtMesh_GetFrame_Private(shadeN, dpdu, dpdv, &frame);

	Frame frameFirst;
	if (sampleMatA) {
		Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_Bump(matA, hitPoint, 1.f
				MATERIALS_PARAM);

		const float3 shadeNA = VLOAD3F(&hitPoint->shadeN.x);
		const float3 dpduA = VLOAD3F(&hitPoint->dpdu.x);
		const float3 dpdvA = VLOAD3F(&hitPoint->dpdv.x);
		ExtMesh_GetFrame_Private(shadeNA, dpduA, dpdvA, &frameFirst);
	} else {

		Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_Bump(matB, hitPoint, 1.f
				MATERIALS_PARAM);
		const float3 shadeNB = VLOAD3F(&hitPoint->shadeN.x);
		const float3 dpduB = VLOAD3F(&hitPoint->dpdu.x);
		const float3 dpdvB = VLOAD3F(&hitPoint->dpdv.x);
		ExtMesh_GetFrame_Private(shadeNB, dpduB, dpdvB, &frameFirst);
	}

	const float3 fixedDirFirst = Frame_ToLocal_Private(&frameFirst, Frame_ToWorld_Private(&frame, fixedDir));
#else
	const float3 fixedDirFirst = fixedDir;
#endif

	float3 result = sampleMatA ?
			Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_Sample(matA, hitPoint, fixedDirFirst, sampledDir,
				u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
				passThroughEventFirst,
#endif
				pdfW, cosSampledDir, event, requestedEvent MATERIALS_PARAM):
			Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_Sample(matB, hitPoint, fixedDirFirst, sampledDir,
				u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
				passThroughEventFirst,
#endif
				pdfW, cosSampledDir, event, requestedEvent MATERIALS_PARAM);

#if defined(PARAM_HAS_BUMPMAPS)
	VSTORE3F(shadeN, &hitPoint->shadeN.x);
	VSTORE3F(dpdu, &hitPoint->dpdu.x);
	VSTORE3F(dpdv, &hitPoint->dpdv.x);
#endif

	if (Spectrum_IsBlack(result))
		return BLACK;

	*pdfW *= weightFirst;
	result *= *pdfW;

	BSDFEvent eventSecond;
	float pdfWSecond;
#if defined(PARAM_HAS_BUMPMAPS)
	Frame frameSecond;
	if (sampleMatA) {
		Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_Bump(matB, hitPoint, 1.f
				MATERIALS_PARAM);

		const float3 shadeNB = VLOAD3F(&hitPoint->shadeN.x);
		const float3 dpduB = VLOAD3F(&hitPoint->dpdu.x);
		const float3 dpdvB = VLOAD3F(&hitPoint->dpdv.x);
		ExtMesh_GetFrame_Private(shadeNB, dpduB, dpdvB, &frameSecond);
	} else {
		Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_Bump(matA, hitPoint, 1.f
				MATERIALS_PARAM);

		const float3 shadeNA = VLOAD3F(&hitPoint->shadeN.x);
		const float3 dpduA = VLOAD3F(&hitPoint->dpdu.x);
		const float3 dpdvA = VLOAD3F(&hitPoint->dpdv.x);
		ExtMesh_GetFrame_Private(shadeNA, dpduA, dpdvA, &frameSecond);
	}

	const float3 fixedDirSecond = Frame_ToLocal_Private(&frameSecond, Frame_ToWorld_Private(&frame, fixedDir));
	*sampledDir = Frame_ToWorld_Private(&frameFirst, *sampledDir);
	const float3 sampledDirSecond = Frame_ToLocal_Private(&frameSecond, *sampledDir);
	*sampledDir = Frame_ToLocal_Private(&frame, *sampledDir);
#else
	const float3 fixedDirSecond = fixedDir;
	const float3 sampledDirSecond = *sampledDir;
#endif

	float3 evalSecond = sampleMatA ?
			Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_Evaluate(matB, hitPoint,
					sampledDirSecond, fixedDirSecond, &eventSecond, &pdfWSecond
					MATERIALS_PARAM) :
			Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_Evaluate(matA, hitPoint,
					sampledDirSecond, fixedDirSecond, &eventSecond, &pdfWSecond
					MATERIALS_PARAM);
	if (!Spectrum_IsBlack(evalSecond)) {
		result += weightSecond * evalSecond;
		*pdfW += weightSecond * pdfWSecond;
	}

#if defined(PARAM_HAS_BUMPMAPS)
	VSTORE3F(shadeN, &hitPoint->shadeN.x);
	VSTORE3F(dpdu, &hitPoint->dpdu.x);
	VSTORE3F(dpdv, &hitPoint->dpdv.x);
#endif

	return result / *pdfW;
}

float3 Material_Index<<CS_MIX_MATERIAL_INDEX>>_GetEmittedRadiance(__global const Material *material,
		__global HitPoint *hitPoint, const float oneOverPrimitiveArea
		MATERIALS_PARAM_DECL) {
	if (material->emitTexIndex != NULL_INDEX)
		return Material_GetEmittedRadianceNoMix(material, hitPoint TEXTURES_PARAM);
	else {
		float3 result = BLACK;
		const float factor = Texture_Index<<CS_FACTOR_TEXTURE_INDEX>>_EvaluateFloat(
			&texs[<<CS_FACTOR_TEXTURE_INDEX>>],
			hitPoint
			TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (weight1 > 0.f)
		   result += weight1 * Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_GetEmittedRadiance(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
				   hitPoint, oneOverPrimitiveArea
				   MATERIALS_PARAM);
		if (weight2 > 0.f)
		   result += weight2 * Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_GetEmittedRadiance(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
				   hitPoint, oneOverPrimitiveArea
				   MATERIALS_PARAM);

		return result;
	}
}

#if defined(PARAM_HAS_VOLUMES)
uint Material_Index<<CS_MIX_MATERIAL_INDEX>>_GetInteriorVolume(__global const Material *material,
		__global HitPoint *hitPoint, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
		if (material->interiorVolumeIndex != NULL_INDEX)
			return material->interiorVolumeIndex;

		const float factor = Texture_Index<<CS_FACTOR_TEXTURE_INDEX>>_EvaluateFloat(
			&texs[<<CS_FACTOR_TEXTURE_INDEX>>],
			hitPoint
			TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;
		if (passThroughEvent < weight1)
			return Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_GetInteriorVolume(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
					hitPoint, passThroughEvent / weight1
					MATERIALS_PARAM);
		else
			return Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_GetInteriorVolume(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
					hitPoint, (passThroughEvent - weight1) / weight2
					MATERIALS_PARAM);
}

uint Material_Index<<CS_MIX_MATERIAL_INDEX>>_GetExteriorVolume(__global const Material *material,
		__global HitPoint *hitPoint, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
		if (material->exteriorVolumeIndex != NULL_INDEX)
			return material->exteriorVolumeIndex;

		const float factor = Texture_Index<<CS_FACTOR_TEXTURE_INDEX>>_EvaluateFloat(
			&texs[<<CS_FACTOR_TEXTURE_INDEX>>],
			hitPoint
			TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (passThroughEvent < weight1)
			return Material_Index<<CS_MAT_A_MATERIAL_INDEX>>_GetExteriorVolume(&mats[<<CS_MAT_A_MATERIAL_INDEX>>],
					hitPoint, passThroughEvent / weight1
					MATERIALS_PARAM);
		else
			return Material_Index<<CS_MAT_B_MATERIAL_INDEX>>_GetExteriorVolume(&mats[<<CS_MAT_B_MATERIAL_INDEX>>],
					hitPoint, (passThroughEvent - weight1) / weight2
					MATERIALS_PARAM);
}
#endif
