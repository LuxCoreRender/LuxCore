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

OPENCL_FORCE_INLINE uint DefaultMaterial_GetInteriorVolume(__global const Material *material) {
	return material->interiorVolumeIndex;
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetExteriorVolume
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE uint DefaultMaterial_GetExteriorVolume(__global const Material *material) {
	return material->exteriorVolumeIndex;
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetPassThroughTransparency
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 DefaultMaterial_GetPassThroughTransparency(__global const Material *material,
		__global const HitPoint *hitPoint, const float3 localFixedDir,
		const float passThroughEvent, const bool backTracing
		TEXTURES_PARAM_DECL) {
	const uint transpTexIndex = (hitPoint->intoObject != backTracing) ?
		material->frontTranspTexIndex : material->backTranspTexIndex;

	if (transpTexIndex != NULL_INDEX) {
		const float weight = clamp(
				Texture_GetFloatValue(transpTexIndex, hitPoint
					TEXTURES_PARAM),
				0.f, 1.f);

		return (passThroughEvent > weight) ? WHITE : BLACK;
	} else
		return BLACK;
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetEmittedRadiance
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 DefaultMaterial_GetEmittedRadiance(__global const Material *material,
		__global const HitPoint *hitPoint, const float oneOverPrimitiveArea
		TEXTURES_PARAM_DECL) {
	const uint emitTexIndex = material->emitTexIndex;
	if (emitTexIndex == NULL_INDEX)
		return BLACK;

	return
		VLOAD3F(hitPoint->color[0].c) *
		VLOAD3F(material->emittedFactor.c) *
		(material->usePrimitiveArea ? oneOverPrimitiveArea : 1.f) *
		clamp(Texture_GetSpectrumValue(emitTexIndex, hitPoint
				TEXTURES_PARAM), BLACK, INFINITY);
}
