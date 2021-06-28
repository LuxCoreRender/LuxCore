#line 2 "bsdfutils_funcs.cl"

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
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

OPENCL_FORCE_INLINE BSDFEvent BSDF_GetEventTypes(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetEventTypes(bsdf->materialIndex
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE bool BSDF_IsDelta(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsDelta(bsdf->materialIndex
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE uint BSDF_GetObjectID(__global const BSDF *bsdf, __global const SceneObject* restrict sceneObjs) {
	const uint sceneObjectIndex = bsdf->sceneObjectIndex;

	return (sceneObjectIndex != NULL_INDEX) ? sceneObjs[sceneObjectIndex].objectID : NULL_INDEX;
}

OPENCL_FORCE_INLINE uint BSDF_GetMaterialID(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].matID;
}

OPENCL_FORCE_INLINE uint BSDF_GetLightID(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].lightID;
}

OPENCL_FORCE_INLINE uint BSDF_GetMaterialInteriorVolume(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetInteriorVolume(bsdf->materialIndex, &bsdf->hitPoint,
			bsdf->hitPoint.passThroughEvent
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE uint BSDF_GetMaterialExteriorVolume(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetExteriorVolume(bsdf->materialIndex, &bsdf->hitPoint,
			bsdf->hitPoint.passThroughEvent
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE float BSDF_GetGlossiness(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetGlossiness(bsdf->materialIndex
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE float3 BSDF_GetPassThroughShadowTransparency(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return VLOAD3F(&mats[bsdf->materialIndex].passThroughShadowTransparency.c[0]);
}

OPENCL_FORCE_INLINE float3 BSDF_GetLandingGeometryN(__global const BSDF *bsdf) {
	return HitPoint_GetGeometryN(&bsdf->hitPoint);
}

OPENCL_FORCE_INLINE float3 BSDF_GetLandingInterpolatedN(__global const BSDF *bsdf) {
	return HitPoint_GetInterpolatedN(&bsdf->hitPoint);
}

OPENCL_FORCE_INLINE float3 BSDF_GetLandingShadeN(__global const BSDF *bsdf) {
	return HitPoint_GetShadeN(&bsdf->hitPoint);
}

OPENCL_FORCE_INLINE float3 BSDF_GetRayOrigin(__global const BSDF *bsdf, const float3 sampleDir) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

	if (bsdf->isVolume)
		return p;
	else {
		// Rise the ray origin along the geometry normal to avoid self intersection
		const float3 geometryN = VLOAD3F(&bsdf->hitPoint.geometryN.x);
		const float riseDirection = (dot(sampleDir, geometryN) > 0.f) ? 1.f : -1.f;
		
		return p + riseDirection * (geometryN * MachineEpsilon_E_Float3(p));
	}
}

OPENCL_FORCE_INLINE bool BSDF_IsAlbedoEndPoint(__global const BSDF *bsdf,
		const AlbedoSpecularSetting albedoSpecularSetting,
		const float albedoSpecularGlossinessThreshold
		MATERIALS_PARAM_DECL) {
	const BSDFEvent eventTypes = BSDF_GetEventTypes(bsdf MATERIALS_PARAM);
	if (!BSDF_IsDelta(bsdf MATERIALS_PARAM) &&
			!((eventTypes & GLOSSY) && (BSDF_GetGlossiness(bsdf MATERIALS_PARAM) < albedoSpecularGlossinessThreshold)))
		return true;

	switch (albedoSpecularSetting) {
		case NO_REFLECT_TRANSMIT:
			return true;
		case ONLY_REFLECT:
			return !((eventTypes & REFLECT) && !(eventTypes & TRANSMIT));
		case ONLY_TRANSMIT:
			return !(!(eventTypes & REFLECT) && (eventTypes & TRANSMIT));
		case REFLECT_TRANSMIT:
			return !((eventTypes & REFLECT) || (eventTypes & TRANSMIT));
		default:
			return true;
	}
}
