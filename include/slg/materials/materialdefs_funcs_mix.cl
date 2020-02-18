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

OPENCL_FORCE_INLINE void MixMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 albedo1, albedo2;
	EvalStack_PopFloat3(albedo2);
	EvalStack_PopFloat3(albedo1);

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float3 albedo =  weight1 * albedo1 + weight2 * albedo2;

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void MixMaterial_GetVolumeSetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float passThroughEvent1 = passThroughEvent / weight1;

	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushFloat(factor);
	EvalStack_PushFloat(passThroughEvent1);
}

OPENCL_FORCE_INLINE void MixMaterial_GetVolumeSetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	uint volIndex1;
	EvalStack_PopUInt(volIndex1);

	float factor;
	EvalStack_PopFloat(factor);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);

	const float passThroughEvent2 = (passThroughEvent - weight1) / weight2;

	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushFloat(factor);
	EvalStack_PushUInt(volIndex1);
	EvalStack_PushFloat(passThroughEvent2);
}

OPENCL_FORCE_INLINE void MixMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const uint volIndex = material->interiorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	} else {
		uint volIndex1, volIndex2;
		EvalStack_PopUInt(volIndex2);
		EvalStack_PopUInt(volIndex1);

		float factor;
		EvalStack_PopFloat(factor);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		const uint volIndex = (passThroughEvent < weight1) ? volIndex1 : volIndex2;
		EvalStack_PushUInt(volIndex);
	}
}

OPENCL_FORCE_INLINE void MixMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const uint volIndex = material->exteriorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	} else {
		uint volIndex1, volIndex2;
		EvalStack_PopUInt(volIndex2);
		EvalStack_PopUInt(volIndex1);

		float factor;
		EvalStack_PopFloat(factor);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		const uint volIndex = (passThroughEvent < weight1) ? volIndex1 : volIndex2;
		EvalStack_PushUInt(volIndex);
	}
}

OPENCL_FORCE_INLINE void MixMaterial_GetPassThroughTransparencySetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	bool backTracing;
	EvalStack_PopUInt(backTracing);
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	float3 localFixedDir;
	EvalStack_PopFloat3(localFixedDir);

	// Save the parameters
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushUInt(backTracing);

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float passThroughEvent1 = passThroughEvent / weight1;

	EvalStack_PushFloat(factor);

	// To setup the following EVAL_GET_PASS_TROUGH_TRANSPARENCY evaluation of first sub-nodes
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent1);
	EvalStack_PushUInt(backTracing);
}

OPENCL_FORCE_INLINE void MixMaterial_GetPassThroughTransparencySetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 transp1;
	EvalStack_PopFloat3(transp1);

	float factor;
	EvalStack_PopFloat(factor);

	bool backTracing;
	EvalStack_PopUInt(backTracing);
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	float3 localFixedDir;
	EvalStack_PopFloat3(localFixedDir);

	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float passThroughEvent2 = (passThroughEvent - weight1) / weight2;

	// Save the parameters
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushUInt(backTracing);

	EvalStack_PushFloat(factor);

	EvalStack_PushFloat3(transp1);

	// To setup the following EVAL_GET_PASS_TROUGH_TRANSPARENCY evaluation of second sub-nodes
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent2);
	EvalStack_PushUInt(backTracing);
}

OPENCL_FORCE_INLINE void MixMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 transp1, transp2;
	EvalStack_PopFloat3(transp2);
	EvalStack_PopFloat3(transp1);

	float factor;
	EvalStack_PopFloat(factor);

	bool backTracing;
	EvalStack_PopUInt(backTracing);
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	float3 localFixedDir;
	EvalStack_PopFloat3(localFixedDir);

	const uint transpTexIndex = (hitPoint->intoObject != backTracing) ?
		material->frontTranspTexIndex : material->backTranspTexIndex;

	float3 transp;
	if (transpTexIndex != NULL_INDEX) {
		const float weight = clamp(
				Texture_GetFloatValue(transpTexIndex, hitPoint TEXTURES_PARAM),
				0.f, 1.f);

		transp = (passThroughEvent > weight) ? WHITE : BLACK;
	} else {
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		transp = (passThroughEvent < weight1) ? transp1 : transp2;
	}

	EvalStack_PushFloat3(transp);
}

OPENCL_FORCE_INLINE void MixMaterial_GetEmittedRadianceSetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float oneOverPrimitiveArea;
	EvalStack_PopFloat(oneOverPrimitiveArea);

	EvalStack_PushFloat(oneOverPrimitiveArea);
	// To setup the following EVAL_GET_EMITTED_RADIANCE evaluation of first sub-nodes
	EvalStack_PushFloat(oneOverPrimitiveArea);
}

OPENCL_FORCE_INLINE void MixMaterial_GetEmittedRadianceSetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 emit1;
	EvalStack_PopFloat3(emit1);

	float oneOverPrimitiveArea;
	EvalStack_PopFloat(oneOverPrimitiveArea);

	EvalStack_PushFloat(oneOverPrimitiveArea);
	EvalStack_PushFloat3(emit1);
	// To setup the following EVAL_GET_EMITTED_RADIANCE evaluation of second sub-nodes
	EvalStack_PushFloat(oneOverPrimitiveArea);
}

OPENCL_FORCE_INLINE void MixMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 emittedRadiance;
	if (material->emitTexIndex != NULL_INDEX)
		 DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
	else {
		float3 emit1, emit2;
		EvalStack_PopFloat3(emit2);
		EvalStack_PopFloat3(emit1);

		float oneOverPrimitiveArea;
		EvalStack_PopFloat(oneOverPrimitiveArea);

		const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		emittedRadiance = BLACK;
		if (weight1 > 0.f)
			emittedRadiance += weight1 * emit1;
		 if (weight2 > 0.f)
			emittedRadiance += weight2 * emit2;

		EvalStack_PushFloat3(emittedRadiance);
	}
}

//------------------------------------------------------------------------------
// Material evaluate
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void MixMaterial_EvaluateSetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	// Save the parameters
	EvalStack_PushFloat3(lightDir);
	EvalStack_PushFloat3(eyeDir);

	// To setup the following EVAL_EVALUATE evaluation of first sub-nodes
	EvalStack_PushFloat3(lightDir);
	EvalStack_PushFloat3(eyeDir);
}

OPENCL_FORCE_NOT_INLINE void MixMaterial_EvaluateSetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Pop the result of the first evaluation
	float directPdfWMatA;
	BSDFEvent eventMatA;
	float3 resultMatA;
	EvalStack_PopFloat(directPdfWMatA);
	EvalStack_PopBSDFEvent(eventMatA);
	EvalStack_PopFloat3(resultMatA);

	// Pop the saved parameters
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	// Save the parameters
	EvalStack_PushFloat3(eyeDir);
	EvalStack_PushFloat3(lightDir);

	// Save the result of the first evaluation
	EvalStack_PushFloat3(resultMatA);
	EvalStack_PushBSDFEvent(eventMatA);
	EvalStack_PushFloat(directPdfWMatA);

	// To setup the following EVAL_EVALUATE evaluation of second sub-nodes
	EvalStack_PushFloat3(lightDir);
	EvalStack_PushFloat3(eyeDir);
}

OPENCL_FORCE_NOT_INLINE void MixMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Pop the result of the second evaluation
	float directPdfWMatB;
	BSDFEvent eventMatB;
	float3 resultMatB;
	EvalStack_PopFloat(directPdfWMatB);
	EvalStack_PopBSDFEvent(eventMatB);
	EvalStack_PopFloat3(resultMatB);

	// Pop the result of the first evaluation
	float directPdfWMatA;
	BSDFEvent eventMatA;
	float3 resultMatA;
	EvalStack_PopFloat(directPdfWMatA);
	EvalStack_PopBSDFEvent(eventMatA);
	EvalStack_PopFloat3(resultMatA);

	// Pop the saved parameters
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	float3 result = BLACK;
	float directPdfW = 0.f;
	
	// This test is usually done by BSDF_Evaluate() and must be repeated in
	// material referencing other materials
	const float isTransmitEval = (signbit(lightDir.z) != signbit(eyeDir.z));

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	//--------------------------------------------------------------------------
	// Evaluate material A
	//--------------------------------------------------------------------------

	const BSDFEvent eventTypesMatA = mats[material->mix.matAIndex].eventTypes;
	if ((weight1 > 0.f) &&
			((!isTransmitEval && (eventTypesMatA & REFLECT)) ||
			(isTransmitEval && (eventTypesMatA & TRANSMIT))) &&
			!Spectrum_IsBlack(resultMatA)) {
		result += weight1 * resultMatA;
		directPdfW += weight1 * directPdfWMatA;
	}

	//--------------------------------------------------------------------------
	// Evaluate material B
	//--------------------------------------------------------------------------

	const BSDFEvent eventTypesMatB = mats[material->mix.matBIndex].eventTypes;
	if ((weight2 > 0.f) &&
			((!isTransmitEval && (eventTypesMatB & REFLECT)) ||
			(isTransmitEval && (eventTypesMatB & TRANSMIT))) &&
			!Spectrum_IsBlack(resultMatB)) {
		result += weight2 * resultMatB;
		directPdfW += weight2 * directPdfWMatB;
	}

	const BSDFEvent event = eventMatA | eventMatB;
	
	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

//------------------------------------------------------------------------------
// Material sample
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void MixMaterial_SampleSetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	// Save the parameters
	EvalStack_PushFloat3(fixedDir);
	EvalStack_PushFloat(u0);
	EvalStack_PushFloat(u1);
	EvalStack_PushFloat(passThroughEvent);

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const bool sampleMatA = (passThroughEvent < weight1);
	const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

	// Save factor
	EvalStack_PushFloat(factor);
	// Save sampleMatA
	EvalStack_PushInt(sampleMatA);

	// To setup the following EVAL_SAMPLE evaluation of first sub-nodes
	EvalStack_PushFloat3(fixedDir);
	EvalStack_PushFloat(u0);
	EvalStack_PushFloat(u1);
	EvalStack_PushFloat(passThroughEventFirst);

	// To setup the following EVAL_CONDITIONAL_GOTO evaluation
	EvalStack_PushInt(!sampleMatA);
}

OPENCL_FORCE_NOT_INLINE void MixMaterial_SampleSetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Pop the result of the first evaluation
	float3 resultFirst, sampledDirFirst;
	float pdfWFirst;
	BSDFEvent eventFirst;
	EvalStack_PopBSDFEvent(eventFirst);
	EvalStack_PopFloat(pdfWFirst);
	EvalStack_PopFloat3(sampledDirFirst);
	EvalStack_PopFloat3(resultFirst);
	
	// Pop sampleMatA
	bool sampleMatA;
	EvalStack_PopInt(sampleMatA);
	// Pop factor
	float factor;
	EvalStack_PopFloat(factor);

	// Pop parameters
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	// Save the parameters
	EvalStack_PushFloat3(fixedDir);
	EvalStack_PushFloat(u0);
	EvalStack_PushFloat(u1);
	EvalStack_PushFloat(passThroughEvent);

	// Save factor
	EvalStack_PushFloat(factor);
	// Save sampleMatA
	EvalStack_PushInt(sampleMatA);
	
	// Save the result of the first evaluation
	EvalStack_PushFloat3(resultFirst);
	EvalStack_PushFloat3(sampledDirFirst);
	EvalStack_PushFloat(pdfWFirst);
	EvalStack_PushBSDFEvent(eventFirst);

	// To setup the following EVAL_EVALUATE evaluation of second sub-nodes
	if (Spectrum_IsBlack(resultFirst)) {
		// In this case, sampledDirFirst is not initialized so I have to put
		// there some meaningful value
		sampledDirFirst = fixedDir;
	}
	EvalStack_PushFloat3(sampledDirFirst);
	EvalStack_PushFloat3(fixedDir);

	// To setup the first EVAL_CONDITIONAL_GOTO evaluation
	EvalStack_PushInt(sampleMatA);
}

OPENCL_FORCE_NOT_INLINE void MixMaterial_Sample(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Pop the result of the second evaluation
	float3 resultSecond;
	BSDFEvent eventSecond;
	float directPdfWSecond;
	EvalStack_PopFloat(directPdfWSecond);
	EvalStack_PopBSDFEvent(eventSecond);
	EvalStack_PopFloat3(resultSecond);

	// Pop the result of the first evaluation
	float3 resultFirst, sampledDirFirst;
	float pdfWFirst;
	BSDFEvent eventFirst;
	EvalStack_PopBSDFEvent(eventFirst);
	EvalStack_PopFloat(pdfWFirst);
	EvalStack_PopFloat3(sampledDirFirst);
	EvalStack_PopFloat3(resultFirst);
	
	// Pop sampleMatA
	bool sampleMatA;
	EvalStack_PopInt(sampleMatA);
	// Pop factor
	float factor;
	EvalStack_PopInt(factor);

	// Pop parameters
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	//--------------------------------------------------------------------------
	
	float3 result = resultFirst;
	if (Spectrum_IsBlack(result)) {
		MATERIAL_SAMPLE_RETURN_BLACK;
	}

	const float3 sampledDir = sampledDirFirst;
	
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;
	const float weightFirst = sampleMatA ? weight1 : weight2;
	const float weightSecond = sampleMatA ? weight2 : weight1;

	float pdfW = weightFirst * pdfWFirst;

	BSDFEvent event = eventFirst;

	if (eventFirst & SPECULAR) {
		EvalStack_PushFloat3(result);
		EvalStack_PushFloat3(sampledDir);
		EvalStack_PushFloat(pdfW);
		EvalStack_PushBSDFEvent(event);

		return;
	}
	
	result *= pdfW;

	const float3 fixedDirSecond = fixedDir;
	const float3 sampledDirSecond = sampledDir;

	// This test is usually done by BSDF_Evaluate() and must be repeated in
	// material referencing other materials
	const float isTransmitEval = (signbit(fixedDirSecond.z) != signbit(sampledDirSecond.z));

	const BSDFEvent eventTypesSecond = mats[sampleMatA ? material->mix.matAIndex : material->mix.matBIndex].eventTypes;
	if (((!isTransmitEval && (eventTypesSecond & REFLECT)) ||
			(isTransmitEval && (eventTypesSecond & TRANSMIT))) &&
			!Spectrum_IsBlack(resultSecond)) {
		result += weightSecond * resultSecond;
		pdfW += weightSecond * directPdfWSecond;
	}

	result /= pdfW;

	//--------------------------------------------------------------------------

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}
