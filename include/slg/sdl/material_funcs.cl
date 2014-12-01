#line 2 "material_funcs.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

float3 Material_GetEmittedRadianceNoMix(__global Material *material, __global HitPoint *hitPoint
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
void Material_BumpNoMix(__global Material *material, __global HitPoint *hitPoint,
        const float3 dpdu, const float3 dpdv,
        const float3 dndu, const float3 dndv, const float weight
        TEXTURES_PARAM_DECL) {
    if ((material->bumpTexIndex != NULL_INDEX) && (weight > 0.f)) {
        const float2 duv = weight * 
#if defined(PARAM_ENABLE_TEX_NORMALMAP)
            ((texs[material->bumpTexIndex].type == NORMALMAP_TEX) ?
                NormalMapTexture_GetDuv(material->bumpTexIndex,
                    hitPoint, dpdu, dpdv, dndu, dndv, material->bumpSampleDistance
                    TEXTURES_PARAM) :
                Texture_GetDuv(material->bumpTexIndex,
                    hitPoint, dpdu, dpdv, dndu, dndv, material->bumpSampleDistance
                    TEXTURES_PARAM));
#else
            Texture_GetDuv(material->bumpTexIndex,
                hitPoint, dpdu, dpdv, dndu, dndv, material->bumpSampleDistance
                TEXTURES_PARAM);
#endif

        const float3 oldShadeN = VLOAD3F(&hitPoint->shadeN.x);
        const float3 bumpDpdu = dpdu + duv.s0 * oldShadeN;
        const float3 bumpDpdv = dpdv + duv.s1 * oldShadeN;
        float3 newShadeN = normalize(cross(bumpDpdu, bumpDpdv));

        // The above transform keeps the normal in the original normal
        // hemisphere. If they are opposed, it means UVN was indirect and
        // the normal needs to be reversed
        newShadeN *= (dot(oldShadeN, newShadeN) < 0.f) ? -1.f : 1.f;

        VSTORE3F(newShadeN, &hitPoint->shadeN.x);
    }
}
#endif

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetInteriorVolumeNoMix(__global Material *material) {
	return material->interiorVolumeIndex;
}

uint Material_GetExteriorVolumeNoMix(__global Material *material) {
	return material->exteriorVolumeIndex;
}
#endif

//------------------------------------------------------------------------------
// All following functions are used only when material dynamic code generation
// is disabled
//------------------------------------------------------------------------------

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)

bool Material_IsDeltaNoMix(__global Material *material) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:	
			return MatteMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)
		case ROUGHMATTE:
			return RoughMatteMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_VELVET)
		case VELVET:
			return VelvetMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT)
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
			return Metal2Material_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)
		case ROUGHGLASS:
			return RoughGlassMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_CLOTH)
		case CLOTH:
			return ClothMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_CARPAINT)
		case CARPAINT:
			return CarPaintMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_IsDelta();
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_IsDelta();
#endif
		default:
			return true;
	}
}

BSDFEvent Material_GetEventTypesNoMix(__global Material *mat) {
	switch (mat->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)
		case ROUGHMATTE:
			return RoughMatteMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_VELVET)
		case VELVET:
			return VelvetMaterial_GetEventTypes();
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
#if defined (PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT)
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_GetEventTypes();
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
#if defined (PARAM_ENABLE_MAT_CLOTH)
		case CLOTH:
			return ClothMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_CARPAINT)
		case CARPAINT:
			return CarPaintMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_GetEventTypes();
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_GetEventTypes();
#endif
		default:
			return NONE;
	}
}

float3 Material_SampleNoMix(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)
		case ROUGHMATTE:
			return RoughMatteMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_VELVET)
		case VELVET:
			return VelvetMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT)
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
			return Metal2Material_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)
		case ROUGHGLASS:
			return RoughGlassMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_CLOTH)
		case CLOTH:
			return ClothMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_CARPAINT)
		case CARPAINT:
			return CarPaintMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_Sample(material, hitPoint, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event, requestedEvent
					TEXTURES_PARAM);
#endif
		default:
			return BLACK;
	}
}

float3 Material_EvaluateNoMix(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)
		case ROUGHMATTE:
			return RoughMatteMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_VELVET)
		case VELVET:
			return VelvetMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MATTETRANSLUCENT)
		case MATTETRANSLUCENT:
			return MatteTranslucentMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT)
		case ROUGHMATTETRANSLUCENT:
			return RoughMatteTranslucentMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLOSSY2)
		case GLOSSY2:
			return Glossy2Material_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_METAL2)
		case METAL2:
			return Metal2Material_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ROUGHGLASS)
		case ROUGHGLASS:
			return RoughGlassMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_CLOTH)
		case CLOTH:
			return ClothMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_CARPAINT)
		case CARPAINT:
			return CarPaintMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_Evaluate(material, hitPoint, lightDir, eyeDir, event, directPdfW
					TEXTURES_PARAM);
#endif
		default:
			return BLACK;
	}
}

float3 Material_GetPassThroughTransparencyNoMix(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_GetPassThroughTransparency(material,
					hitPoint, fixedDir, passThroughEvent
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_GetPassThroughTransparency(material,
					hitPoint, fixedDir, passThroughEvent
					TEXTURES_PARAM);
#endif
		default:
			return BLACK;
	}
}

#endif
