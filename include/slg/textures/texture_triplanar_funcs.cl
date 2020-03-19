#line 2 "texture_triplanar_funcs.cl"

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
// Triplanar texture
//------------------------------------------------------------------------------

//----------------------------------------------------------------------
// For the very special case of Triplanar texture evaluation
//
// EVAL_TRIPLANAR_STEP_1, EVAL_TRIPLANAR_STEP_2, EVAL_TRIPLANAR_STEP_3
// and EVAL_TRIPLANAR
//----------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void TriplanarTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_TRIPLANAR_STEP_1: {
			// Save original UV
			EvalStack_PushFloat(hitPoint->defaultUV.u);
			EvalStack_PushFloat(hitPoint->defaultUV.v);

			// Compute localPoint
			float3 localShadeN;
			const float3 localPoint = TextureMapping3D_Map(&texture->triplanarTex.mapping,
					hitPoint, &localShadeN TEXTURES_PARAM);

			// Compute the 3 weights
			float weightsX = Sqr(Sqr(localShadeN.x));
			float weightsY = Sqr(Sqr(localShadeN.y));
			float weightsZ = Sqr(Sqr(localShadeN.z));

			const float sum = weightsX + weightsY + weightsZ;
			weightsX = weightsX / sum;
			weightsY = weightsY / sum;
			weightsZ = weightsZ / sum;

			// Save 3 weights	
			EvalStack_PushFloat(weightsX);
			EvalStack_PushFloat(weightsY);
			EvalStack_PushFloat(weightsZ);
			// Save localPoint
			EvalStack_PushFloat3(localPoint);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			hitPointTmp->defaultUV.u = localPoint.y;
			hitPointTmp->defaultUV.v = localPoint.z;
			break;
		}
		case EVAL_TRIPLANAR_STEP_2: {
			// Read localPoint
			//
			// Note: -3 is there to skip the result of EVAL_TRIPLANAR_STEP_1
			const float3 localPoint = EvalStack_ReadFloat3(-3 - 3);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			hitPointTmp->defaultUV.u = localPoint.x;
			hitPointTmp->defaultUV.v = localPoint.z;
			break;
		}
		case EVAL_TRIPLANAR_STEP_3: {
			// Read localPoint
			//
			// Note: -6 is there to skip the result of EVAL_TRIPLANAR_STEP_1 and EVAL_TRIPLANAR_STEP_2
			const float3 localPoint = EvalStack_ReadFloat3(-3 - 6);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			hitPointTmp->defaultUV.u = localPoint.x;
			hitPointTmp->defaultUV.v = localPoint.y;
			break;
		}
		case EVAL_FLOAT:
		case EVAL_SPECTRUM: {
			// Read textures evaluation
			float3 tex1, tex2, tex3;
			EvalStack_PopFloat3(tex3);
			EvalStack_PopFloat3(tex2);
			EvalStack_PopFloat3(tex1);

			// Read localPoint
			float3 localPoint;
			EvalStack_PopFloat3(localPoint);

			// Read 3 weights
			float weightX, weightY, weightZ;
			EvalStack_PopFloat(weightZ);
			EvalStack_PopFloat(weightY);
			EvalStack_PopFloat(weightX);

			// Restore original UV
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			EvalStack_PopFloat(hitPointTmp->defaultUV.v);
			EvalStack_PopFloat(hitPointTmp->defaultUV.u);

			const float3 result = tex1 * weightX + tex2 * weightY + tex3 * weightZ;
			if (evalType == EVAL_FLOAT) {
				EvalStack_PushFloat(Spectrum_Y(result));
			} else {
				EvalStack_PushFloat3(result);
			}
			break;
		}
		case EVAL_BUMP_TRIPLANAR_STEP_1: {
			// Save original hit point
			const float3 p = VLOAD3F(&hitPoint->p.x);
			EvalStack_PushFloat3(p);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			hitPointTmp->p.x = p.x + sampleDistance;
			hitPointTmp->p.y = p.y;
			hitPointTmp->p.z = p.z;
			break;
		}
		case EVAL_BUMP_TRIPLANAR_STEP_2: {
			// Read original hit point
			//
			// Note: -1 is there to skip the result of EVAL_BUMP_TRIPLANAR_STEP_1
			const float3 p = EvalStack_ReadFloat3(-1 - 3);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			hitPointTmp->p.x = p.x;
			hitPointTmp->p.y = p.y + sampleDistance;
			hitPointTmp->p.z = p.z;
			break;
		}
		case EVAL_BUMP_TRIPLANAR_STEP_3: {
			// Read original hit point
			//
			// Note: -2 is there to skip the result of EVAL_BUMP_TRIPLANAR_STEP_2
			const float3 p = EvalStack_ReadFloat3(-2 - 3);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			hitPointTmp->p.x = p.x;
			hitPointTmp->p.y = p.y;
			hitPointTmp->p.z = p.z + sampleDistance;
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
			if (texture->triplanarTex.enableUVlessBumpMap) {
				float evalFloatTexBase, evalFloatTexOffsetX,
						evalFloatTexOffsetY, evalFloatTexOffsetZ;

				// Read X/Y/Z textures evaluation
				EvalStack_PopFloat(evalFloatTexOffsetZ);
				EvalStack_PopFloat(evalFloatTexOffsetY);
				EvalStack_PopFloat(evalFloatTexOffsetX);

				// Read and restore original hit point
				float3 p;
				EvalStack_PopFloat3(p);
				__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
				VSTORE3F(p, &hitPointTmp->p.x);

				// Read base textures evaluation
				EvalStack_PopFloat(evalFloatTexBase);

				const float3 shadeN = TriplanarTexture_BumpUVLess(hitPoint,
						sampleDistance, evalFloatTexBase, evalFloatTexOffsetX,
						evalFloatTexOffsetY, evalFloatTexOffsetZ);
				EvalStack_PushFloat3(shadeN);
			} else
				Texture_EvalOpGenericBump(evalStack, evalStackOffset,
						hitPoint, sampleDistance);
			break;
		default:
			// Something wrong here
			break;
	}
}
