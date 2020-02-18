#line 2 "materialdefs_funcs_glossycoating.cl"

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

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 albedo1;
	EvalStack_PopFloat3(albedo1);

	const float3 albedo = albedo;
	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const uint volIndex = material->interiorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	}
	// Else nothing to do because there is already the matBase volume
	// index on the stack
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const uint volIndex = material->exteriorVolumeIndex;
	if (volIndex != NULL_INDEX) {
		float passThroughEvent;
		EvalStack_PopFloat(passThroughEvent);

		EvalStack_PushUInt(volIndex);
	}
	// Else nothing to do because there is already the matBase volume
	// index on the stack
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	// Nothing to do there is already the matBase pass trough
	// transparency on the stack
}

OPENCL_FORCE_INLINE void GlossyCoatingMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	if (material->emitTexIndex != NULL_INDEX)
		DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
	// Else nothing to do because there is already the matBase emitted
	// radiance on the stack	
}
