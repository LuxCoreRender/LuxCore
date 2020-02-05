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

//#define DEBUG_PRINTF_KERNEL_NAME 1
//#define DEBUG_PRINTF_TEXTURE_EVAL 1

//------------------------------------------------------------------------------
// Texture evaluation functions
//------------------------------------------------------------------------------

#define EvalStack_PushUInt(a) { ((__global uint *)evalStack)[*evalStackOffset] = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PopUInt(a) { *evalStackOffset = *evalStackOffset - 1; a = ((__global uint *)evalStack)[*evalStackOffset]; }

#define EvalStack_PushFloat(a) { evalStack[*evalStackOffset] = a; *evalStackOffset = *evalStackOffset + 1; }
#define EvalStack_PushFloat2(a) { EvalStack_PushFloat(a.s0); EvalStack_PushFloat(a.s1); }
#define EvalStack_PushFloat3(a) { EvalStack_PushFloat(a.s0); EvalStack_PushFloat(a.s1); EvalStack_PushFloat(a.s2); }
#define EvalStack_PopFloat(a) { *evalStackOffset = *evalStackOffset - 1; a = evalStack[*evalStackOffset]; }
#define EvalStack_PopFloat2(a) { EvalStack_PopFloat(a.s1); EvalStack_PopFloat(a.s0); }
#define EvalStack_PopFloat3(a) { EvalStack_PopFloat(a.s2); EvalStack_PopFloat(a.s1); EvalStack_PopFloat(a.s0); }
#define EvalStack_ReadFloat(x) (evalStack[(*evalStackOffset) + x])
#define EvalStack_ReadFloat2(x) ((float2)(evalStack[(*evalStackOffset) + x], evalStack[(*evalStackOffset) + x + 1]))
#define EvalStack_ReadFloat3(x) ((float3)(evalStack[(*evalStackOffset) + x], evalStack[(*evalStackOffset) + x + 1], evalStack[(*evalStackOffset) + x + 2]))

OPENCL_FORCE_NOT_INLINE void Texture_EvalOpGenericBumpOffsetU(
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance) {
	const float3 origP = VLOAD3F(&hitPoint->p.x);
	const float3 origShadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float2 origUV = VLOAD2F(&hitPoint->uv[0].u);

	// Save original P
	EvalStack_PushFloat3(origP);
	// Save original shadeN
	EvalStack_PushFloat3(origShadeN);
	// Save original UV
	EvalStack_PushFloat2(origUV);

	// Update HitPoint
	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	const float3 dpdu = VLOAD3F(&hitPointTmp->dpdu.x);
	const float3 dndu = VLOAD3F(&hitPoint->dndu.x);
	// Shift hitPointTmp.du in the u direction and calculate value
	const float uu = sampleDistance / length(dpdu);
	VSTORE3F(origP + uu * dpdu, &hitPointTmp->p.x);
	hitPointTmp->uv[0].u = origUV.s0 + uu;
	hitPointTmp->uv[0].v = origUV.s1;
	VSTORE3F(normalize(origShadeN + uu * dndu), &hitPointTmp->shadeN.x);
}

OPENCL_FORCE_NOT_INLINE void Texture_EvalOpGenericBumpOffsetV(
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance) {
	// -1 is for result of EVAL_BUMP_GENERIC_OFFSET_U
	const float3 origP = EvalStack_ReadFloat3(-8 - 1);
	const float3 origShadeN = EvalStack_ReadFloat3(-5 - 1);
	const float2 origUV = EvalStack_ReadFloat2(-2 - 1);

	// Update HitPoint
	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	const float3 dpdv = VLOAD3F(&hitPointTmp->dpdv.x);
	const float3 dndv = VLOAD3F(&hitPoint->dndv.x);
	// Shift hitPointTmp.dv in the v direction and calculate value
	const float vv = sampleDistance / length(dpdv);
	VSTORE3F(origP + vv * dpdv, &hitPointTmp->p.x);
	hitPointTmp->uv[0].u = origUV.s0;
	hitPointTmp->uv[0].v = origUV.s1 + vv;
	VSTORE3F(normalize(origShadeN + vv * dndv), &hitPointTmp->shadeN.x);
}

OPENCL_FORCE_NOT_INLINE void Texture_EvalOpGenericBump(
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance) {
	float evalFloatTexBase, evalFloatTexOffsetU, evalFloatTexOffsetV;
	EvalStack_PopFloat(evalFloatTexOffsetV);
	EvalStack_PopFloat(evalFloatTexOffsetU);

	float3 origP, origShadeN;
	float2 origUV;
	EvalStack_PopFloat2(origUV);
	EvalStack_PopFloat3(origShadeN);
	EvalStack_PopFloat3(origP);

	// evalFloatTexBase is the very first evaluation done so
	// it is the last for a stack pop
	EvalStack_PopFloat(evalFloatTexBase);

	// Restore original P, shadeN and UV
	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
	VSTORE3F(origP, &hitPointTmp->p.x);
	VSTORE3F(origShadeN, &hitPointTmp->shadeN.x);
	hitPointTmp->uv[0].u = origUV.s0;
	hitPointTmp->uv[0].v = origUV.s1;

	const float3 shadeN = GenericTexture_Bump(hitPoint, sampleDistance,
			evalFloatTexBase, evalFloatTexOffsetU, evalFloatTexOffsetV);
	EvalStack_PushFloat3(shadeN);
}

OPENCL_FORCE_NOT_INLINE void Texture_EvalOp(
		__global const TextureEvalOp* restrict evalOp,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	__global const Texture* restrict tex = &texs[evalOp->texIndex];

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("EvalOp tex index=%d type=%d evalType=%d *evalStackOffset=%d\n", evalOp->texIndex, tex->type, evalOp->evalType, *evalStackOffset);
#endif

	const TextureEvalOpType evalType = evalOp->evalType;
	switch (tex->type) {
		//----------------------------------------------------------------------
		// CONST_FLOAT
		//----------------------------------------------------------------------
		case CONST_FLOAT: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ConstFloatTexture_ConstEvaluateFloat(tex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ConstFloatTexture_ConstEvaluateSpectrum(tex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ConstTexture_Bump(hitPoint);
					EvalStack_PushFloat3(shadeN);
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
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ConstFloat3Texture_ConstEvaluateSpectrum(tex);
					EvalStack_PushFloat3(eval);
					break;
				}	
				case EVAL_BUMP: {
					const float3 shadeN = ConstTexture_Bump(hitPoint);
					EvalStack_PushFloat3(shadeN);
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
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ImageMapTexture_ConstEvaluateSpectrum(tex, hitPoint IMAGEMAPS_PARAM);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ImageMapTexture_Bump(tex, hitPoint IMAGEMAPS_PARAM);
					EvalStack_PushFloat3(shadeN);
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
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = ScaleTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = ScaleTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					float evalFloatTex1, evalFloatTex2;

					EvalStack_PopFloat(evalFloatTex2);
					EvalStack_PopFloat(evalFloatTex1);

					float3 bumbNTex1, bumbNTex2;

					EvalStack_PopFloat3(bumbNTex2);
					EvalStack_PopFloat3(bumbNTex1);

					const float3 shadeN = ScaleTexture_Bump(hitPoint, bumbNTex1, bumbNTex2, evalFloatTex1, evalFloatTex2);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// FRESNEL_APPROX_N
		//----------------------------------------------------------------------
		case FRESNEL_APPROX_N: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float eval = FresnelApproxNTexture_ConstEvaluateFloat(tex1);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float3 eval = FresnelApproxNTexture_ConstEvaluateSpectrum(tex1);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// FRESNEL_APPROX_K
		//----------------------------------------------------------------------
		case FRESNEL_APPROX_K: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float eval = FresnelApproxKTexture_ConstEvaluateFloat(tex1);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float3 eval = FresnelApproxKTexture_ConstEvaluateSpectrum(tex1);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// MIX_TEX
		//----------------------------------------------------------------------
		case MIX_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float amt, tex1, tex2;
					EvalStack_PopFloat(amt);
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = MixTexture_ConstEvaluateFloat(tex1, tex2, amt);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float amt;
					float3 tex1, tex2;
					EvalStack_PopFloat(amt);
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = MixTexture_ConstEvaluateSpectrum(tex1, tex2, amt);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					float evalFloatTex1, evalFloatTex2, evalFloatAmount;

					EvalStack_PopFloat(evalFloatAmount);
					EvalStack_PopFloat(evalFloatTex2);
					EvalStack_PopFloat(evalFloatTex1);

					float3 bumbNTex1, bumbNTex2, bumbNAmount;

					EvalStack_PopFloat3(bumbNAmount);
					EvalStack_PopFloat3(bumbNTex2);
					EvalStack_PopFloat3(bumbNTex1);

					const float3 shadeN = MixTexture_Bump(hitPoint,
							bumbNTex1, bumbNTex2, bumbNAmount,
							evalFloatTex1, evalFloatTex2, evalFloatAmount);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// ADD_TEX
		//----------------------------------------------------------------------
		case ADD_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = AddTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = AddTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					float3 bumbNTex1, bumbNTex2;

					EvalStack_PopFloat3(bumbNTex2);
					EvalStack_PopFloat3(bumbNTex1);

					const float3 shadeN = AddTexture_Bump(hitPoint, bumbNTex1, bumbNTex2);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// SUBTRACT_TEX
		//----------------------------------------------------------------------
		case SUBTRACT_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = SubtractTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = SubtractTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					float3 bumbNTex1, bumbNTex2;

					EvalStack_PopFloat3(bumbNTex2);
					EvalStack_PopFloat3(bumbNTex1);

					const float3 shadeN = SubtractTexture_Bump(hitPoint, bumbNTex1, bumbNTex2);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// HITPOINTCOLOR
		//----------------------------------------------------------------------
		case HITPOINTCOLOR: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = HitPointColorTexture_ConstEvaluateFloat(hitPoint,
							tex->hitPointColor.dataIndex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = HitPointColorTexture_ConstEvaluateSpectrum(hitPoint,
							tex->hitPointColor.dataIndex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// HITPOINTALPHA
		//----------------------------------------------------------------------
		case HITPOINTALPHA: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = HitPointAlphaTexture_ConstEvaluateFloat(hitPoint,
							tex->hitPointAlpha.dataIndex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = HitPointAlphaTexture_ConstEvaluateSpectrum(hitPoint,
							tex->hitPointAlpha.dataIndex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// HITPOINTGREY
		//----------------------------------------------------------------------
		case HITPOINTGREY: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = HitPointGreyTexture_ConstEvaluateFloat(hitPoint,
							tex->hitPointGrey.dataIndex, tex->hitPointGrey.channelIndex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = HitPointGreyTexture_ConstEvaluateSpectrum(hitPoint,
							tex->hitPointGrey.dataIndex, tex->hitPointGrey.channelIndex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// NORMALMAP_TEX
		//----------------------------------------------------------------------
		case NORMALMAP_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = NormalMapTexture_ConstEvaluateFloat();
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = NormalMapTexture_ConstEvaluateSpectrum();
					EvalStack_PushFloat3(eval);
					break;
				}	
				case EVAL_BUMP: {
					float3 evalSpectrumTex;
					EvalStack_PopFloat3(evalSpectrumTex);

					const float3 shadeN = NormalMapTexture_Bump(tex, hitPoint, evalSpectrumTex);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLACKBODY_TEX
		//----------------------------------------------------------------------
		case BLACKBODY_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlackBodyTexture_ConstEvaluateFloat(VLOAD3F(tex->blackBody.rgb.c));
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlackBodyTexture_ConstEvaluateSpectrum(VLOAD3F(tex->blackBody.rgb.c));
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// IRREGULARDATA_TEX
		//----------------------------------------------------------------------
		case IRREGULARDATA_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = IrregularDataTexture_ConstEvaluateFloat(VLOAD3F(tex->irregularData.rgb.c));
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = IrregularDataTexture_ConstEvaluateSpectrum(VLOAD3F(tex->irregularData.rgb.c));
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// DENSITYGRID_TEX
		//----------------------------------------------------------------------
		case DENSITYGRID_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = DensityGridTexture_ConstEvaluateFloat(hitPoint,
							tex->densityGrid.nx, tex->densityGrid.ny, tex->densityGrid.nz,
							tex->densityGrid.imageMapIndex, &tex->densityGrid.mapping
							IMAGEMAPS_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = DensityGridTexture_ConstEvaluateSpectrum(hitPoint,
							tex->densityGrid.nx, tex->densityGrid.ny, tex->densityGrid.nz,
							tex->densityGrid.imageMapIndex, &tex->densityGrid.mapping
							IMAGEMAPS_PARAM);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// ABS_TEX
		//----------------------------------------------------------------------
		case ABS_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float eval = AbsTexture_ConstEvaluateFloat(tex1);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float3 eval = AbsTexture_ConstEvaluateSpectrum(tex1);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// CLAMP_TEX
		//----------------------------------------------------------------------
		case CLAMP_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float eval = ClampTexture_ConstEvaluateFloat(tex1,
							tex->clampTex.minVal, tex->clampTex.maxVal);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float3 eval = ClampTexture_ConstEvaluateSpectrum(tex1,
							tex->clampTex.minVal, tex->clampTex.maxVal);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BILERP_TEX
		//----------------------------------------------------------------------
		case BILERP_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex00, tex01, tex10, tex11;
					EvalStack_PopFloat(tex11);
					EvalStack_PopFloat(tex10);
					EvalStack_PopFloat(tex01);
					EvalStack_PopFloat(tex00);
					

					const float eval = BilerpTexture_ConstEvaluateFloat(hitPoint,
							tex00, tex01, tex10, tex11);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex00, tex01, tex10, tex11;
					EvalStack_PopFloat3(tex11);
					EvalStack_PopFloat3(tex10);
					EvalStack_PopFloat3(tex01);
					EvalStack_PopFloat3(tex00);

					const float3 eval = BilerpTexture_ConstEvaluateSpectrum(hitPoint,
							tex00, tex01, tex10, tex11);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// COLORDEPTH_TEX
		//----------------------------------------------------------------------
		case COLORDEPTH_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float eval = ColorDepthTexture_ConstEvaluateFloat(tex->colorDepthTex.dVal, tex1);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float3 eval = ColorDepthTexture_ConstEvaluateSpectrum(tex->colorDepthTex.dVal, tex1);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// HSV_TEX
		//----------------------------------------------------------------------
		case HSV_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, hueTex, satTex, valTex;
					EvalStack_PopFloat(valTex);
					EvalStack_PopFloat(satTex);
					EvalStack_PopFloat(hueTex);
					EvalStack_PopFloat(tex1);
					

					const float eval = HsvTexture_ConstEvaluateFloat(tex1, hueTex, satTex, valTex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					float hueTex, satTex, valTex;
					EvalStack_PopFloat(valTex);
					EvalStack_PopFloat(satTex);
					EvalStack_PopFloat(hueTex);
					EvalStack_PopFloat3(tex1);

					const float3 eval = HsvTexture_ConstEvaluateSpectrum(tex1, hueTex, satTex, valTex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// DIVIDE_TEX
		//----------------------------------------------------------------------
		case DIVIDE_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = DivideTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = DivideTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// REMAP_TEX
		//----------------------------------------------------------------------
		case REMAP_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float valueTex, sourceMinTex, sourceMaxTex, targetMinTex, targetMaxTex;
					EvalStack_PopFloat(targetMaxTex);
					EvalStack_PopFloat(targetMinTex);
					EvalStack_PopFloat(sourceMaxTex);
					EvalStack_PopFloat(sourceMinTex);
					EvalStack_PopFloat(valueTex);

					const float eval = RemapTexture_ConstEvaluateFloat(valueTex,
							sourceMinTex, sourceMaxTex, targetMinTex, targetMaxTex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 valueTex;
					float sourceMinTex, sourceMaxTex, targetMinTex, targetMaxTex;
					EvalStack_PopFloat(targetMaxTex);
					EvalStack_PopFloat(targetMinTex);
					EvalStack_PopFloat(sourceMaxTex);
					EvalStack_PopFloat(sourceMinTex);
					EvalStack_PopFloat3(valueTex);

					const float3 eval = RemapTexture_ConstEvaluateSpectrum(valueTex,
							sourceMinTex, sourceMaxTex, targetMinTex, targetMaxTex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// OBJECTID_TEX
		//----------------------------------------------------------------------
		case OBJECTID_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ObjectIDTexture_ConstEvaluateFloat(hitPoint);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ObjectIDTexture_ConstEvaluateSpectrum(hitPoint);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ConstTexture_Bump(hitPoint);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// OBJECTID_COLOR_TEX
		//----------------------------------------------------------------------
		case OBJECTID_COLOR_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ObjectIDColorTexture_ConstEvaluateFloat(hitPoint);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ObjectIDColorTexture_ConstEvaluateSpectrum(hitPoint);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ConstTexture_Bump(hitPoint);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// OBJECTID_NORMALIZED_TEX
		//----------------------------------------------------------------------
		case OBJECTID_NORMALIZED_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ObjectIDNormalizedTexture_ConstEvaluateFloat(hitPoint);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ObjectIDNormalizedTexture_ConstEvaluateSpectrum(hitPoint);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP: {
					const float3 shadeN = ConstTexture_Bump(hitPoint);
					EvalStack_PushFloat3(shadeN);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// DOT_PRODUCT_TEX
		//----------------------------------------------------------------------
		case DOT_PRODUCT_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = DotProductTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = DotProductTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// POWER_TEX
		//----------------------------------------------------------------------
		case POWER_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = PowerTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float3 eval = PowerTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// LESS_THAN_TEX
		//----------------------------------------------------------------------
		case LESS_THAN_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = LessThanTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float3 eval = LessThanTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// GREATER_THAN_TEX
		//----------------------------------------------------------------------
		case GREATER_THAN_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = GreaterThanTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float3 eval = GreaterThanTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// ROUNDING_TEX
		//----------------------------------------------------------------------
		case ROUNDING_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = RoundingTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float3 eval = RoundingTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// MODULO_TEX
		//----------------------------------------------------------------------
		case MODULO_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = ModuloTexture_ConstEvaluateFloat(tex1, tex2);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float3 eval = ModuloTexture_ConstEvaluateSpectrum(tex1, tex2);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// SHADING_NORMAL_TEX
		//----------------------------------------------------------------------
		case SHADING_NORMAL_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ShadingNormalTexture_ConstEvaluateFloat(hitPoint);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ShadingNormalTexture_ConstEvaluateSpectrum(hitPoint);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// POSITION_TEX
		//----------------------------------------------------------------------
		case POSITION_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = PositionTexture_ConstEvaluateFloat(hitPoint);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = PositionTexture_ConstEvaluateSpectrum(hitPoint);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// SPLIT_FLOAT3
		//----------------------------------------------------------------------
		case SPLIT_FLOAT3: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float eval = SplitFloat3Texture_ConstEvaluateFloat(tex1,
							tex->splitFloat3Tex.channelIndex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat(tex1);

					const float3 eval = SplitFloat3Texture_ConstEvaluateSpectrum(tex1,
							tex->splitFloat3Tex.channelIndex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// MAKE_FLOAT3
		//----------------------------------------------------------------------
		case MAKE_FLOAT3: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2, tex3;
					EvalStack_PopFloat(tex3);
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = MakeFloat3Texture_ConstEvaluateFloat(tex1,
							tex2, tex3);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1, tex2, tex3;
					EvalStack_PopFloat(tex3);
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float3 eval = MakeFloat3Texture_ConstEvaluateSpectrum(tex1,
							tex2, tex3);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BRIGHT_CONTRAST_TEX
		//----------------------------------------------------------------------
		case BRIGHT_CONTRAST_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, brightnessTex, contrastTex;
					EvalStack_PopFloat(contrastTex);
					EvalStack_PopFloat(brightnessTex);
					EvalStack_PopFloat(tex1);

					const float eval = BrightContrastTexture_ConstEvaluateFloat(tex1,
							brightnessTex, contrastTex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					float brightnessTex, contrastTex;
					EvalStack_PopFloat(contrastTex);
					EvalStack_PopFloat(brightnessTex);
					EvalStack_PopFloat3(tex1);

					const float3 eval = BrightContrastTexture_ConstEvaluateSpectrum(tex1,
							brightnessTex, contrastTex);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_BLEND
		//----------------------------------------------------------------------
		case BLENDER_BLEND: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderBlendTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderBlend.type, tex->blenderBlend.direction,
							tex->blenderBlend.contrast, tex->blenderBlend.bright,
							&tex->blenderBlend.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderBlendTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderBlend.type, tex->blenderBlend.direction,
							tex->blenderBlend.contrast, tex->blenderBlend.bright,
							&tex->blenderBlend.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_CLOUDS
		//----------------------------------------------------------------------
		case BLENDER_CLOUDS: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderCloudsTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderClouds.noisebasis, tex->blenderClouds.noisesize,
							tex->blenderClouds.noisedepth, tex->blenderClouds.contrast,
							tex->blenderClouds.bright, tex->blenderClouds.hard,
							&tex->blenderClouds.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderCloudsTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderClouds.noisebasis, tex->blenderClouds.noisesize,
							tex->blenderClouds.noisedepth, tex->blenderClouds.contrast,
							tex->blenderClouds.bright, tex->blenderClouds.hard,
							&tex->blenderClouds.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_DISTORTED_NOISE
		//----------------------------------------------------------------------
		case BLENDER_DISTORTED_NOISE: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderDistortedNoiseTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderDistortedNoise.noisedistortion, tex->blenderDistortedNoise.noisebasis,
							tex->blenderDistortedNoise.distortion, tex->blenderDistortedNoise.noisesize,
							tex->blenderDistortedNoise.contrast, tex->blenderDistortedNoise.bright,
							&tex->blenderDistortedNoise.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderDistortedNoiseTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderDistortedNoise.noisedistortion, tex->blenderDistortedNoise.noisebasis,
							tex->blenderDistortedNoise.distortion, tex->blenderDistortedNoise.noisesize,
							tex->blenderDistortedNoise.contrast, tex->blenderDistortedNoise.bright,
							&tex->blenderDistortedNoise.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_MAGIC
		//----------------------------------------------------------------------
		case BLENDER_MAGIC: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderMagicTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderMagic.noisedepth, tex->blenderMagic.turbulence,
							tex->blenderMagic.contrast, tex->blenderMagic.bright,
							&tex->blenderMagic.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderMagicTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderMagic.noisedepth, tex->blenderMagic.turbulence,
							tex->blenderMagic.contrast, tex->blenderMagic.bright,
							&tex->blenderMagic.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_MARBLE
		//----------------------------------------------------------------------
		case BLENDER_MARBLE: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderMarbleTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderMarble.type, tex->blenderMarble.noisebasis,
							tex->blenderMarble.noisebasis2, tex->blenderMarble.noisesize,
							tex->blenderMarble.turbulence, tex->blenderMarble.noisedepth,
							tex->blenderMarble.contrast, tex->blenderMarble.bright,
							tex->blenderMarble.hard,
							&tex->blenderMagic.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderMarbleTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderMarble.type, tex->blenderMarble.noisebasis,
							tex->blenderMarble.noisebasis2, tex->blenderMarble.noisesize,
							tex->blenderMarble.turbulence, tex->blenderMarble.noisedepth,
							tex->blenderMarble.contrast, tex->blenderMarble.bright,
							tex->blenderMarble.hard,
							&tex->blenderMagic.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_MUSGRAVE
		//----------------------------------------------------------------------
		case BLENDER_MUSGRAVE: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderMusgraveTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderMusgrave.type, tex->blenderMusgrave.noisebasis,
							tex->blenderMusgrave.dimension, tex->blenderMusgrave.intensity,
							tex->blenderMusgrave.lacunarity, tex->blenderMusgrave.offset,
							tex->blenderMusgrave.gain, tex->blenderMusgrave.octaves,
							tex->blenderMusgrave.noisesize, tex->blenderMusgrave.contrast,
							tex->blenderMusgrave.bright,
							&tex->blenderMusgrave.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderMusgraveTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderMusgrave.type, tex->blenderMusgrave.noisebasis,
							tex->blenderMusgrave.dimension, tex->blenderMusgrave.intensity,
							tex->blenderMusgrave.lacunarity, tex->blenderMusgrave.offset,
							tex->blenderMusgrave.gain, tex->blenderMusgrave.octaves,
							tex->blenderMusgrave.noisesize, tex->blenderMusgrave.contrast,
							tex->blenderMusgrave.bright,
							&tex->blenderMusgrave.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_NOISE
		//----------------------------------------------------------------------
		case BLENDER_NOISE: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderNoiseTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderNoise.noisedepth, tex->blenderNoise.bright,
							tex->blenderNoise.contrast);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderNoiseTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderNoise.noisedepth, tex->blenderNoise.bright,
							tex->blenderNoise.contrast);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_STUCCI
		//----------------------------------------------------------------------
		case BLENDER_STUCCI: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderStucciTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderStucci.type, tex->blenderStucci.noisebasis,
							tex->blenderStucci.noisesize, tex->blenderStucci.turbulence,
							tex->blenderStucci.contrast, tex->blenderStucci.bright,
							tex->blenderStucci.hard,
							&tex->blenderStucci.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderStucciTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderStucci.type, tex->blenderStucci.noisebasis,
							tex->blenderStucci.noisesize, tex->blenderStucci.turbulence,
							tex->blenderStucci.contrast, tex->blenderStucci.bright,
							tex->blenderStucci.hard,
							&tex->blenderStucci.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_WOOD
		//----------------------------------------------------------------------
		case BLENDER_WOOD: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderWoodTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderWood.type, tex->blenderWood.noisebasis2,
							tex->blenderWood.noisebasis, tex->blenderWood.noisesize,
							tex->blenderWood.turbulence, tex->blenderWood.contrast,
							tex->blenderWood.bright, tex->blenderWood.hard,
							&tex->blenderWood.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderWoodTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderWood.type, tex->blenderWood.noisebasis2,
							tex->blenderWood.noisebasis, tex->blenderWood.noisesize,
							tex->blenderWood.turbulence, tex->blenderWood.contrast,
							tex->blenderWood.bright, tex->blenderWood.hard,
							&tex->blenderWood.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
		//----------------------------------------------------------------------
		// BLENDER_VORONOI
		//----------------------------------------------------------------------
		case BLENDER_VORONOI: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = BlenderVoronoiTexture_ConstEvaluateFloat(hitPoint,
							tex->blenderVoronoi.distancemetric, tex->blenderVoronoi.feature_weight1,
							tex->blenderVoronoi.feature_weight2, tex->blenderVoronoi.feature_weight3,
							tex->blenderVoronoi.feature_weight4, tex->blenderVoronoi.noisesize,
							tex->blenderVoronoi.intensity, tex->blenderVoronoi.exponent,
							tex->blenderVoronoi.contrast, tex->blenderVoronoi.bright,
							&tex->blenderWood.mapping);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlenderVoronoiTexture_ConstEvaluateSpectrum(hitPoint,
							tex->blenderVoronoi.distancemetric, tex->blenderVoronoi.feature_weight1,
							tex->blenderVoronoi.feature_weight2, tex->blenderVoronoi.feature_weight3,
							tex->blenderVoronoi.feature_weight4, tex->blenderVoronoi.noisesize,
							tex->blenderVoronoi.intensity, tex->blenderVoronoi.exponent,
							tex->blenderVoronoi.contrast, tex->blenderVoronoi.bright,
							&tex->blenderWood.mapping);
					EvalStack_PushFloat3(eval);
					break;
				}
				case EVAL_BUMP_GENERIC_OFFSET_U:
					Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP_GENERIC_OFFSET_V:
					Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				case EVAL_BUMP:
					Texture_EvalOpGenericBump(evalStack, evalStackOffset,
							hitPoint, sampleDistance);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		}
// code continue in texture_eval_funcs2.cl. I have to split the string constant
// in multiple parts because VisulStudio C++ limit of 64 kbytes