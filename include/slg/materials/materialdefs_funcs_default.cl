#line 2 "material_funcs_default.cl"

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
// Default material functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// DefaultMaterial_GetInteriorVolume
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void DefaultMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);

	const uint volIndex = material->interiorVolumeIndex;

	EvalStack_PushUInt(volIndex);
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetExteriorVolume
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void DefaultMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);

	const uint volIndex = material->exteriorVolumeIndex;

	EvalStack_PushUInt(volIndex);
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetPassThroughTransparency
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void DefaultMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
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
	} else
		transp = BLACK;
	
	EvalStack_PushFloat3(transp);
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetEmittedRadiance
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void DefaultMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float oneOverPrimitiveArea;
	EvalStack_PopFloat(oneOverPrimitiveArea);

	const uint emitTexIndex = material->emitTexIndex;
	const float3 emittedRadiance = (emitTexIndex == NULL_INDEX) ?
		BLACK :
		(VLOAD3F(material->emittedFactor.c) *
		(material->usePrimitiveArea ? oneOverPrimitiveArea : 1.f) *
		clamp(Texture_GetSpectrumValue(emitTexIndex, hitPoint
				TEXTURES_PARAM), BLACK, MAKE_FLOAT3(INFINITY, INFINITY, INFINITY)));

	EvalStack_PushFloat3(emittedRadiance);
}
