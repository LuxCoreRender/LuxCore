#line 2 "material_main.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

//------------------------------------------------------------------------------
// Main material functions
//
// This is a glue between dynamic generated code and static.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Material_IsDynamic
//------------------------------------------------------------------------------

bool Material_IsDynamic(__global const Material *material) {
#if defined (PARAM_ENABLE_MAT_MIX) || defined (PARAM_ENABLE_MAT_GLOSSYCOATING)
		return (material->type == MIX) || (material->type == GLOSSYCOATING);
#else
		return false;
#endif
}

//------------------------------------------------------------------------------
// Material_GetEventTypes
//------------------------------------------------------------------------------

BSDFEvent Material_GetEventTypes(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_GetEventTypesWithDynamic(matIndex
			MATERIALS_PARAM);
	else
		return Material_GetEventTypesWithoutDynamic(material
			MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_IsDelta
//------------------------------------------------------------------------------

bool Material_IsDelta(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_IsDeltaWithDynamic(matIndex
			MATERIALS_PARAM);
	else
		return Material_IsDeltaWithoutDynamic(material
			MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_Evaluate
//------------------------------------------------------------------------------

float3 Material_Evaluate(const uint matIndex,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_EvaluateWithDynamic(matIndex, hitPoint,
			lightDir, eyeDir,
			event, directPdfW
			MATERIALS_PARAM);
	else
		return Material_EvaluateWithoutDynamic(material, hitPoint,
			lightDir, eyeDir,
			event, directPdfW
			MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_Sample
//------------------------------------------------------------------------------

float3 Material_Sample(const uint matIndex, __global HitPoint *hitPoint,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_SampleWithDynamic(matIndex, hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
				passThroughEvent,
#endif
				pdfW,  cosSampledDir, event, requestedEvent
				MATERIALS_PARAM);
	else
		return Material_SampleWithoutDynamic(material, hitPoint, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
				passThroughEvent,
#endif
				pdfW,  cosSampledDir, event, requestedEvent
				MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_GetPassThroughTransparency
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Material_GetPassThroughTransparency(const uint matIndex, __global HitPoint *hitPoint,
		const float3 localFixedDir, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_GetPassThroughTransparencyWithDynamic(matIndex, hitPoint,
			localFixedDir, passThroughEvent
			MATERIALS_PARAM);
	else
		return Material_GetPassThroughTransparencyWithoutDynamic(material, hitPoint,
			localFixedDir, passThroughEvent
			MATERIALS_PARAM);
}
#endif

//------------------------------------------------------------------------------
// Material_GetEmittedRadiance
//------------------------------------------------------------------------------

float3 Material_GetEmittedRadiance(const uint matIndex,
		__global HitPoint *hitPoint, const float oneOverPrimitiveArea
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	float3 result;
	if (Material_IsDynamic(material))
		result = Material_GetEmittedRadianceWithDynamic(matIndex, hitPoint
			MATERIALS_PARAM);
	else
		result = Material_GetEmittedRadianceWithoutDynamic(material, hitPoint
			MATERIALS_PARAM);

	return VLOAD3F(material->emittedFactor.c) * (material->usePrimitiveArea ? oneOverPrimitiveArea : 1.f) * result;
}

//------------------------------------------------------------------------------
// Material_GetInteriorVolume
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetInteriorVolume(const uint matIndex,
		__global HitPoint *hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float passThroughEvent
#endif
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_GetInteriorVolumeWithDynamic(matIndex, hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, passThroughEvent
#endif
			MATERIALS_PARAM);
	else
		return Material_GetInteriorVolumeWithoutDynamic(material, hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, passThroughEvent
#endif
			MATERIALS_PARAM);
}
#endif

//------------------------------------------------------------------------------
// Material_GetExteriorVolume
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetExteriorVolume(const uint matIndex,
		__global HitPoint *hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float passThroughEvent
#endif
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];
	
	if (Material_IsDynamic(material))
		return Material_GetExteriorVolumeWithDynamic(matIndex, hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, passThroughEvent
#endif
			MATERIALS_PARAM);
	else
		return Material_GetExteriorVolumeWithoutDynamic(material, hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, passThroughEvent
#endif
			MATERIALS_PARAM);
}
#endif

//------------------------------------------------------------------------------
// Material_Bump
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_BUMPMAPS)
void Material_Bump(const uint matIndex, __global HitPoint *hitPoint
	MATERIALS_PARAM_DECL) {
	const uint bumpTexIndex = mats[matIndex].bumpTexIndex;
	
	if (bumpTexIndex != NULL_INDEX) {
		float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);

		shadeN = Texture_Bump(mats[matIndex].bumpTexIndex, hitPoint, mats[matIndex].bumpSampleDistance
			TEXTURES_PARAM);

		// Update dpdu and dpdv so they are still orthogonal to shadeN
		float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
		float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
		dpdu = cross(shadeN, cross(dpdu, shadeN));
		dpdv = cross(shadeN, cross(dpdv, shadeN));
		// Update HitPoint structure
		VSTORE3F(shadeN, &hitPoint->shadeN.x);
		VSTORE3F(dpdu, &hitPoint->dpdu.x);
		VSTORE3F(dpdv, &hitPoint->dpdv.x);
	}
}
#endif
