#line 2 "texture_funcs_evalops.cl"

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

// NOTE: Keep in mind this file must be less than 64 Kbytes because
// VisulStudio C++ limit of 64 kbytes per file sources

//#define DEBUG_PRINTF_KERNEL_NAME 1
//#define DEBUG_PRINTF_TEXTURE_EVAL 1

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
	__global const Texture* restrict texture = &texs[evalOp->texIndex];

#if defined(DEBUG_PRINTF_TEXTURE_EVAL)
	printf("EvalOp texture index=%d type=%d evalType=%d *evalStackOffset=%d\n", evalOp->texIndex, texture->type, evalOp->evalType, *evalStackOffset);
#endif

	const TextureEvalOpType evalType = evalOp->evalType;
	switch (texture->type) {
		//----------------------------------------------------------------------
		// CONST_FLOAT
		//----------------------------------------------------------------------
		case CONST_FLOAT: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = ConstFloatTexture_ConstEvaluateFloat(texture);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ConstFloatTexture_ConstEvaluateSpectrum(texture);
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
					const float eval = ConstFloat3Texture_ConstEvaluateFloat(texture);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = ConstFloat3Texture_ConstEvaluateSpectrum(texture);
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
			ImageMapTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
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

					const float3 eval = MixTexture_ConstEvaluateSpectrum(tex1, tex2, MAKE_FLOAT3(amt, amt, amt));
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
		case HITPOINTCOLOR:
			HitPointColorTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// HITPOINTALPHA
		//----------------------------------------------------------------------
		case HITPOINTALPHA:
			HitPointAlphaTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// HITPOINTGREY
		//----------------------------------------------------------------------
		case HITPOINTGREY:
			HitPointGreyTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// HITPOINTVERTEXAOV
		//----------------------------------------------------------------------
		case HITPOINTVERTEXAOV:
			HitPointVertexAOVTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// HITPOINTTRIANGLEAOV
		//----------------------------------------------------------------------
		case HITPOINTTRIANGLEAOV:
			HitPointTriangleAOVTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
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

					const float3 shadeN = NormalMapTexture_Bump(texture, hitPoint, evalSpectrumTex);
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
					const float eval = BlackBodyTexture_ConstEvaluateFloat(VLOAD3F(texture->blackBody.rgb.c));
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = BlackBodyTexture_ConstEvaluateSpectrum(VLOAD3F(texture->blackBody.rgb.c));
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
		// IRREGULARDATA_TEX
		//----------------------------------------------------------------------
		case IRREGULARDATA_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = IrregularDataTexture_ConstEvaluateFloat(VLOAD3F(texture->irregularData.rgb.c));
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = IrregularDataTexture_ConstEvaluateSpectrum(VLOAD3F(texture->irregularData.rgb.c));
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
		// DENSITYGRID_TEX
		//----------------------------------------------------------------------
		case DENSITYGRID_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = DensityGridTexture_ConstEvaluateFloat(hitPoint,
							texture->densityGrid.nx, texture->densityGrid.ny, texture->densityGrid.nz,
							texture->densityGrid.imageMapIndex, &texture->densityGrid.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = DensityGridTexture_ConstEvaluateSpectrum(hitPoint,
							texture->densityGrid.nx, texture->densityGrid.ny, texture->densityGrid.nz,
							texture->densityGrid.imageMapIndex, &texture->densityGrid.mapping
							TEXTURES_PARAM);
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
		case ABS_TEX:
			AbsTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// CLAMP_TEX
		//----------------------------------------------------------------------
		case CLAMP_TEX:
			ClampTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
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

					const float eval = ColorDepthTexture_ConstEvaluateFloat(texture->colorDepthTex.dVal, tex1);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float3 eval = ColorDepthTexture_ConstEvaluateSpectrum(texture->colorDepthTex.dVal, tex1);
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
					float3 tex1;
					float hueTex, satTex, valTex;
					EvalStack_PopFloat(valTex);
					EvalStack_PopFloat(satTex);
					EvalStack_PopFloat(hueTex);
					EvalStack_PopFloat3(tex1);
					

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
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

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
		case SHADING_NORMAL_TEX:
			ShadingNormalTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
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
							texture->splitFloat3Tex.channelIndex);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1;
					EvalStack_PopFloat3(tex1);

					const float3 eval = SplitFloat3Texture_ConstEvaluateSpectrum(tex1,
							texture->splitFloat3Tex.channelIndex);
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
		case BLENDER_BLEND:
			BlenderBlendTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_CLOUDS
		//----------------------------------------------------------------------
		case BLENDER_CLOUDS:
			BlenderCloudsTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_DISTORTED_NOISE
		//----------------------------------------------------------------------
		case BLENDER_DISTORTED_NOISE:
			BlenderDistortedNoiseTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_MAGIC
		//----------------------------------------------------------------------
		case BLENDER_MAGIC:
			BlenderMagicTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_MARBLE
		//----------------------------------------------------------------------
		case BLENDER_MARBLE:
			BlenderMarbleTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_MUSGRAVE
		//----------------------------------------------------------------------
		case BLENDER_MUSGRAVE:
			BlenderMusgraveTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_NOISE
		//----------------------------------------------------------------------
		case BLENDER_NOISE:
			BlenderNoiseTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_STUCCI
		//----------------------------------------------------------------------
		case BLENDER_STUCCI:
			BlenderStucciTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_WOOD
		//----------------------------------------------------------------------
		case BLENDER_WOOD:
			BlenderWoodTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BLENDER_VORONOI
		//----------------------------------------------------------------------
		case BLENDER_VORONOI:
			BlenderVoronoiTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// CHECKERBOARD2D
		//----------------------------------------------------------------------
		case CHECKERBOARD2D: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = CheckerBoard2DTexture_ConstEvaluateFloat(hitPoint,
							tex1, tex2, &texture->checkerBoard2D.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = CheckerBoard2DTexture_ConstEvaluateSpectrum(hitPoint,
							tex1, tex2, &texture->checkerBoard2D.mapping
							TEXTURES_PARAM);
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
		// CHECKERBOARD3D
		//----------------------------------------------------------------------
		case CHECKERBOARD3D: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = CheckerBoard3DTexture_ConstEvaluateFloat(hitPoint,
							tex1, tex2, &texture->checkerBoard3D.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = CheckerBoard3DTexture_ConstEvaluateSpectrum(hitPoint,
							tex1, tex2, &texture->checkerBoard3D.mapping
							TEXTURES_PARAM);
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
		// CLOUD_TEX
		//----------------------------------------------------------------------
		case CLOUD_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = CloudTexture_ConstEvaluateFloat(hitPoint,
							texture->cloud.radius, texture->cloud.numspheres,
							texture->cloud.spheresize, texture->cloud.sharpness,
							texture->cloud.basefadedistance, texture->cloud.baseflatness,
							texture->cloud.variability, texture->cloud.omega,
							texture->cloud.noisescale, texture->cloud.noiseoffset,
							texture->cloud.turbulence, texture->cloud.octaves,
							&texture->cloud.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = CloudTexture_ConstEvaluateSpectrum(hitPoint,
							texture->cloud.radius, texture->cloud.numspheres,
							texture->cloud.spheresize, texture->cloud.sharpness,
							texture->cloud.basefadedistance, texture->cloud.baseflatness,
							texture->cloud.variability, texture->cloud.omega,
							texture->cloud.noisescale, texture->cloud.noiseoffset,
							texture->cloud.turbulence, texture->cloud.octaves,
							&texture->cloud.mapping
							TEXTURES_PARAM);
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
		// FBM_TEX
		//----------------------------------------------------------------------
		case FBM_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = FBMTexture_ConstEvaluateFloat(hitPoint,
							texture->fbm.omega, texture->fbm.octaves,
							&texture->fbm.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = FBMTexture_ConstEvaluateSpectrum(hitPoint,
							texture->fbm.omega, texture->fbm.octaves,
							&texture->fbm.mapping
							TEXTURES_PARAM);
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
		// MARBLE
		//----------------------------------------------------------------------
		case MARBLE: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = MarbleTexture_ConstEvaluateFloat(hitPoint,
							texture->marble.scale, texture->marble.omega,
							texture->marble.octaves, texture->marble.variation,
							&texture->marble.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = MarbleTexture_ConstEvaluateSpectrum(hitPoint,
							texture->marble.scale, texture->marble.omega,
							texture->marble.octaves, texture->marble.variation,
							&texture->marble.mapping
							TEXTURES_PARAM);
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
		// DOTS
		//----------------------------------------------------------------------
		case DOTS: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = DotsTexture_ConstEvaluateFloat(hitPoint,
							tex1, tex2, &texture->checkerBoard2D.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = DotsTexture_ConstEvaluateSpectrum(hitPoint,
							tex1, tex2, &texture->checkerBoard2D.mapping
							TEXTURES_PARAM);
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
		// BRICK
		//----------------------------------------------------------------------
		case BRICK:
			BrickTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// WINDY
		//----------------------------------------------------------------------
		case WINDY: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = WindyTexture_ConstEvaluateFloat(hitPoint,
							&texture->windy.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = WindyTexture_ConstEvaluateSpectrum(hitPoint,
							&texture->windy.mapping
							TEXTURES_PARAM);
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
		// WRINKLED
		//----------------------------------------------------------------------
		case WRINKLED: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = WrinkledTexture_ConstEvaluateFloat(hitPoint,
							texture->wrinkled.omega, texture->wrinkled.octaves,
							&texture->wrinkled.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = WrinkledTexture_ConstEvaluateSpectrum(hitPoint,
							texture->wrinkled.omega, texture->wrinkled.octaves,
							&texture->wrinkled.mapping
							TEXTURES_PARAM);
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
		// UV_TEX
		//----------------------------------------------------------------------
		case UV_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = UVTexture_ConstEvaluateFloat(hitPoint,
							&texture->uvTex.mapping
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = UVTexture_ConstEvaluateSpectrum(hitPoint,
							&texture->uvTex.mapping
							TEXTURES_PARAM);
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
		// BAND_TEX
		//----------------------------------------------------------------------
		case BAND_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float eval = BandTexture_ConstEvaluateFloat(hitPoint,
							texture->band.interpType, texture->band.size,
							texture->band.offsets, texture->band.values,
							tex1);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float tex1;
					EvalStack_PopFloat(tex1);

					const float3 eval = BandTexture_ConstEvaluateSpectrum(hitPoint,
							texture->band.interpType, texture->band.size,
							texture->band.offsets, texture->band.values,
							tex1);
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
		// WIREFRAME_TEX
		//----------------------------------------------------------------------
		case WIREFRAME_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					float tex1, tex2;
					EvalStack_PopFloat(tex2);
					EvalStack_PopFloat(tex1);

					const float eval = WireFrameTexture_ConstEvaluateFloat(hitPoint,
							texture->wireFrameTex.width,
							tex1, tex2
							TEXTURES_PARAM);
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					float3 tex1, tex2;
					EvalStack_PopFloat3(tex2);
					EvalStack_PopFloat3(tex1);

					const float3 eval = WireFrameTexture_ConstEvaluateSpectrum(hitPoint,
							texture->wireFrameTex.width,
							tex1, tex2
							TEXTURES_PARAM);
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
		// TRIPLANAR_TEX
		//----------------------------------------------------------------------
		case TRIPLANAR_TEX:
			TriplanarTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// DISTORT_TEX
		//----------------------------------------------------------------------
		case DISTORT_TEX:
			DistortTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// BOMBING_TEX
		//----------------------------------------------------------------------
		case BOMBING_TEX:
			BombingTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		// FRESNELCOLOR_TEX
		//----------------------------------------------------------------------
		case FRESNELCOLOR_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = FresnelColorTexture_ConstEvaluateFloat();
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = FresnelColorTexture_ConstEvaluateSpectrum();
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
		// FRESNELCONST_TEX
		//----------------------------------------------------------------------
		case FRESNELCONST_TEX: {
			switch (evalType) {
				case EVAL_FLOAT: {
					const float eval = FresnelColorTexture_ConstEvaluateFloat();
					EvalStack_PushFloat(eval);
					break;
				}
				case EVAL_SPECTRUM: {
					const float3 eval = FresnelColorTexture_ConstEvaluateSpectrum();
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
		// RANDOM_TEX
		//----------------------------------------------------------------------
		case RANDOM_TEX:
			RandomTexture_EvalOp(texture, evalType, evalStack, evalStackOffset,
					hitPoint, sampleDistance TEXTURES_PARAM);
			break;
		//----------------------------------------------------------------------
		default:
			// Something wrong here
			break;
	}
}
