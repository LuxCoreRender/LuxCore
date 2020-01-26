#line 2 "texture_eval_funcs.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
// Texture evaluation functions
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void Texture_EvalOp(
		__global const TextureEvalOp* restrict evalOp,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {

	#define EvalStack_Push(a) { evalStack[*evalStackOffset] = a; *evalStackOffset = *evalStackOffset + 1; }
	#define EvalStack_Pop(a) { *evalStackOffset = *evalStackOffset - 1; a = evalStack[*evalStackOffset]; }

	__global const Texture* restrict tex = &texs[evalOp->texIndex];

//	printf("EvalOp tex index=%d and type=%d\n", evalOp->texIndex, tex->type);

	const TextureEvalOpType evalType = evalOp->evalType;
	switch (tex->type) {
		case CONST_FLOAT: {
			if (evalType == EVAL_FLOAT) {
				const float eval = ConstFloatTexture_ConstEvaluateFloat(tex);
				EvalStack_Push(eval);
			} else {
				const float3 eval = ConstFloatTexture_ConstEvaluateSpectrum(tex);
				EvalStack_Push(eval.s0);
				EvalStack_Push(eval.s1);
				EvalStack_Push(eval.s2);
			}
			break;
		}
		case CONST_FLOAT3: {
			if (evalType == EVAL_FLOAT) {
				const float eval = ConstFloat3Texture_ConstEvaluateFloat(tex);
				EvalStack_Push(eval);
			} else {
				const float3 eval = ConstFloat3Texture_ConstEvaluateSpectrum(tex);
				EvalStack_Push(eval.s0);
				EvalStack_Push(eval.s1);
				EvalStack_Push(eval.s2);
			}
			break;
		}
		case IMAGEMAP: {
			if (evalType == EVAL_FLOAT) {
				const float eval = ImageMapTexture_ConstEvaluateFloat(tex, hitPoint IMAGEMAPS_PARAM);
				EvalStack_Push(eval);
			} else {
				const float3 eval = ImageMapTexture_ConstEvaluateSpectrum(tex, hitPoint IMAGEMAPS_PARAM);
				EvalStack_Push(eval.s0);
				EvalStack_Push(eval.s1);
				EvalStack_Push(eval.s2);
				
			}
			break;
		}
		case SCALE_TEX: {
			if (evalType == EVAL_FLOAT) {
				float tex1, tex2;
				EvalStack_Pop(tex2);
				EvalStack_Pop(tex1);

				const float eval = ScaleTexture_ConstEvaluateFloat(tex1, tex2);
				EvalStack_Push(eval);
			} else {
				float3 tex1, tex2;
				EvalStack_Pop(tex2.s2);
				EvalStack_Pop(tex2.s1);
				EvalStack_Pop(tex2.s0);
				
				EvalStack_Pop(tex1.s2);
				EvalStack_Pop(tex1.s1);
				EvalStack_Pop(tex1.s0);

				const float3 eval = ScaleTexture_ConstEvaluateSpectrum(tex1, tex2);
				EvalStack_Push(eval.s0);
				EvalStack_Push(eval.s1);
				EvalStack_Push(eval.s2);
			}
			break;
		}
		default:
			// Something wrong here
			break;
	}

	#undef EvalStack_Push
	#undef EvalStack_Pop
}

OPENCL_FORCE_NOT_INLINE float Texture_GetFloatValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
//	printf("===============================================================\n");
//	printf("Texture_GetFloatValue()\n");
//	printf("===============================================================\n");

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalFloatOpStartIndex = startTex->evalFloatOpStartIndex;
	const uint evalFloatOpLength = startTex->evalFloatOpLength;

//	printf("texIndex=%d evalFloatOpStartIndex=%d evalFloatOpLength=%d\n", texIndex, evalFloatOpStartIndex, evalFloatOpLength);

	uint evalStackOffset = 0;
	for (uint i = 0; i < evalFloatOpLength; ++i) {
//		printf("EvalOp: #%d\n", i);

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalFloatOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffset, hitPoint TEXTURES_PARAM);
	}
//		printf("evalStackOffset=#%d\n", evalStack);

	const float result = evalStack[0];

//	printf("Result=%f\n", v);
	
	return result;
}

OPENCL_FORCE_NOT_INLINE float3 Texture_GetSpectrumValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
//	printf("===============================================================\n");
//	printf("Texture_GetSpectrumValue()\n");
//	printf("===============================================================\n");

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalSpectrumOpStartIndex = startTex->evalSpectrumOpStartIndex;
	const uint evalSpectrumOpLength = startTex->evalSpectrumOpLength;

//	printf("texIndex=%d evalSpectrumOpStartIndex=%d evalSpectrumOpLength=%d\n", texIndex, evalSpectrumOpStartIndex, evalSpectrumOpLength);

	uint evalStackOffset = 0;
	for (uint i = 0; i < evalSpectrumOpLength; ++i) {
//		printf("EvalOp: #%d\n", i);

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalSpectrumOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffset, hitPoint TEXTURES_PARAM);
	}
//		printf("evalStackOffset=#%d\n", evalStack);
	
	const float3 result = (float3)(evalStack[0], evalStack[1], evalStack[2]);

//	printf("Result=(%f, %f, %f)\n", result.s0, result.s1, result.s2);

	return result;
}

OPENCL_FORCE_NOT_INLINE float3 Texture_Bump(const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	return VLOAD3F(&hitPoint->shadeN.x);
}
