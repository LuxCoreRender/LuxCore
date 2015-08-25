#line 2 "material_funcs.cl"

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
// Generic material functions
//
// They include the support for all material but Mix
// (because OpenCL doesn't support recursion)
//------------------------------------------------------------------------------

float3 Material_GetEmittedRadianceNoMix(__global const Material *material, __global HitPoint *hitPoint
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

#if defined(PARAM_HAS_BUMPMAPS)
void Material_BumpNoMix(__global const Material *material, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	if (material->bumpTexIndex != NULL_INDEX) {
#if defined(PARAM_ENABLE_TEX_NORMALMAP)
		if (texs[material->bumpTexIndex].type == NORMALMAP_TEX)
			NormalMapTexture_Bump(material->bumpTexIndex,
					hitPoint, material->bumpSampleDistance
					TEXTURES_PARAM);
		else
#endif
			Texture_Bump(material->bumpTexIndex,
					hitPoint, material->bumpSampleDistance
					TEXTURES_PARAM);
	}
}
#endif

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetInteriorVolumeNoMix(__global const Material *material) {
	return material->interiorVolumeIndex;
}

uint Material_GetExteriorVolumeNoMix(__global const Material *material) {
	return material->exteriorVolumeIndex;
}
#endif
