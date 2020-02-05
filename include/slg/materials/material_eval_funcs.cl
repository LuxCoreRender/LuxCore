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

// All EvalStack_* macros are defined in texture_eval_funcs.cl

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
				case EVAL_ALBEDO: {
					const float3 albedo = Texture_GetSpectrumValue(mat->matte.kdTexIndex, hitPoint TEXTURES_PARAM);
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = MatteTranslucentMaterial_Albedo(
							Texture_GetSpectrumValue(mat->matteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
							Texture_GetSpectrumValue(mat->matteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = Glossy2Material_Albedo(Texture_GetSpectrumValue(mat->glossy2.kdTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
				default:
					// Something wrong here
					break;
			}
			break;			
		//----------------------------------------------------------------------
		// GLOSSY2
		//----------------------------------------------------------------------
		case METAL2:
			switch (evalType) {
				case EVAL_ALBEDO: {
					float3 n, k;
					Metal2Material_GetNK(mat, hitPoint,
							&n, &k
							TEXTURES_PARAM);

					const float3 albedo = Metal2Material_Albedo(n, k);
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = VelvetMaterial_Albedo(Texture_GetSpectrumValue(mat->velvet.kdTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = ClothMaterial_Albedo(
							hitPoint->uv[0].u, hitPoint->uv[0].v,
							mat->cloth.Preset,
							mat->cloth.Repeat_U,
							mat->cloth.Repeat_V,
							mat->cloth.specularNormalization,
							Texture_GetSpectrumValue(mat->cloth.Warp_KdIndex, hitPoint TEXTURES_PARAM),
							Texture_GetSpectrumValue(mat->cloth.Weft_KdIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = CarPaintMaterial_Albedo(Texture_GetSpectrumValue(mat->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = RoughMatteMaterial_Albedo(Texture_GetSpectrumValue(mat->roughmatte.kdTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = RoughMatteTranslucentMaterial_Albedo(
							Texture_GetSpectrumValue(mat->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
							Texture_GetSpectrumValue(mat->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = GlossyTranslucentMaterial_Albedo(Texture_GetSpectrumValue(mat->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = DisneyMaterial_Albedo(Texture_GetSpectrumValue(mat->disney.baseColorTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = BLACK;
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					float3 albedo1, albedo2;
					EvalStack_PopFloat3(albedo2);
					EvalStack_PopFloat3(albedo1);
					
					const float3 albedo = MixMaterial_Albedo(Texture_GetFloatValue(mat->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM),
							albedo1, albedo2);
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					float3 albedo1;
					EvalStack_PopFloat3(albedo1);
					
					const float3 albedo = GlossyCoatingMaterial_Albedo(albedo1);
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {		
					const float3 albedo = WHITE;
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {		
					const float3 albedo = WHITE;
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {		
					const float3 albedo = WHITE;
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {		
					const float3 albedo = WHITE;
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {		
					const float3 albedo = WHITE;
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = HomogeneousVolMaterial_Albedo(
							Texture_GetSpectrumValue(mat->volume.homogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
							Texture_GetSpectrumValue(mat->volume.homogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
				case EVAL_ALBEDO: {
					const float3 albedo = HeterogeneousVolMaterial_Albedo(
							Texture_GetSpectrumValue(mat->volume.heterogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
							Texture_GetSpectrumValue(mat->volume.heterogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
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
