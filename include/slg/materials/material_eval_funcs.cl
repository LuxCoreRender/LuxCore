#line 2 "material_eval_funcs.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the License);         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an AS IS BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

//#define DEBUG_PRINTF_KERNEL_NAME 1
//#define DEBUG_PRINTF_MATERIAL_EVAL 1

//------------------------------------------------------------------------------
// Material_GetEventTypes
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE BSDFEvent Material_GetEventTypes(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	return material->eventTypes;
}

//------------------------------------------------------------------------------
// Material_IsDynamic
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE bool Material_IsDynamic(__global const Material *mat) {
		return (mat->type == MIX) || (mat->type == GLOSSYCOATING);
}

//------------------------------------------------------------------------------
// Material evaluation functions
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void Material_EvalOp(
		__global const MaterialEvalOp* restrict evalOp,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	const uint matIndex = evalOp->matIndex;
	__global const Material* restrict mat = &mats[matIndex];

#if defined(DEBUG_PRINTF_MATERIAL_EVAL)
	printf("EvalOp mat index=%d type=%d evalType=%d *evalStackOffset=%d\n", evalOp->matIndex, mat->type, evalOp->evalType, *evalStackOffset);
#endif

	const MaterialEvalOpType evalType = evalOp->evalType;
	switch (mat->type) {
		//----------------------------------------------------------------------
		// MATTE
		//----------------------------------------------------------------------
		case MATTE:
			switch (evalType) {
				case EVAL_ALBEDO:
					MatteMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					MatteMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					MatteMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					MatteMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					MatteMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// MATTETRANSLUCENT
		//----------------------------------------------------------------------
		case MATTETRANSLUCENT:
			switch (evalType) {
				case EVAL_ALBEDO:
					MatteTranslucentMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					MatteTranslucentMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					MatteTranslucentMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					MatteTranslucentMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					MatteTranslucentMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// GLOSSY2
		//----------------------------------------------------------------------
		case GLOSSY2:
			switch (evalType) {
				case EVAL_ALBEDO:
					Glossy2Material_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					Glossy2Material_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					Glossy2Material_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					Glossy2Material_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					Glossy2Material_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;			
		//----------------------------------------------------------------------
		// METAL2
		//----------------------------------------------------------------------
		case METAL2:
			switch (evalType) {
				case EVAL_ALBEDO:
					Metal2Material_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					Metal2Material_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					Metal2Material_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					Metal2Material_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					Metal2Material_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;			
		//----------------------------------------------------------------------
		// VELVET
		//----------------------------------------------------------------------
		case VELVET:
			switch (evalType) {
				case EVAL_ALBEDO:
					VelvetMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					VelvetMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					VelvetMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					VelvetMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					VelvetMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;	
		//----------------------------------------------------------------------
		// CLOTH
		//----------------------------------------------------------------------
		case CLOTH:
			switch (evalType) {
				case EVAL_ALBEDO:
					ClothMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					ClothMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					ClothMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					ClothMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					ClothMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// CARPAINT
		//----------------------------------------------------------------------
		case CARPAINT:
			switch (evalType) {
				case EVAL_ALBEDO:
					CarPaintMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					CarPaintMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					CarPaintMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					CarPaintMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					CarPaintMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// ROUGHMATTE
		//----------------------------------------------------------------------
		case ROUGHMATTE:
			switch (evalType) {
				case EVAL_ALBEDO:
					RoughMatteMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					RoughMatteMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					RoughMatteMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					RoughMatteMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					RoughMatteMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// ROUGHMATTETRANSLUCENT
		//----------------------------------------------------------------------
		case ROUGHMATTETRANSLUCENT:
			switch (evalType) {
				case EVAL_ALBEDO:
					RoughMatteTranslucentMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					RoughMatteTranslucentMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					RoughMatteTranslucentMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					RoughMatteTranslucentMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					RoughMatteTranslucentMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// GLOSSYTRANSLUCENT
		//----------------------------------------------------------------------
		case GLOSSYTRANSLUCENT:
			switch (evalType) {
				case EVAL_ALBEDO:
					GlossyTranslucentMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					GlossyTranslucentMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					GlossyTranslucentMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					GlossyTranslucentMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					GlossyTranslucentMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// DISNEY
		//----------------------------------------------------------------------
		case DISNEY:
			switch (evalType) {
				case EVAL_ALBEDO:
					DisneyMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					DisneyMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					DisneyMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					DisneyMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					DisneyMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// NULLMAT
		//----------------------------------------------------------------------
		case NULLMAT:
			switch (evalType) {
				case EVAL_ALBEDO:
					NullMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					NullMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					NullMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					NullMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					NullMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// MIX
		//----------------------------------------------------------------------
		case MIX:
			switch (evalType) {
				case EVAL_ALBEDO:
					MixMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_VOLUME_MIX_SETUP1:
					MixMaterial_GetVolumeSetUp1(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_VOLUME_MIX_SETUP2:
					MixMaterial_GetVolumeSetUp2(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					MixMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					MixMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE_MIX_SETUP1:
					MixMaterial_GetEmittedRadianceSetUp1(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE_MIX_SETUP2:
					MixMaterial_GetEmittedRadianceSetUp2(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					MixMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP1:
					MixMaterial_GetPassThroughTransparencySetUp1(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP2:
					MixMaterial_GetPassThroughTransparencySetUp2(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					MixMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// GLOSSYCOATING
		//----------------------------------------------------------------------
		case GLOSSYCOATING:
			switch (evalType) {
				case EVAL_ALBEDO:
					GlossyCoatingMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					GlossyCoatingMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					GlossyCoatingMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					GlossyCoatingMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					GlossyCoatingMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// MIRROR
		//----------------------------------------------------------------------
		case MIRROR:
			switch (evalType) {
				case EVAL_ALBEDO:
					MirrorMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					MirrorMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					MirrorMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					MirrorMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					MirrorMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// GLASS
		//----------------------------------------------------------------------
		case GLASS:
			switch (evalType) {
				case EVAL_ALBEDO:
					GlassMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					GlassMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					GlassMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					GlassMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					GlassMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// ARCHGLASS
		//----------------------------------------------------------------------
		case ARCHGLASS:
			switch (evalType) {
				case EVAL_ALBEDO:
					ArchGlassMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					ArchGlassMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					ArchGlassMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					ArchGlassMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					ArchGlassMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// ROUGHGLASS
		//----------------------------------------------------------------------
		case ROUGHGLASS:
			switch (evalType) {
				case EVAL_ALBEDO:
					RoughGlassMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					RoughGlassMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					RoughGlassMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					RoughGlassMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					RoughGlassMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// CLEAR_VOL
		//----------------------------------------------------------------------
		case CLEAR_VOL:
			switch (evalType) {
				case EVAL_ALBEDO:
					ClearVolMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					ClearVolMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					ClearVolMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					ClearVolMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					ClearVolMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// HOMOGENEOUS_VOL
		//----------------------------------------------------------------------
		case HOMOGENEOUS_VOL:
			switch (evalType) {
				case EVAL_ALBEDO:
					HomogeneousVolMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					HomogeneousVolMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					HomogeneousVolMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					HomogeneousVolMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					HomogeneousVolMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		//----------------------------------------------------------------------
		// HETEROGENEOUS_VOL
		//----------------------------------------------------------------------
		case HETEROGENEOUS_VOL:
			switch (evalType) {
				case EVAL_ALBEDO:
					HeterogeneousVolMaterial_Albedo(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_INTERIOR_VOLUME:
					HeterogeneousVolMaterial_GetInteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EXTERIOR_VOLUME:
					HeterogeneousVolMaterial_GetExteriorVolume(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_EMITTED_RADIANCE:
					HeterogeneousVolMaterial_GetEmittedRadiance(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					HeterogeneousVolMaterial_GetPassThroughTransparency(mat, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
					break;
				default:
					// Something wrong here
					break;
			}
			break;
		default:
			// Something wrong here
			break;
	}
}
