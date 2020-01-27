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
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {

	#define EvalStack_Push(a) { evalStack[*evalStackOffset] = a; *evalStackOffset = *evalStackOffset + 1; }
	#define EvalStack_Push3(a) { EvalStack_Push(a.s0); EvalStack_Push(a.s1); EvalStack_Push(a.s2); }
	#define EvalStack_Pop(a) { *evalStackOffset = *evalStackOffset - 1; a = evalStack[*evalStackOffset]; }
	#define EvalStack_Pop3(a) { EvalStack_Pop(a.s2); EvalStack_Pop(a.s1); EvalStack_Pop(a.s0); }
	#define EvalStack_Read(x) (evalStack[(*evalStackOffset) + x])

	__global const Texture* restrict tex = &texs[evalOp->texIndex];

//	printf("EvalOp tex index=%d type=%d evalType=%d *evalStackOffset=%d\n", evalOp->texIndex, tex->type, evalOp->evalType, *evalStackOffset);

	const TextureEvalOpType evalType = evalOp->evalType;
	switch (tex->type) {
		//----------------------------------------------------------------------
		// CONST_FLOAT
		//----------------------------------------------------------------------
		case CONST_FLOAT: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ConstFloatTexture_ConstEvaluateFloat(tex);
					EvalStack_Push(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ConstFloatTexture_ConstEvaluateSpectrum(tex);
					EvalStack_Push3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ConstFloat3Texture_Bump(hitPoint);
					EvalStack_Push3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// CONST_FLOAT3
		//----------------------------------------------------------------------
		case CONST_FLOAT3: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ConstFloat3Texture_ConstEvaluateFloat(tex);
					EvalStack_Push(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ConstFloat3Texture_ConstEvaluateSpectrum(tex);
					EvalStack_Push3(eval);
					break;
				}	
				case EVAL_BUMP: {
					const float3 shadeN = ConstFloat3Texture_Bump(hitPoint);
					EvalStack_Push3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// IMAGEMAP
		//----------------------------------------------------------------------
		case IMAGEMAP: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ImageMapTexture_ConstEvaluateFloat(tex, hitPoint IMAGEMAPS_PARAM);
					EvalStack_Push(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ImageMapTexture_ConstEvaluateSpectrum(tex, hitPoint IMAGEMAPS_PARAM);
					EvalStack_Push3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ImageMapTexture_Bump(tex, hitPoint IMAGEMAPS_PARAM);
					EvalStack_Push3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// SCALE_TEX
		//----------------------------------------------------------------------
		case SCALE_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_Pop(tex2);
					EvalStack_Pop(tex1);

					const float eval = ScaleTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_Push(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_Pop3(tex2);
					EvalStack_Pop3(tex1);

					const float3 eval = ScaleTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_Push3(eval);
					break;
				}
				case EVAL_BUMP: {
					float evalFloatTex1, evalFloatTex2;

					EvalStack_Pop(evalFloatTex2);
					EvalStack_Pop(evalFloatTex1);

					float3 bumbNTex1, bumbNTex2;

					EvalStack_Pop3(bumbNTex2);
					EvalStack_Pop3(bumbNTex1);

					const float3 shadeN = ScaleTexture_Bump(hitPoint, bumbNTex1, bumbNTex2, evalFloatTex1, evalFloatTex2);
					EvalStack_Push3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// CHECKERBOARD2D
		//----------------------------------------------------------------------
		case CHECKERBOARD2D: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_Pop(tex2);
					EvalStack_Pop(tex1);

					const float eval = CheckerBoard2DTexture_ConstEvaluateFloat(hitPoint, tex1, tex2, &tex->checkerBoard2D.mapping);
					EvalStack_Push(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_Pop3(tex2);
					EvalStack_Pop3(tex1);

					const float3 eval = CheckerBoard2DTexture_ConstEvaluateSpectrum(hitPoint, tex1, tex2, &tex->checkerBoard2D.mapping);
					EvalStack_Push3(eval);
					break;
				}
				case EVAL_BUMP: {
					// TODO
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// For the very special case of Triplanar texture evaluation
		//
		// EVAL_TRIPLANAR_STEP_1, EVAL_TRIPLANAR_STEP_2, EVAL_TRIPLANAR_STEP_3
		// and EVAL_TRIPLANAR
		//----------------------------------------------------------------------
		case TRIPLANAR_TEX: {
			switch (evalType) {
				case EVAL_TRIPLANAR_STEP_1: {
					// Save original UV
					const uint uvIndex = tex->triplanarTex.uvIndex;
					EvalStack_Push(hitPoint->uv[uvIndex].u);
					EvalStack_Push(hitPoint->uv[uvIndex].v);

					// Compute localPoint
					float3 localShadeN;
					const float3 localPoint = TextureMapping3D_Map(&tex->triplanarTex.mapping, hitPoint, &localShadeN);

					// Compute the 3 weights
					float weightsX = Sqr(Sqr(localShadeN.x));
					float weightsY = Sqr(Sqr(localShadeN.y));
					float weightsZ = Sqr(Sqr(localShadeN.z));

					const float sum = weightsX + weightsY + weightsZ;
					weightsX = weightsX / sum;
					weightsY = weightsY / sum;
					weightsZ = weightsZ / sum;

					// Save 3 weights	
					EvalStack_Push(weightsX);
					EvalStack_Push(weightsY);
					EvalStack_Push(weightsZ);
					// Save localPoint
					EvalStack_Push3(localPoint);

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					hitPointTmp->uv[uvIndex].u = localPoint.y;
					hitPointTmp->uv[uvIndex].v = localPoint.z;
					break;
				}
				case EVAL_TRIPLANAR_STEP_2: {
					// Read localPoint
					//
					// Note: -3 is there to skip the result of EVAL_TRIPLANAR_STEP_1
					const float3 localPoint = (float3)(EvalStack_Read(-3 - 3), EvalStack_Read(-3 - 2), EvalStack_Read(-3 - 1));

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					const uint uvIndex = tex->triplanarTex.uvIndex;
					hitPointTmp->uv[uvIndex].u = localPoint.x;
					hitPointTmp->uv[uvIndex].v = localPoint.z;
					break;
				}
				case EVAL_TRIPLANAR_STEP_3: {
					// Read localPoint
					//
					// Note: -6 is there to skip the result of EVAL_TRIPLANAR_STEP_1 and EVAL_TRIPLANAR_STEP_2
					const float3 localPoint = (float3)(EvalStack_Read(-6 - 3), EvalStack_Read(-6 - 2), EvalStack_Read(-6 - 1));

					// Update HitPoint
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					const uint uvIndex = tex->triplanarTex.uvIndex;
					hitPointTmp->uv[uvIndex].u = localPoint.x;
					hitPointTmp->uv[uvIndex].v = localPoint.y;
					break;
				}
				case EVAL_FLOAT:
				case EVAL_SPECTRUM: {
					// Read textures evaluation
					float3 tex1, tex2, tex3;
					EvalStack_Pop3(tex3);
					EvalStack_Pop3(tex2);
					EvalStack_Pop3(tex1);
					
					// Read localPoint
					float3 localPoint;
					EvalStack_Pop3(localPoint);

					// Read 3 weights
					float weightX, weightY, weightZ;
					EvalStack_Pop(weightZ);
					EvalStack_Pop(weightY);
					EvalStack_Pop(weightX);

					// Restore original UV
					const uint uvIndex = tex->triplanarTex.uvIndex;
					__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
					EvalStack_Pop(hitPointTmp->uv[uvIndex].v);
					EvalStack_Pop(hitPointTmp->uv[uvIndex].u);

					const float3 result = tex1 * weightX + tex2 * weightY + tex3 * weightZ;
					if (evalType == EVAL_FLOAT) {
						EvalStack_Push(Spectrum_Y(result));
					} else {
						EvalStack_Push3(result);
					}
					break;
				}
				case EVAL_BUMP:
					// TODO
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		default:
			// Something wrong here
			break;
	}

	#undef EvalStack_Push
	#undef EvalStack_Push3
	#undef EvalStack_Pop
	#undef EvalStack_Pop3
	#undef EvalStack_Read
}

//------------------------------------------------------------------------------
// Texture_GetFloatValue()
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

	uint evalStackOffset = 0;
	for (uint i = 0; i < evalFloatOpLength; ++i) {
//		printf("EvalOp: #%d\n", i);

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalFloatOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffset, hitPoint, 0.f TEXTURES_PARAM);
	}
//	printf("evalStackOffset=#%d\n", evalStackOffset);

	const float result = evalStack[0];

//	printf("Result=%f\n", v);
	
	return result;
}

//------------------------------------------------------------------------------
// Texture_GetSpectrumValue()
//------------------------------------------------------------------------------

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

		Texture_EvalOp(evalOp, evalStack, &evalStackOffset, hitPoint, 0.f TEXTURES_PARAM);
	}
//	printf("evalStackOffset=#%d\n", evalStackOffset);
	
	const float3 result = (float3)(evalStack[0], evalStack[1], evalStack[2]);

//	printf("Result=(%f, %f, %f)\n", result.s0, result.s1, result.s2);

	return result;
}

//------------------------------------------------------------------------------
// Texture_Bump()
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Texture_Bump(const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
//	printf("===============================================================\n");
//	printf("Texture_Bump()\n");
//	printf("===============================================================\n");

	__global const Texture* restrict startTex = &texs[texIndex];
	const size_t gid = get_global_id(0);
	__global float *evalStack = &texEvalStacks[gid * maxTextureEvalStackSize];

	const uint evalBumpOpStartIndex = startTex->evalBumpOpStartIndex;
	const uint evalBumpOpLength = startTex->evalBumpOpLength;

	uint evalStackOffset = 0;
	for (uint i = 0; i < evalBumpOpLength; ++i) {
//		printf("EvalOp: #%d\n", i);

		__global const TextureEvalOp* restrict evalOp = &texEvalOps[evalBumpOpStartIndex + i];

		Texture_EvalOp(evalOp, evalStack, &evalStackOffset, hitPoint, sampleDistance TEXTURES_PARAM);
	}
//		printf("evalStackOffset=#%d\n", evalStack);

	const float3 result = (float3)(evalStack[0], evalStack[1], evalStack[2]);

//	printf("Result=(%f, %f, %f)\n", result.x, result.y, result.z);

	return result;
}
