#line 2 "material_main_withoutdynamic.cl"

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
// Material_EvaluateWithoutDynamic
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_EvaluateWithoutDynamic(__global const Material* restrict material,
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	switch (material->type) {
		case MATTE:
			return MatteMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->matte.kdTexIndex, hitPoint TEXTURES_PARAM));
		case MIRROR:
			return MirrorMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->mirror.krTexIndex, hitPoint TEXTURES_PARAM));
		case GLASS:
			return GlassMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->glass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->glass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->glass.interiorIorTexIndex TEXTURES_PARAM),
					(material->glass.cauchyCTex != NULL_INDEX) ? Texture_GetFloatValue(material->glass.cauchyCTex, hitPoint TEXTURES_PARAM) : -1.f);
		case ARCHGLASS:
			return ArchGlassMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->archglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->archglass.interiorIorTexIndex TEXTURES_PARAM));
		case NULLMAT:
			return NullMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW);
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->matteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->matteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM));
		case GLOSSY2:
			return Glossy2Material_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint TEXTURES_PARAM),
					material->glossy2.multibounce,
					Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint TEXTURES_PARAM));
		case METAL2: {
			float3 n, k;
			Metal2Material_GetNK(material, hitPoint,
					&n, &k
					TEXTURES_PARAM);

			return Metal2Material_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetFloatValue(material->metal2.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->metal2.nvTexIndex, hitPoint TEXTURES_PARAM),
					n,
					k);
		}
		case ROUGHGLASS:
			return RoughGlassMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->roughglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughglass.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughglass.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughglass.nvTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->roughglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->roughglass.interiorIorTexIndex TEXTURES_PARAM));
		case VELVET:
			return VelvetMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint TEXTURES_PARAM));
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
		case ROUGHMATTE:
			return RoughMatteMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetFloatValue(material->roughmatte.sigmaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatte.kdTexIndex, hitPoint TEXTURES_PARAM));
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughmatteTranslucent.sigmaTexIndex, hitPoint TEXTURES_PARAM));
		case GLOSSYTRANSLUCENT:
			return GlossyTranslucentMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetFloatValue(material->glossytranslucent.indexTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.indexbfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nubfTexIndex, hitPoint TEXTURES_PARAM),					
					Texture_GetFloatValue(material->glossytranslucent.nvTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nvbfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.kabfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthbfTexIndex, hitPoint TEXTURES_PARAM),
					material->glossytranslucent.multibounce,
					material->glossytranslucent.multibouncebf,
					Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksbfTexIndex, hitPoint TEXTURES_PARAM));
		case DISNEY:
			return DisneyMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->disney.baseColorTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.subsurfaceTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.roughnessTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.metallicTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.specularTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.specularTintTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.clearcoatTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.clearcoatGlossTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.anisotropicTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.sheenTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.sheenTintTexIndex, hitPoint TEXTURES_PARAM));
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint TEXTURES_PARAM));
		case CLEAR_VOL:
			return ClearVolMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW);
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_Evaluate(
					hitPoint, lightDir, eyeDir, event, directPdfW,
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.gTexIndex, hitPoint TEXTURES_PARAM));
		default:
			// Something has gone very wrong
			return BLACK;
	}
}

//------------------------------------------------------------------------------
// Material_SampleWithoutDynamic
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_SampleWithoutDynamic(__global const Material* restrict material, __global const HitPoint *hitPoint,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	switch (material->type) {
		case MATTE:
			return MatteMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->matte.kdTexIndex, hitPoint TEXTURES_PARAM));
		case MIRROR:
			return MirrorMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->mirror.krTexIndex, hitPoint TEXTURES_PARAM));
		case GLASS:
			return GlassMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->glass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->glass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->glass.interiorIorTexIndex TEXTURES_PARAM),
					(material->glass.cauchyCTex != NULL_INDEX) ? Texture_GetFloatValue(material->glass.cauchyCTex, hitPoint TEXTURES_PARAM) : -1.f);
		case ARCHGLASS:
			return ArchGlassMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->archglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->archglass.krTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->archglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->archglass.interiorIorTexIndex TEXTURES_PARAM));
		case NULLMAT:
			return NullMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event);
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->matteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->matteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM));

		case GLOSSY2:
			return Glossy2Material_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint TEXTURES_PARAM),
					material->glossy2.multibounce,
					Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint TEXTURES_PARAM));
		case METAL2: {
			float3 n, k;
			Metal2Material_GetNK(material, hitPoint,
					&n, &k
					TEXTURES_PARAM);

			return Metal2Material_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetFloatValue(material->metal2.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->metal2.nvTexIndex, hitPoint TEXTURES_PARAM),
					n,
					k);
		}
		case ROUGHGLASS:
			return RoughGlassMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->roughglass.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughglass.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughglass.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughglass.nvTexIndex, hitPoint TEXTURES_PARAM),
					ExtractExteriorIors(hitPoint, material->roughglass.exteriorIorTexIndex TEXTURES_PARAM),
					ExtractInteriorIors(hitPoint, material->roughglass.interiorIorTexIndex TEXTURES_PARAM));
		case VELVET:
			return VelvetMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint TEXTURES_PARAM));
		case CLOTH:
			return ClothMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					material->cloth.Preset,
					material->cloth.Repeat_U,
					material->cloth.Repeat_V,
					material->cloth.specularNormalization,
					Texture_GetSpectrumValue(material->cloth.Warp_KsIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Weft_KsIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Warp_KdIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->cloth.Weft_KdIndex, hitPoint TEXTURES_PARAM));
		case CARPAINT:
			return CarPaintMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
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
		case ROUGHMATTE:
			return RoughMatteMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetFloatValue(material->roughmatte.sigmaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatte.kdTexIndex, hitPoint TEXTURES_PARAM));
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->roughmatteTranslucent.krTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->roughmatteTranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->roughmatteTranslucent.sigmaTexIndex, hitPoint TEXTURES_PARAM));
		case GLOSSYTRANSLUCENT:
			return GlossyTranslucentMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetFloatValue(material->glossytranslucent.indexTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.indexbfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nuTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nubfTexIndex, hitPoint TEXTURES_PARAM),					
					Texture_GetFloatValue(material->glossytranslucent.nvTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.nvbfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.kaTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.kabfTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->glossytranslucent.depthbfTexIndex, hitPoint TEXTURES_PARAM),
					material->glossytranslucent.multibounce,
					material->glossytranslucent.multibouncebf,
					Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ktTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->glossytranslucent.ksbfTexIndex, hitPoint TEXTURES_PARAM));
		case DISNEY:
			return DisneyMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->disney.baseColorTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.subsurfaceTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.roughnessTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.metallicTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.specularTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.specularTintTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.clearcoatTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.clearcoatGlossTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.anisotropicTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.sheenTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetFloatValue(material->disney.sheenTintTexIndex, hitPoint TEXTURES_PARAM));
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint TEXTURES_PARAM));
		case CLEAR_VOL:
			return ClearVolMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event);
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_Sample(
					hitPoint, fixedDir, sampledDir, u0, u1,
					passThroughEvent,
					pdfW, event,
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM),
					Texture_GetSpectrumValue(material->volume.heterogenous.gTexIndex, hitPoint TEXTURES_PARAM));
		default:
			// Something has gone very wrong
			return BLACK;
	}
}

//------------------------------------------------------------------------------
// Material_GetPassThroughTransparencyWithoutDynamic
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_GetPassThroughTransparencyWithoutDynamic(__global const Material* restrict material, __global const HitPoint *hitPoint,
		const float3 localFixedDir, const float passThroughEvent, const bool backTracing
		MATERIALS_PARAM_DECL) {
	switch (material->type) {
		case ARCHGLASS:
			return ArchGlassMaterial_GetPassThroughTransparency(material, hitPoint, localFixedDir, passThroughEvent, backTracing TEXTURES_PARAM);
		case NULLMAT:
			return NullMaterial_GetPassThroughTransparency(material, hitPoint, localFixedDir, passThroughEvent, backTracing TEXTURES_PARAM);
		default:
			return DefaultMaterial_GetPassThroughTransparency(material, hitPoint, localFixedDir, passThroughEvent, backTracing TEXTURES_PARAM);
	}
}

//------------------------------------------------------------------------------
// Material_GetEmittedRadianceWithoutDynamic
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 Material_GetEmittedRadianceWithoutDynamic(__global const Material* restrict material,
		__global const HitPoint *hitPoint, const float oneOverPrimitiveArea
		MATERIALS_PARAM_DECL) {
	return DefaultMaterial_GetEmittedRadiance(material, hitPoint, oneOverPrimitiveArea
		TEXTURES_PARAM);
}
