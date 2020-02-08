#line 2 "materialdefs_funcs_mix.cl"

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

OPENCL_FORCE_INLINE void MixMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	float3 albedo1, albedo2;
	EvalStack_PopFloat3(albedo2);
	EvalStack_PopFloat3(albedo1);

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float3 albedo =  weight1 * albedo1 + weight2 * albedo2;

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void MixMaterial_GetVolumeSetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float passThroughEvent1 = passThroughEvent / weight1;

	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushFloat(factor);
	EvalStack_PushFloat(passThroughEvent1);
}

OPENCL_FORCE_INLINE void MixMaterial_GetVolumeSetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	uint volIndex1;
	EvalStack_PopUInt(volIndex1);

	float factor;
	EvalStack_PopFloat(factor);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);

	const float passThroughEvent2 = (passThroughEvent - weight1) / weight2;

	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushFloat(factor);
	EvalStack_PushUInt(volIndex1);
	EvalStack_PushFloat(passThroughEvent2);
}

OPENCL_FORCE_INLINE void MixMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	const uint volIndex = material->interiorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	} else {
		uint volIndex1, volIndex2;
		EvalStack_PopUInt(volIndex2);
		EvalStack_PopUInt(volIndex1);

		float factor;
		EvalStack_PopFloat(factor);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		const uint volIndex = (passThroughEvent < weight1) ? volIndex1 : volIndex2;
		EvalStack_PushUInt(volIndex);
	}
}

OPENCL_FORCE_INLINE void MixMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	const uint volIndex = material->exteriorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	} else {
		uint volIndex1, volIndex2;
		EvalStack_PopUInt(volIndex2);
		EvalStack_PopUInt(volIndex1);

		float factor;
		EvalStack_PopFloat(factor);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		const uint volIndex = (passThroughEvent < weight1) ? volIndex1 : volIndex2;
		EvalStack_PushUInt(volIndex);
	}
}

OPENCL_FORCE_INLINE void MixMaterial_GetPassThroughTransparencySetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	bool backTracing;
	EvalStack_PopUInt(backTracing);
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	float3 localFixedDir;
	EvalStack_PopFloat3(localFixedDir);

	// Save the parameters
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushUInt(backTracing);

	const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float passThroughEvent1 = passThroughEvent / weight1;

	EvalStack_PushFloat(factor);

	// To setup the following EVAL_GET_PASS_TROUGH_TRANSPARENCY evaluation of first sub-nodes
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent1);
	EvalStack_PushUInt(backTracing);
}

OPENCL_FORCE_INLINE void MixMaterial_GetPassThroughTransparencySetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	float3 transp1;
	EvalStack_PopFloat3(transp1);

	float factor;
	EvalStack_PopFloat(factor);

	bool backTracing;
	EvalStack_PopUInt(backTracing);
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	float3 localFixedDir;
	EvalStack_PopFloat3(localFixedDir);

	const float weight2 = clamp(factor, 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const float passThroughEvent2 = (passThroughEvent - weight1) / weight2;

	// Save the parameters
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent);
	EvalStack_PushUInt(backTracing);

	EvalStack_PushFloat(factor);

	EvalStack_PushFloat3(transp1);

	// To setup the following EVAL_GET_PASS_TROUGH_TRANSPARENCY evaluation of second sub-nodes
	EvalStack_PushFloat3(localFixedDir);
	EvalStack_PushFloat(passThroughEvent2);
	EvalStack_PushUInt(backTracing);
}

OPENCL_FORCE_INLINE void MixMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	float3 transp1, transp2;
	EvalStack_PopFloat3(transp2);
	EvalStack_PopFloat3(transp1);

	float factor;
	EvalStack_PopFloat(factor);

	bool backTracing;
	EvalStack_PopUInt(backTracing);
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	float3 localFixedDir;
	EvalStack_PopFloat3(localFixedDir);

	const uint transpTexIndex = (hitPoint->intoObject != backTracing) ?
		material->frontTranspTexIndex : material->backTranspTexIndex;

	float3 transp;
	if (transpTexIndex != NULL_INDEX) {
		const float weight = clamp(
				Texture_GetFloatValue(transpTexIndex, hitPoint TEXTURES_PARAM),
				0.f, 1.f);

		transp = (passThroughEvent > weight) ? WHITE : BLACK;
	} else {
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		transp = (passThroughEvent < weight1) ? transp1 : transp2;
	}

	EvalStack_PushFloat3(transp);
}

OPENCL_FORCE_INLINE void MixMaterial_GetEmittedRadianceSetUp1(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	float oneOverPrimitiveArea;
	EvalStack_PopFloat(oneOverPrimitiveArea);

	EvalStack_PushFloat(oneOverPrimitiveArea);
	// To setup the following EVAL_GET_EMITTED_RADIANCE evaluation of first sub-nodes
	EvalStack_PushFloat(oneOverPrimitiveArea);
}

OPENCL_FORCE_INLINE void MixMaterial_GetEmittedRadianceSetUp2(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	float3 emit1;
	EvalStack_PopFloat3(emit1);

	float oneOverPrimitiveArea;
	EvalStack_PopFloat(oneOverPrimitiveArea);

	EvalStack_PushFloat(oneOverPrimitiveArea);
	EvalStack_PushFloat3(emit1);
	// To setup the following EVAL_GET_EMITTED_RADIANCE evaluation of second sub-nodes
	EvalStack_PushFloat(oneOverPrimitiveArea);
}

OPENCL_FORCE_INLINE void MixMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	float3 emittedRadiance;
	if (material->emitTexIndex != NULL_INDEX)
		 DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
	else {
		float3 emit1, emit2;
		EvalStack_PopFloat3(emit2);
		EvalStack_PopFloat3(emit1);

		float oneOverPrimitiveArea;
		EvalStack_PopFloat(oneOverPrimitiveArea);

		const float factor = Texture_GetFloatValue(material->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		emittedRadiance = BLACK;
		if (weight1 > 0.f)
			emittedRadiance += weight1 * emit1;
		 if (weight2 > 0.f)
			emittedRadiance += weight2 * emit2;

		EvalStack_PushFloat3(emittedRadiance);
	}
}
