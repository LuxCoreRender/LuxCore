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
void Material_BumpNoMix(__global const Material *material, __global HitPoint *hitPoint, const float weight
		TEXTURES_PARAM_DECL) {
	if ((material->bumpTexIndex != NULL_INDEX) && (weight > 0.f)) {
		const float2 duv = weight * 
#if defined(PARAM_ENABLE_TEX_NORMALMAP)
			((texs[material->bumpTexIndex].type == NORMALMAP_TEX) ?
				NormalMapTexture_GetDuv(material->bumpTexIndex,
					hitPoint, material->bumpSampleDistance
					TEXTURES_PARAM) :
				Texture_GetDuv(material->bumpTexIndex,
					hitPoint, material->bumpSampleDistance
					TEXTURES_PARAM));
#else
			Texture_GetDuv(material->bumpTexIndex,
				hitPoint, material->bumpSampleDistance
				TEXTURES_PARAM);
#endif

		const float3 oldShadeN = VLOAD3F(&hitPoint->shadeN.x);
		const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
		const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
		const float3 bumpDpdu = dpdu + duv.s0 * oldShadeN;
		const float3 bumpDpdv = dpdv + duv.s1 * oldShadeN;
		float3 newShadeN = normalize(cross(bumpDpdu, bumpDpdv));

		// The above transform keeps the normal in the original normal
		// hemisphere. If they are opposed, it means UVN was indirect and
		// the normal needs to be reversed
		newShadeN *= (dot(oldShadeN, newShadeN) < 0.f) ? -1.f : 1.f;

		VSTORE3F(newShadeN, &hitPoint->shadeN.x);
		VSTORE3F(bumpDpdu, &hitPoint->dpdu.x);
		VSTORE3F(bumpDpdu, &hitPoint->dpdv.x);
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

