#line 2 "material_main_withoutdynamic.cl"

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
// They include the support for all material but one requiring dynamic code
// generation like Mix (because OpenCL doesn't support recursion)
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Material_GetEventTypes
//------------------------------------------------------------------------------

BSDFEvent Material_GetEventTypesWithoutDynamic(__global const Material *material) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
			return Metal2Material_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)
		case ROUGHGLASS:
			return RoughGlassMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_VELVET)
		case VELVET:
			return VelvetMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_CLOTH)
		case CLOTH:
			return ClothMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_CARPAINT)
		case CARPAINT:
			return CarPaintMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)
		case ROUGHMATTE:
			return RoughMatteMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT)
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT)
		case GLOSSYTRANSLUCENT:
			return GlossyTranslucentMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_GetEventTypes();
#endif
		default:
			// Something has gone very wrong
			return DIFFUSE | REFLECT;
	}
}

//------------------------------------------------------------------------------
// Material_IsDeltaWithoutDynamic
//------------------------------------------------------------------------------

bool Material_IsDeltaWithoutDynamic(__global const Material *material) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_IsDelta();
#endif
		default:
			return DefaultMaterial_IsDelta();
	}
}

//------------------------------------------------------------------------------
// Material_GetPassThroughTransparencyWithoutDynamic
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Material_GetPassThroughTransparencyWithoutDynamic(__global const Material *material, __global HitPoint *hitPoint,
		const float3 localFixedDir, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_GetPassThroughTransparency(material, hitPoint, localFixedDir, passThroughEvent TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_GetPassThroughTransparency(material, hitPoint, localFixedDir, passThroughEvent TEXTURES_PARAM);
#endif
		default:
			return DefaultMaterial_GetPassThroughTransparency(material, hitPoint, localFixedDir, passThroughEvent TEXTURES_PARAM);
	}
}
#endif

//------------------------------------------------------------------------------
// Material_GetEmittedRadianceWithoutDynamic
//------------------------------------------------------------------------------

float3 Material_GetEmittedRadianceWithoutDynamic(__global const Material *material, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	return DefaultMaterial_GetEmittedRadiance(material, hitPoint
		TEXTURES_PARAM);
}

//------------------------------------------------------------------------------
// Material_GetInteriorVolumeWithoutDynamic
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetInteriorVolumeWithoutDynamic(__global const Material *material) {
	return DefaultMaterial_GetInteriorVolume(material);
}
#endif

//------------------------------------------------------------------------------
// Material_GetExteriorVolumeWithoutDynamic
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetExteriorVolumeWithoutDynamic(__global const Material *material) {
	return DefaultMaterial_GetExteriorVolume(material);
}
#endif
