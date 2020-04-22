#line 2 "texture_hitpoint_funcs.cl"

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
// HitPointColor texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float HitPointColorTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	return Spectrum_Y(HitPoint_GetColor(hitPoint, dataIndex EXTMESH_PARAM));
}

OPENCL_FORCE_INLINE float3 HitPointColorTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	return HitPoint_GetColor(hitPoint, dataIndex EXTMESH_PARAM);
}

OPENCL_FORCE_NOT_INLINE void HitPointColorTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_FLOAT: {
			const float eval = HitPointColorTexture_ConstEvaluateFloat(hitPoint,
					texture->hitPointColor.dataIndex
					TEXTURES_PARAM);
			EvalStack_PushFloat(eval);
			break;
		}
		case EVAL_SPECTRUM: {
			const float3 eval = HitPointColorTexture_ConstEvaluateSpectrum(hitPoint,
					texture->hitPointColor.dataIndex
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
}

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float HitPointAlphaTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	return HitPoint_GetAlpha(hitPoint, dataIndex EXTMESH_PARAM);
}

OPENCL_FORCE_INLINE float3 HitPointAlphaTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	const float alpha = HitPoint_GetAlpha(hitPoint, dataIndex EXTMESH_PARAM);
	return MAKE_FLOAT3(alpha, alpha, alpha);
}

OPENCL_FORCE_NOT_INLINE void HitPointAlphaTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_FLOAT: {
			const float eval = HitPointAlphaTexture_ConstEvaluateFloat(hitPoint,
					texture->hitPointAlpha.dataIndex
					TEXTURES_PARAM);
			EvalStack_PushFloat(eval);
			break;
		}
		case EVAL_SPECTRUM: {
			const float3 eval = HitPointAlphaTexture_ConstEvaluateSpectrum(hitPoint,
					texture->hitPointAlpha.dataIndex
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
}

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float HitPointGreyTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const uint dataIndex, const uint channel
		TEXTURES_PARAM_DECL) {
	const float3 col = HitPoint_GetColor(hitPoint, dataIndex EXTMESH_PARAM);

	switch (channel) {
		case 0:
			return col.x;
		case 1:
			return col.y;
		case 2:
			return col.z;
		default:
			return Spectrum_Y(col);
	}
}

OPENCL_FORCE_INLINE float3 HitPointGreyTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const uint dataIndex, const uint channel
		TEXTURES_PARAM_DECL) {
	const float3 col = HitPoint_GetColor(hitPoint, dataIndex EXTMESH_PARAM);

	float v;
	switch (channel) {
		case 0:
			v = col.x;
			break;
		case 1:
			v = col.y;
			break;
		case 2:
			v = col.z;
			break;
		default:
			v = Spectrum_Y(col);
			break;
	}

	return TO_FLOAT3(v);
}

OPENCL_FORCE_NOT_INLINE void HitPointGreyTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_FLOAT: {
			const float eval = HitPointGreyTexture_ConstEvaluateFloat(hitPoint,
					texture->hitPointGrey.dataIndex, texture->hitPointGrey.channelIndex
					TEXTURES_PARAM);
			EvalStack_PushFloat(eval);
			break;
		}
		case EVAL_SPECTRUM: {
			const float3 eval = HitPointGreyTexture_ConstEvaluateSpectrum(hitPoint,
					texture->hitPointGrey.dataIndex, texture->hitPointGrey.channelIndex
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
}

//------------------------------------------------------------------------------
// HitPointVertexAOV texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float HitPointVertexAOVTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	return HitPoint_GetVertexAOV(hitPoint, dataIndex EXTMESH_PARAM);
}

OPENCL_FORCE_INLINE float3 HitPointVertexAOVTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	const float v = HitPoint_GetVertexAOV(hitPoint, dataIndex EXTMESH_PARAM);
	return TO_FLOAT3(v);
}

OPENCL_FORCE_NOT_INLINE void HitPointVertexAOVTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_FLOAT: {
			const float eval = HitPointVertexAOVTexture_ConstEvaluateFloat(hitPoint,
					texture->hitPointVertexAOV.dataIndex
					TEXTURES_PARAM);
			EvalStack_PushFloat(eval);
			break;
		}
		case EVAL_SPECTRUM: {
			const float3 eval = HitPointVertexAOVTexture_ConstEvaluateSpectrum(hitPoint,
					texture->hitPointVertexAOV.dataIndex
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
}

//------------------------------------------------------------------------------
// HitPointTriangleAOV texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float HitPointTriangleAOVTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	return HitPoint_GetTriAOV(hitPoint, dataIndex EXTMESH_PARAM);
}

OPENCL_FORCE_INLINE float3 HitPointTriangleAOVTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const uint dataIndex
		TEXTURES_PARAM_DECL) {
	const float t = HitPoint_GetTriAOV(hitPoint, dataIndex EXTMESH_PARAM);
	return MAKE_FLOAT3(t, t, t);
}

OPENCL_FORCE_NOT_INLINE void HitPointTriangleAOVTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_FLOAT: {
			const float eval = HitPointTriangleAOVTexture_ConstEvaluateFloat(hitPoint,
					texture->hitPointTriangleAOV.dataIndex
					TEXTURES_PARAM);
			EvalStack_PushFloat(eval);
			break;
		}
		case EVAL_SPECTRUM: {
			const float3 eval = HitPointTriangleAOVTexture_ConstEvaluateSpectrum(hitPoint,
					texture->hitPointTriangleAOV.dataIndex
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
}

//------------------------------------------------------------------------------
// Shading Normal texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float ShadingNormalTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	// This method doesn't really make sense for a vector - just return the first element
	return hitPoint->shadeN.x;
}

OPENCL_FORCE_INLINE float3 ShadingNormalTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	return MAKE_FLOAT3(hitPoint->shadeN.x, hitPoint->shadeN.y, hitPoint->shadeN.z);
}

OPENCL_FORCE_NOT_INLINE void ShadingNormalTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
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
}
