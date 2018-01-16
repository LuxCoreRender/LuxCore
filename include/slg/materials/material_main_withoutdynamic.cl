#line 2 "material_main_withoutdynamic.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
 * distributed under the License is distributed on an "AS IS" BASIS,      *
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

BSDFEvent Material_GetEventTypesWithoutDynamic(__global const Material* restrict material
		MATERIALS_PARAM_DECL) {
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

bool Material_IsDeltaWithoutDynamic(__global const Material* restrict material
		MATERIALS_PARAM_DECL) {
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
// Material_EvaluateWithoutDynamic
//------------------------------------------------------------------------------

float3 Material_EvaluateWithoutDynamic(__global const Material* restrict material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->matte.kdTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->mirror.krTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->glass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->glass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->glass.interiorIorTexIndex TEXTURES_PARAM),
					(material->glass.cauchyCTex != NULL_INDEX) ? Texture_GetFloatValue(material->glass.cauchyCTex, hitPoint TEXTURES_PARAM) : -1.f);
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->archglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->archglass.interiorIorTexIndex TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->matteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->matteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM));

#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
					Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint TEXTURES_PARAM),
#endif
					Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint TEXTURES_PARAM),
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
					Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
					Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
					material->glossy2.multibounce,
#endif
					Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2: {
			float3 n, k;
			Metal2Material_GetNK(material, hitPoint,
					&n, &k
					TEXTURES_PARAM);

			return Metal2Material_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetFloatValue(material->metal2.nuTexIndex, hitPoint TEXTURES_PARAM),
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
					Texture_GetFloatValue(material->metal2.nvTexIndex, hitPoint TEXTURES_PARAM),
#endif
					n,
					k);
		}
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)
		case ROUGHGLASS:
			return RoughGlassMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->roughglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughglass.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->metal2.nuTexIndex, hitPoint TEXTURES_PARAM),
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
					Texture_GetFloatValue(material->metal2.nvTexIndex, hitPoint TEXTURES_PARAM),
#endif
					ExtractExteriorIors(hitPoint, material->roughglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->roughglass.interiorIorTexIndex TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_VELVET)
		case VELVET:
			return VelvetMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_CLOTH)
		case CLOTH:
			return ClothMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					material->cloth.Preset,
					material->cloth.Repeat_U,
					material->cloth.Repeat_V,
					material->cloth.specularNormalization,
					Texture_GetSpectrumValue(material->cloth.Warp_KsIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Weft_KsIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Warp_KdIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Weft_KdIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_CARPAINT)
		case CARPAINT:
			return CarPaintMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->carpaint.KaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.depthTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.Ks2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.M2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.R2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.Ks3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.M3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.R3TexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)
		case ROUGHMATTE:
			return RoughMatteMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetFloatValue(material->roughmatte.sigmaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatte.kdTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT)
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughmatteTranslucent.sigmaTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT)
		case GLOSSYTRANSLUCENT:
			return GlossyTranslucentMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
					Texture_GetFloatValue(material->glossytranslucent.indexTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.indexbfTexIndex, hitPoint TEXTURES_PARAM),
#endif
					Texture_GetFloatValue(material->glossytranslucent.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nubfTexIndex, hitPoint TEXTURES_PARAM),					
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
					Texture_GetFloatValue(material->glossytranslucent.nvTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nvbfTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
					Texture_GetSpectrumValue(material->glossytranslucent.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.kabfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthbfTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
					material->glossytranslucent.multibounce,
					material->glossytranslucent.multibouncebf,
#endif
					Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksbfTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW);
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.gTexIndex, hitPoint TEXTURES_PARAM));
#endif
		default:
			// Something has gone very wrong
			return BLACK;
	}
}

//------------------------------------------------------------------------------
// Material_SampleWithoutDynamic
//------------------------------------------------------------------------------

float3 Material_SampleWithoutDynamic(__global const Material* restrict material, __global HitPoint *hitPoint,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->matte.kdTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->mirror.krTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->glass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->glass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->glass.interiorIorTexIndex TEXTURES_PARAM),
					(material->glass.cauchyCTex != NULL_INDEX) ? Texture_GetFloatValue(material->glass.cauchyCTex, hitPoint TEXTURES_PARAM) : -1.f);
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->archglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->archglass.interiorIorTexIndex TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->matteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->matteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM));

#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
					Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint TEXTURES_PARAM),
#endif
					Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint TEXTURES_PARAM),
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
					Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
					Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
					material->glossy2.multibounce,
#endif
					Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2: {
			float3 n, k;
			Metal2Material_GetNK(material, hitPoint,
					&n, &k
					TEXTURES_PARAM);

			return Metal2Material_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetFloatValue(material->metal2.nuTexIndex, hitPoint TEXTURES_PARAM),
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
					Texture_GetFloatValue(material->metal2.nvTexIndex, hitPoint TEXTURES_PARAM),
#endif
					n,
					k);
		}
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)
		case ROUGHGLASS:
			return RoughGlassMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->roughglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughglass.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughglass.nuTexIndex, hitPoint TEXTURES_PARAM),
#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)
					Texture_GetFloatValue(material->roughglass.nvTexIndex, hitPoint TEXTURES_PARAM),
#endif
					ExtractExteriorIors(hitPoint, material->roughglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->roughglass.interiorIorTexIndex TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_VELVET)
		case VELVET:
			return VelvetMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_CLOTH)
		case CLOTH:
			return ClothMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					material->cloth.Preset,
					material->cloth.Repeat_U,
					material->cloth.Repeat_V,
					material->cloth.specularNormalization,
					Texture_GetSpectrumValue(material->cloth.Warp_KsIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Weft_KsIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Warp_KdIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Weft_KdIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_CARPAINT)
		case CARPAINT:
			return CarPaintMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->carpaint.KaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.depthTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.KdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.Ks1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.M1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.R1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.Ks2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.M2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.R2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->carpaint.Ks3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.M3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->carpaint.R3TexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)
		case ROUGHMATTE:
			return RoughMatteMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetFloatValue(material->roughmatte.sigmaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatte.kdTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT)
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughmatteTranslucent.sigmaTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT)
		case GLOSSYTRANSLUCENT:
			return GlossyTranslucentMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
					Texture_GetFloatValue(material->glossytranslucent.indexTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.indexbfTexIndex, hitPoint TEXTURES_PARAM),
#endif
					Texture_GetFloatValue(material->glossytranslucent.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nubfTexIndex, hitPoint TEXTURES_PARAM),					
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
					Texture_GetFloatValue(material->glossytranslucent.nvTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nvbfTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
					Texture_GetSpectrumValue(material->glossytranslucent.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.kabfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthbfTexIndex, hitPoint TEXTURES_PARAM),
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
					material->glossytranslucent.multibounce,
					material->glossytranslucent.multibouncebf,
#endif
					Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksbfTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint TEXTURES_PARAM));
#endif
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event);
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event,
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.gTexIndex, hitPoint TEXTURES_PARAM));
#endif
		default:
			// Something has gone very wrong
			return BLACK;
	}
}

//------------------------------------------------------------------------------
// Material_GetPassThroughTransparencyWithoutDynamic
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Material_GetPassThroughTransparencyWithoutDynamic(__global const Material* restrict material, __global HitPoint *hitPoint,
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

float3 Material_GetEmittedRadianceWithoutDynamic(__global const Material* restrict material, __global HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	return DefaultMaterial_GetEmittedRadiance(material, hitPoint
		TEXTURES_PARAM);
}

//------------------------------------------------------------------------------
// Material_GetInteriorVolumeWithoutDynamic
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetInteriorVolumeWithoutDynamic(__global const Material *material,
		__global HitPoint *hitPoint, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	return DefaultMaterial_GetInteriorVolume(material);
}
#endif

//------------------------------------------------------------------------------
// Material_GetExteriorVolumeWithoutDynamic
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetExteriorVolumeWithoutDynamic(__global const Material *material,
		__global HitPoint *hitPoint, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	return DefaultMaterial_GetExteriorVolume(material);
}
#endif
