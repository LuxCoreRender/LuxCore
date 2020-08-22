#line 2 "texture_distort_funcs.cl"

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
// Distort texture
//------------------------------------------------------------------------------

//----------------------------------------------------------------------
// For the very special case of Distort texture evaluation
//
// EVAL_DISTORT_SETUP
//----------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void DistortTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_DISTORT_SETUP: {
			// Read texture evaluation
			float3 offsetColor;
			EvalStack_PopFloat3(offsetColor);

			// Save original P
			float3 p = VLOAD3F(&hitPoint->p.x);
			EvalStack_PushFloat3(p);

			// Save original UV
			float2 uv = VLOAD2F(&hitPoint->defaultUV.u);
			EvalStack_PushFloat(uv.x);
			EvalStack_PushFloat(uv.y);

			// Distort HitPoint P and UV
			__global HitPoint *tmpHitPoint = (__global HitPoint *)hitPoint;

			const float3 offset = offsetColor * texture->distortTex.strength;
			p += offset;
			VSTORE3F(p, &tmpHitPoint->p.x);

			uv.x += offset.x;
			uv.y += offset.y;
			VSTORE2F(uv, &tmpHitPoint->defaultUV.u);
			break;
		}
		case EVAL_FLOAT:
		case EVAL_SPECTRUM: {
			// Read textures evaluation
			float3 result;
			EvalStack_PopFloat3(result);

			// Restore original P and UV
			__global HitPoint *tmpHitPoint = (__global HitPoint *)hitPoint;
			float2 uv;
			EvalStack_PopFloat2(uv);
			VSTORE2F(uv, &tmpHitPoint->defaultUV.u);

			float3 p;
			EvalStack_PopFloat3(p);
			VSTORE3F(p, &tmpHitPoint->p.x);

			// Save the result
			if (evalType == EVAL_FLOAT) {
				EvalStack_PushFloat(Spectrum_Y(result));
			} else {
				EvalStack_PushFloat3(result);
			}
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
}
