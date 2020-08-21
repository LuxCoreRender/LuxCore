#line 2 "texture_funcs.cl"

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
// Texture_GetFloatValue()
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float Texture_GetFloatValueSlowPath(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("===============================================================\n");
	printf("Texture_GetFloatValue()\n");
	printf("===============================================================\n");
#endif

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalFloatOpStartIndex = startTex->evalFloatOpStartIndex;
	const uint evalFloatOpLength = startTex->evalFloatOpLength;

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("texIndex=%d evalFloatOpStartIndex=%d evalFloatOpLength=%d\n", texIndex, evalFloatOpStartIndex, evalFloatOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	for (uint i = 0; i < evalFloatOpLength; ++i) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
		printf("EvalOp: #%d\n", i);
#endif

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalFloatOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint, 0.f TEXTURES_PARAM);
	}
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif

	float result;
	EvalStack_PopFloat(result);

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("Result=%f\n", result);
#endif
	
	return result;
}

OPENCL_FORCE_INLINE float Texture_GetFloatValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global const Texture* restrict tex = &texs[texIndex];
	switch (tex->type) {
		//----------------------------------------------------------------------
		// Fast paths
		//----------------------------------------------------------------------
		case CONST_FLOAT:
			return ConstFloatTexture_ConstEvaluateFloat(tex);
		case CONST_FLOAT3:
			return ConstFloat3Texture_ConstEvaluateFloat(tex);
		case IMAGEMAP:
			return ImageMapTexture_ConstEvaluateFloat(tex, hitPoint TEXTURES_PARAM);
		//----------------------------------------------------------------------
		// Fall back to the slow path
		//----------------------------------------------------------------------
		default:
			return Texture_GetFloatValueSlowPath(texIndex, hitPoint TEXTURES_PARAM);
	}
}

//------------------------------------------------------------------------------
// Texture_GetSpectrumValue()
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Texture_GetSpectrumValueSlowPath(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("===============================================================\n");
	printf("Texture_GetSpectrumValue()\n");
	printf("===============================================================\n");
#endif

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalSpectrumOpStartIndex = startTex->evalSpectrumOpStartIndex;
	const uint evalSpectrumOpLength = startTex->evalSpectrumOpLength;

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("texIndex=%d evalSpectrumOpStartIndex=%d evalSpectrumOpLength=%d\n", texIndex, evalSpectrumOpStartIndex, evalSpectrumOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	for (uint i = 0; i < evalSpectrumOpLength; ++i) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
		printf("EvalOp: #%d\n", i);
#endif

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalSpectrumOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint, 0.f TEXTURES_PARAM);
	}
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif
	
	float3 result;
	EvalStack_PopFloat3(result);


#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("Result=(%f, %f, %f)\n", result.s0, result.s1, result.s2);
#endif

	return result;
}

OPENCL_FORCE_INLINE float3 Texture_GetSpectrumValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global const Texture* restrict tex = &texs[texIndex];
	switch (tex->type) {
		//----------------------------------------------------------------------
		// Fast paths
		//----------------------------------------------------------------------
		case CONST_FLOAT:
			return ConstFloatTexture_ConstEvaluateSpectrum(tex);
		case CONST_FLOAT3:
			return ConstFloat3Texture_ConstEvaluateSpectrum(tex);
		case IMAGEMAP:
			return ImageMapTexture_ConstEvaluateSpectrum(tex, hitPoint TEXTURES_PARAM);
		//----------------------------------------------------------------------
		// Fall back to the slow path
		//----------------------------------------------------------------------
		default:
			return Texture_GetSpectrumValueSlowPath(texIndex, hitPoint TEXTURES_PARAM);
	}
}

//------------------------------------------------------------------------------
// Texture_Bump()
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Texture_Bump(const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("===============================================================\n");
	printf("Texture_Bump()\n");
	printf("===============================================================\n");
#endif

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalBumpOpStartIndex = startTex->evalBumpOpStartIndex;
	const uint evalBumpOpLength = startTex->evalBumpOpLength;

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("texIndex=%d evalSpectrumOpStartIndex=%d evalSpectrumOpLength=%d\n", texIndex, evalBumpOpStartIndex, evalBumpOpLength);
#endif

	uint evalStackOffsetVal = 0;
	uint *evalStackOffset = &evalStackOffsetVal; // Used by macros
	for (uint i = 0; i < evalBumpOpLength; ++i) {
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
		printf("EvalOp: #%d\n", i);
#endif

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalBumpOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffsetVal, hitPoint, sampleDistance TEXTURES_PARAM);
	}
#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("evalStackOffset=#%d\n", evalStackOffsetVal);
#endif

	float3 result;
	EvalStack_PopFloat3(result);

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("Result=(%f, %f, %f)\n", result.x, result.y, result.z);
#endif

	return result;
}
