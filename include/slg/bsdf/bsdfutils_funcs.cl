#line 2 "bsdfutils_funcs.cl"

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

OPENCL_FORCE_INLINE bool BSDF_IsAlbedoEndPoint(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return !BSDF_IsDelta(bsdf MATERIALS_PARAM) ||
			// This is a very special case to not have white Albedo AOV if the
			// material is mirror. Mirror has no ray split so it can be render
			// without any noise.
			(mats[bsdf->materialIndex].type != MIRROR);
}

OPENCL_FORCE_INLINE uint BSDF_GetObjectID(__global const BSDF *bsdf, __global const SceneObject* restrict sceneObjs) {
	return sceneObjs[bsdf->sceneObjectIndex].objectID;
}

OPENCL_FORCE_INLINE uint BSDF_GetMaterialID(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].matID;
}

OPENCL_FORCE_INLINE uint BSDF_GetLightID(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].lightID;
}

#if defined(PARAM_HAS_VOLUMES)
OPENCL_FORCE_INLINE uint BSDF_GetMaterialInteriorVolume(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetInteriorVolume(bsdf->materialIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE uint BSDF_GetMaterialExteriorVolume(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetExteriorVolume(bsdf->materialIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			MATERIALS_PARAM);
}
#endif

OPENCL_FORCE_INLINE float BSDF_GetGlossiness(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetGlossiness(bsdf->materialIndex
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE float3 BSDF_GetLandingShadeN(__global const BSDF *bsdf) {
	return (bsdf->hitPoint.intoObject ? 1.f : -1.f) * VLOAD3F(&bsdf->hitPoint.shadeN.x);
}

OPENCL_FORCE_INLINE float3 BSDF_GetLandingGeometryN(__global const BSDF *bsdf) {
	return (bsdf->hitPoint.intoObject ? 1.f : -1.f) * VLOAD3F(&bsdf->hitPoint.geometryN.x);
}

OPENCL_FORCE_INLINE float3 BSDF_GetRayOrigin(__global const BSDF *bsdf, const float3 sampleDir) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);

#if defined(PARAM_HAS_VOLUMES)
	if (bsdf->isVolume)
		return p;
	else {
#endif
		// Rise the ray origin along the geometry normal to avoid self intersection
		const float3 geometryN = VLOAD3F(&bsdf->hitPoint.geometryN.x);
		const float riseDirection = (dot(sampleDir, geometryN) > 0.f) ? 1.f : -1.f;
		
		return p + riseDirection * (geometryN * MachineEpsilon_E_Float3(p));
#if defined(PARAM_HAS_VOLUMES)
	}
#endif
}
