#line 2 "material_funcs_default.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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
// DefaultMatteMaterial_IsDelta
//------------------------------------------------------------------------------

bool DefaultMaterial_IsDelta() {
	return false;
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetPassThroughTransparency
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_PASSTHROUGH)
float3 DefaultMaterial_GetPassThroughTransparency(__global const Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	const uint transpTexIndex = material->transpTexIndex;
	if (transpTexIndex == NULL_INDEX)
		return BLACK;
	else
		return Texture_GetSpectrumValue(transpTexIndex, hitPoint
				TEXTURES_PARAM);
}
#endif

//------------------------------------------------------------------------------
// DefaultMaterial_GetEmittedRadiance
//------------------------------------------------------------------------------

float3 DefaultMaterial_GetEmittedRadiance(__global const Material *material, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	const uint emitTexIndex = material->emitTexIndex;
	if (emitTexIndex == NULL_INDEX)
		return BLACK;

	return
#if defined(PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR)
		VLOAD3F(hitPoint->color.c) *
#endif
		Texture_GetSpectrumValue(emitTexIndex, hitPoint
				TEXTURES_PARAM);
}

//------------------------------------------------------------------------------
// DefaultMaterial_GetInteriorVolume
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint DefaultMaterial_GetInteriorVolume(__global const Material *material) {
	return material->interiorVolumeIndex;
}
#endif

//------------------------------------------------------------------------------
// DefaultMaterial_GetExteriorVolume
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint DefaultMaterial_GetExteriorVolume(__global const Material *material) {
	return material->exteriorVolumeIndex;
}
#endif
