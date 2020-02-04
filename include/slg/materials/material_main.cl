#line 2 "material_main.cl"

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

//------------------------------------------------------------------------------
// Main material functions
//
// This is a glue between dynamic generated code and static.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Material_IsDynamic
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE bool Material_IsDynamic(__global const Material *material) {
		return (material->type == MIX) || (material->type == GLOSSYCOATING);
}

//------------------------------------------------------------------------------
// Material_GetEventTypes
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE BSDFEvent Material_GetEventTypes(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	return material->eventTypes;
}

//------------------------------------------------------------------------------
// Material_IsDelta
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE bool Material_IsDelta(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	return material->isDelta;
}

//------------------------------------------------------------------------------
// Material_Albedo
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 Material_Albedo(const uint matIndex,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_AlbedoWithDynamic(matIndex, hitPoint
			MATERIALS_PARAM);
	else
		return Material_AlbedoWithoutDynamic(material, hitPoint
			MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_Evaluate
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 Material_Evaluate(const uint matIndex,
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
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

OPENCL_FORCE_INLINE float3 Material_Sample(const uint matIndex, __global const HitPoint *hitPoint,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_SampleWithDynamic(matIndex, hitPoint, fixedDir, sampledDir, u0, u1,
				passThroughEvent,
				pdfW, event
				MATERIALS_PARAM);
	else
		return Material_SampleWithoutDynamic(material, hitPoint, fixedDir, sampledDir, u0, u1,
				passThroughEvent,
				pdfW, event
				MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_GetPassThroughTransparency
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 Material_GetPassThroughTransparency(const uint matIndex,
		__global const HitPoint *hitPoint,
		const float3 localFixedDir, const float passThroughEvent, const bool backTracing
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_GetPassThroughTransparencyWithDynamic(matIndex, hitPoint,
			localFixedDir, passThroughEvent, backTracing
			MATERIALS_PARAM);
	else
		return Material_GetPassThroughTransparencyWithoutDynamic(material, hitPoint,
			localFixedDir, passThroughEvent, backTracing
			MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_GetEmittedRadiance
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 Material_GetEmittedRadiance(const uint matIndex,
		__global const HitPoint *hitPoint, const float oneOverPrimitiveArea
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	float3 result;
	if (Material_IsDynamic(material))
		result = Material_GetEmittedRadianceWithDynamic(matIndex, hitPoint, oneOverPrimitiveArea
			MATERIALS_PARAM);
	else
		result = Material_GetEmittedRadianceWithoutDynamic(material, hitPoint, oneOverPrimitiveArea
			MATERIALS_PARAM);

	return result;
}

//------------------------------------------------------------------------------
// Material_GetEmittedCosThetaMax
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float Material_GetEmittedCosThetaMax(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	return material->emittedCosThetaMax;
}

//------------------------------------------------------------------------------
// Material_GetInteriorVolume
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE uint Material_GetInteriorVolume(const uint matIndex,
		__global const HitPoint *hitPoint,
		const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];

	if (Material_IsDynamic(material))
		return Material_GetInteriorVolumeWithDynamic(matIndex, hitPoint,
				passThroughEvent
				MATERIALS_PARAM);
	else
		return Material_GetInteriorVolumeWithoutDynamic(material, hitPoint,
				passThroughEvent
				MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_GetExteriorVolume
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE uint Material_GetExteriorVolume(const uint matIndex,
		__global const HitPoint *hitPoint,
		const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	__global const Material *material = &mats[matIndex];
	
	if (Material_IsDynamic(material))
		return Material_GetExteriorVolumeWithDynamic(matIndex, hitPoint,
				passThroughEvent
				MATERIALS_PARAM);
	else
		return Material_GetExteriorVolumeWithoutDynamic(material, hitPoint,
				passThroughEvent
				MATERIALS_PARAM);
}

//------------------------------------------------------------------------------
// Material_Bump
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void Material_Bump(const uint matIndex, __global HitPoint *hitPoint
	MATERIALS_PARAM_DECL) {
	const uint bumpTexIndex = mats[matIndex].bumpTexIndex;
	
	if (bumpTexIndex != NULL_INDEX) {
		const float3 shadeN = Texture_Bump(mats[matIndex].bumpTexIndex, hitPoint, mats[matIndex].bumpSampleDistance
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

//------------------------------------------------------------------------------
// Material_GetGlossiness
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float Material_GetGlossiness(const uint matIndex
		MATERIALS_PARAM_DECL) {
	return mats[matIndex].glossiness;
}

//------------------------------------------------------------------------------
// Material_GetGlossiness
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float Material_IsPhotonGIEnabled(const uint matIndex
		MATERIALS_PARAM_DECL) {
	return mats[matIndex].isPhotonGIEnabled;
}
