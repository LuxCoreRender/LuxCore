#line 2 "material_main.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the License);         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an AS IS BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

//------------------------------------------------------------------------------
// Main material functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Material_Albedo
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_Albedo(const uint matIndex,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("===============================================================\n");
	printf("Material_Albedo()\n");
	printf("===============================================================\n");
#endif

	__global const Material* restrict startMat = &mats[matIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &matEvalStacks[gid * maxMaterialEvalStackSize];

	const uint evalOpStartIndex = startMat->evalAlbedoOpStartIndex;
	const uint evalOpLength = startMat->evalAlbedoOpLength;

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("matIndex=%d evalOpStartIndex=%d evalOpLength=%d\n", matIndex, evalOpStartIndex, evalOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	for (uint i = 0; i < evalOpLength; ++i) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
		printf("EvalOp: #%d evalStackOffset=%d\n", i, evalStackOffsetVal);
#endif

		__global const MaterialEvalOp* restrict evalOp = &matEvalOps[evalOpStartIndex + i];

		i += Material_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint MATERIALS_PARAM);
	}
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif
	
	float3 result;
	EvalStack_PopFloat3(result);

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("Result=(%f, %f, %f)\n", result.s0, result.s1, result.s2);
#endif

	return result;
}

//------------------------------------------------------------------------------
// Material_Evaluate
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_Evaluate(const uint matIndex,
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("===============================================================\n");
	printf("Material_Evaluate()\n");
	printf("===============================================================\n");
#endif

	__global const Material* restrict startMat = &mats[matIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &matEvalStacks[gid * maxMaterialEvalStackSize];

	const uint evalOpStartIndex = startMat->evalEvaluateOpStartIndex;
	const uint evalOpLength = startMat->evalEvaluateOpLength;

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("matIndex=%d evalOpStartIndex=%d evalOpLength=%d\n", matIndex, evalOpStartIndex, evalOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	// Evaluation parameters
	EvalStack_PushFloat3(lightDir);
	EvalStack_PushFloat3(eyeDir);

	for (uint i = 0; i < evalOpLength; ++i) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
		printf("EvalOp: #%d evalStackOffset=%d\n", i, evalStackOffsetVal);
#endif

		__global const MaterialEvalOp* restrict evalOp = &matEvalOps[evalOpStartIndex + i];

		i += Material_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint MATERIALS_PARAM);
	}
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif
	
	float pdfResult;
	EvalStack_PopFloat(pdfResult);
	BSDFEvent eventResult;
	EvalStack_PopBSDFEvent(eventResult);
	float3 result;
	EvalStack_PopFloat3(result);

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("Result=(%f, %f, %f) %d %f\n", result.s0, result.s1, result.s2, eventResult, pdfResult);
#endif

	*event = eventResult;
	if (directPdfW)
		*directPdfW = pdfResult;

	return result;
}

//------------------------------------------------------------------------------
// Material_Sample
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_Sample(const uint matIndex, __global const HitPoint *hitPoint,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("===============================================================\n");
	printf("Material_Sample()\n");
	printf("===============================================================\n");
#endif

	__global const Material* restrict startMat = &mats[matIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &matEvalStacks[gid * maxMaterialEvalStackSize];

	const uint evalOpStartIndex = startMat->evalSampleOpStartIndex;
	const uint evalOpLength = startMat->evalSampleOpLength;

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("matIndex=%d evalOpStartIndex=%d evalOpLength=%d\n", matIndex, evalOpStartIndex, evalOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	// Evaluation parameters
	EvalStack_PushFloat3(fixedDir);
	EvalStack_PushFloat(u0);
	EvalStack_PushFloat(u1);
	EvalStack_PushFloat(passThroughEvent);

	for (uint i = 0; i < evalOpLength; ++i) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
		printf("EvalOp: #%d evalStackOffset=%d\n", i, evalStackOffsetVal);
#endif

		__global const MaterialEvalOp* restrict evalOp = &matEvalOps[evalOpStartIndex + i];

		i += Material_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint MATERIALS_PARAM);
	}
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif
	
	BSDFEvent eventResult;
	EvalStack_PopBSDFEvent(eventResult);
	float pdfResult;
	EvalStack_PopFloat(pdfResult);
	float3 result, sampledDirResult;
	EvalStack_PopFloat3(sampledDirResult);
	EvalStack_PopFloat3(result);

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("Result=(%f, %f, %f) (%f, %f, %f) %f %d\n", result.s0, result.s1, result.s2,
			sampledDirResult.s0, sampledDirResult.s1, sampledDirResult.s2, pdfResult, eventResult);
#endif

	*sampledDir = sampledDirResult;
	if (pdfW)
		*pdfW = pdfResult;
	*event = eventResult;

	return result;
}

//------------------------------------------------------------------------------
// Material_GetPassThroughTransparency
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_GetPassThroughTransparency(const uint matIndex,
		__global const HitPoint *hitPoint,
		const float3 localFixedDir, const float passThroughEvent, const bool backTracing
		MATERIALS_PARAM_DECL) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("===============================================================\n");
	printf("Material_GetPassThroughTransparency()\n");
	printf("===============================================================\n");
#endif

	__global const Material* restrict startMat = &mats[matIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &matEvalStacks[gid * maxMaterialEvalStackSize];

	const uint evalOpStartIndex = startMat->evalGetPassThroughTransparencyOpStartIndex;
	const uint evalOpLength = startMat->evalGetPassThroughTransparencyOpLength;

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("matIndex=%d evalOpStartIndex=%d evalOpLength=%d\n", matIndex, evalOpStartIndex, evalOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	// Evaluation parameters
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushUInt(backTracing);

	for (uint i = 0; i < evalOpLength; ++i) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
		printf("EvalOp: #%d evalStackOffset=%d\n", i, evalStackOffsetVal);
#endif

		__global const MaterialEvalOp* restrict evalOp = &matEvalOps[evalOpStartIndex + i];

		i += Material_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint MATERIALS_PARAM);
	}
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif
	
	float3 result;
	EvalStack_PopFloat3(result);

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("Result=(%f, %f, %f)\n", result.s0, result.s1, result.s2);
#endif

	return result;
}

//------------------------------------------------------------------------------
// Material_GetEmittedRadiance
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_GetEmittedRadiance(const uint matIndex,
		__global const HitPoint *hitPoint, const float oneOverPrimitiveArea
		MATERIALS_PARAM_DECL) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("===============================================================\n");
	printf("Material_GetEmittedRadiance()\n");
	printf("===============================================================\n");
#endif

	__global const Material* restrict startMat = &mats[matIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &matEvalStacks[gid * maxMaterialEvalStackSize];

	const uint evalOpStartIndex = startMat->evalGetEmittedRadianceOpStartIndex;
	const uint evalOpLength = startMat->evalGetEmittedRadianceOpLength;

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("matIndex=%d evalOpStartIndex=%d evalOpLength=%d\n", matIndex, evalOpStartIndex, evalOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	// Evaluation parameters
	EvalStack_PushFloat(oneOverPrimitiveArea);

	for (uint i = 0; i < evalOpLength; ++i) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
		printf("EvalOp: #%d evalStackOffset=%d\n", i, evalStackOffsetVal);
#endif

		__global const MaterialEvalOp* restrict evalOp = &matEvalOps[evalOpStartIndex + i];

		i += Material_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint MATERIALS_PARAM);
	}
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif

	float3 result;
	EvalStack_PopFloat3(result);

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("Result=(%f, %f, %f)\n", result.s0, result.s1, result.s2);
#endif

	return result;
}

//------------------------------------------------------------------------------
// Material_GetInteriorVolume
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE uint Material_GetInteriorVolume(const uint matIndex,
		__global const HitPoint *hitPoint,
		const float passThroughEvent
		MATERIALS_PARAM_DECL) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("===============================================================\n");
	printf("Material_GetInteriorVolume()\n");
	printf("===============================================================\n");
#endif

	__global const Material* restrict startMat = &mats[matIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &matEvalStacks[gid * maxMaterialEvalStackSize];

	const uint evalOpStartIndex = startMat->evalGetInteriorVolumeOpStartIndex;
	const uint evalOpLength = startMat->evalGetInteriorVolumeOpLength;

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("matIndex=%d evalOpStartIndex=%d evalOpLength=%d\n", matIndex, evalOpStartIndex, evalOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	// Evaluation parameters
	EvalStack_PushFloat(passThroughEvent);

	for (uint i = 0; i < evalOpLength; ++i) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
		printf("EvalOp: #%d evalStackOffset=%d\n", i, evalStackOffsetVal);
#endif

		__global const MaterialEvalOp* restrict evalOp = &matEvalOps[evalOpStartIndex + i];

		i += Material_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint MATERIALS_PARAM);
	}
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif

	uint result;
	EvalStack_PopUInt(result);

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("Result=(%d)\n", result);
#endif

	return result;
}

//------------------------------------------------------------------------------
// Material_GetExteriorVolume
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE uint Material_GetExteriorVolume(const uint matIndex,
		__global const HitPoint *hitPoint,
		const float passThroughEvent
		MATERIALS_PARAM_DECL) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("===============================================================\n");
	printf("Material_GetExteriorVolume()\n");
	printf("===============================================================\n");
#endif

	__global const Material* restrict startMat = &mats[matIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &matEvalStacks[gid * maxMaterialEvalStackSize];

	const uint evalOpStartIndex = startMat->evalGetExteriorVolumeOpStartIndex;
	const uint evalOpLength = startMat->evalGetExteriorVolumeOpLength;

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("matIndex=%d evalOpStartIndex=%d evalOpLength=%d\n", matIndex, evalOpStartIndex, evalOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	// Evaluation parameters
	EvalStack_PushFloat(passThroughEvent);

	for (uint i = 0; i < evalOpLength; ++i) {
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
		printf("EvalOp: #%d evalStackOffset=%d\n", i, evalStackOffsetVal);
#endif

		__global const MaterialEvalOp* restrict evalOp = &matEvalOps[evalOpStartIndex + i];

		i += Material_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint MATERIALS_PARAM);
	}
#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif

	uint result;
	EvalStack_PopUInt(result);

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("Result=(%d)\n", result);
#endif

	return result;
}
