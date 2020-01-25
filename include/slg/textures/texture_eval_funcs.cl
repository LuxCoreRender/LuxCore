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

	for (uint i = 0; i < evalFloatOpLength; ++i) {
//		printf("EvalOp: #%d\n", i);

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalFloatOpStartIndex + i];

		float eval;
		__global const Texture* restrict tex = &texs[evalOp->texIndex];

//		printf("EvalOp tex index=%d and type=%d\n", evalOp->texIndex, tex->type);

		switch (tex->type) {
			case CONST_FLOAT: {
				eval = ConstFloatTexture_ConstEvaluateFloat(tex);
				break;
			}
			case CONST_FLOAT3: {
				eval = ConstFloat3Texture_ConstEvaluateFloat(tex);
				break;
			}
			case IMAGEMAP: {
				eval = ImageMapTexture_ConstEvaluateFloat(tex, hitPoint IMAGEMAPS_PARAM);
				break;
			}
			case SCALE_TEX: {
				const float tex2 = *(--evalStack);
				const float tex1 = *(--evalStack);

				eval = ScaleTexture_ConstEvaluateFloat(tex1, tex2);
				break;
			}
			default:
				// Something wrong here
				continue;
		}

		*evalStack++ = eval;
	}
	
	const float v = *(--evalStack);

//	printf("Result=%f\n", v);

	return v;
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

	for (uint i = 0; i < evalSpectrumOpLength; ++i) {
//		printf("EvalOp: #%d\n", i);

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalSpectrumOpStartIndex + i];

		float3 eval;
		__global const Texture* restrict tex = &texs[evalOp->texIndex];

//		printf("EvalOp tex index=%d and type=%d\n", evalOp->texIndex, tex->type);

		switch (tex->type) {
			case CONST_FLOAT: {
				eval = ConstFloatTexture_ConstEvaluateSpectrum(tex);
				break;
			}
			case CONST_FLOAT3: {
				eval = ConstFloat3Texture_ConstEvaluateSpectrum(tex);
				break;
			}
			case IMAGEMAP: {
				eval = ImageMapTexture_ConstEvaluateSpectrum(tex, hitPoint IMAGEMAPS_PARAM);
				break;
			}
			case SCALE_TEX: {
				const float b2 = *(--evalStack);
				const float g2 = *(--evalStack);
				const float r2 = *(--evalStack);
				const float3 tex2 = (float3)(r2, g2, b2);

				const float b1 = *(--evalStack);
				const float g1 = *(--evalStack);
				const float r1 = *(--evalStack);
				const float3 tex1 = (float3)(r1, g1, b1);

				eval = ScaleTexture_ConstEvaluateSpectrum(tex1, tex2);
				break;
			}
			default:
				// Something wrong here
				continue;
		}

		*evalStack++ = eval.s0;
		*evalStack++ = eval.s1;
		*evalStack++ = eval.s2;
	}
	
	const float b = *(--evalStack);
	const float g = *(--evalStack);
	const float r = *(--evalStack);

//	printf("Result=(%f, %f, %f)\n", r, g, b);

	return (float3)(r, g, b);
}

OPENCL_FORCE_NOT_INLINE float3 Texture_Bump(const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	return VLOAD3F(&hitPoint->shadeN.x);
}
