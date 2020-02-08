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
					const float3 albedo = Texture_GetSpectrumValue(mat->matte.kdTexIndex,
							hitPoint TEXTURES_PARAM);
					EvalStack_PushFloat3(albedo);
					break;
				}
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat,
							hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
							Texture_GetSpectrumValue(mat->matteTranslucent.krTexIndex,hitPoint TEXTURES_PARAM),
							Texture_GetSpectrumValue(mat->matteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM));
					EvalStack_PushFloat3(albedo);
					break;
				}
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat,
							hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = NullMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_VOLUME_MIX_SETUP1: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const float factor = Texture_GetFloatValue(mat->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
					const float weight2 = clamp(factor, 0.f, 1.f);
					const float weight1 = 1.f - weight2;

					const float passThroughEvent1 = passThroughEvent / weight1;

					EvalStack_PushFloat(passThroughEvent);
					EvalStack_PushFloat(factor);
					EvalStack_PushFloat(passThroughEvent1);
					break;
				}
				case EVAL_GET_VOLUME_MIX_SETUP2: {
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
					break;
				}
				case EVAL_GET_INTERIOR_VOLUME: {
					const uint volIndex = mat->interiorVolumeIndex;
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
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					const uint volIndex = mat->exteriorVolumeIndex;
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
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE_MIX_SETUP1: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					EvalStack_PushFloat(oneOverPrimitiveArea);
					// To setup the following EVAL_GET_EMITTED_RADIANCE evaluation of first sub-nodes
					EvalStack_PushFloat(oneOverPrimitiveArea);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE_MIX_SETUP2: {
					float3 emit1;
					EvalStack_PopFloat3(emit1);

					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					EvalStack_PushFloat(oneOverPrimitiveArea);
					EvalStack_PushFloat3(emit1);
					// To setup the following EVAL_GET_EMITTED_RADIANCE evaluation of second sub-nodes
					EvalStack_PushFloat(oneOverPrimitiveArea);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float3 emittedRadiance;
					if (mat->emitTexIndex != NULL_INDEX) {
						float oneOverPrimitiveArea;
						EvalStack_PopFloat(oneOverPrimitiveArea);

						emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					} else {
						float3 emit1, emit2;
						EvalStack_PopFloat3(emit2);
						EvalStack_PopFloat3(emit1);

						float oneOverPrimitiveArea;
						EvalStack_PopFloat(oneOverPrimitiveArea);
						
						const float factor = Texture_GetFloatValue(mat->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
						const float weight2 = clamp(factor, 0.f, 1.f);
						const float weight1 = 1.f - weight2;

						emittedRadiance = BLACK;
						if (weight1 > 0.f)
							emittedRadiance += weight1 * emit1;
						 if (weight2 > 0.f)
							emittedRadiance += weight2 * emit2;
					}

					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP1: {
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

					const float factor = Texture_GetFloatValue(mat->mix.mixFactorTexIndex, hitPoint TEXTURES_PARAM);
					const float weight2 = clamp(factor, 0.f, 1.f);
					const float weight1 = 1.f - weight2;

					const float passThroughEvent1 = passThroughEvent / weight1;

					EvalStack_PushFloat(factor);

					// To setup the following EVAL_GET_PASS_TROUGH_TRANSPARENCY evaluation of first sub-nodes
					EvalStack_PushFloat3(localFixedDir);
					EvalStack_PushFloat(passThroughEvent1);
					EvalStack_PushUInt(backTracing);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP2: {
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
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
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
						mat->frontTranspTexIndex : mat->backTranspTexIndex;

					float3 transp;
					if (transpTexIndex != NULL_INDEX) {
						transp = DefaultMaterial_GetPassThroughTransparency(mat,
								hitPoint, localFixedDir, passThroughEvent, backTracing
								TEXTURES_PARAM);
					} else {
						const float weight2 = clamp(factor, 0.f, 1.f);
						const float weight1 = 1.f - weight2;

						transp = (passThroughEvent < weight1) ? transp1 : transp2;
					}
					
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					const uint volIndex = mat->interiorVolumeIndex;
					if (volIndex != NULL_INDEX) {
						float passThroughEvent;
						EvalStack_PopFloat(passThroughEvent);

						EvalStack_PushUInt(volIndex);
					}
					// Else nothing to do because there is already the matBase volume
					// index on the stack
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					const uint volIndex = mat->exteriorVolumeIndex;
					if (volIndex != NULL_INDEX) {
						float passThroughEvent;
						EvalStack_PopFloat(passThroughEvent);

						EvalStack_PushUInt(volIndex);
					}
					// Else nothing to do because there is already the matBase volume
					// index on the stack
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					if (mat->emitTexIndex != NULL_INDEX) {
						float oneOverPrimitiveArea;
						EvalStack_PopFloat(oneOverPrimitiveArea);

						const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
						
						EvalStack_PushFloat3(emittedRadiance);
					}
					// Else nothing to do because there is already the matBase emitted
					// radiance on the stack					
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					// Nothing to do there is already the matBase pass trough
					// transparency on the stack		
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = ArchGlassMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
				case EVAL_GET_INTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetInteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EXTERIOR_VOLUME: {
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);

					const uint volIndex = DefaultMaterial_GetExteriorVolume(mat);
					EvalStack_PushUInt(volIndex);
					break;
				}
				case EVAL_GET_EMITTED_RADIANCE: {
					float oneOverPrimitiveArea;
					EvalStack_PopFloat(oneOverPrimitiveArea);

					const float3 emittedRadiance = DefaultMaterial_GetEmittedRadiance(mat, hitPoint, oneOverPrimitiveArea TEXTURES_PARAM);
					EvalStack_PushFloat3(emittedRadiance);
					break;
				}
				case EVAL_GET_PASS_TROUGH_TRANSPARENCY: {
					bool backTracing;
					EvalStack_PopUInt(backTracing);
					float passThroughEvent;
					EvalStack_PopFloat(passThroughEvent);
					float3 localFixedDir;
					EvalStack_PopFloat3(localFixedDir);

					const float3 transp = DefaultMaterial_GetPassThroughTransparency(mat,
							hitPoint, localFixedDir, passThroughEvent, backTracing
							TEXTURES_PARAM);
					EvalStack_PushFloat3(transp);
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
