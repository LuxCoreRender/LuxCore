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

OPENCL_FORCE_INLINE BSDFEvent BSDF_GetEventTypes(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetEventTypes(bsdf->materialIndex
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE bool BSDF_IsDelta(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsDelta(bsdf->materialIndex
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE bool BSDF_IsAlbedoEndPoint(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return !BSDF_IsDelta(bsdf MATERIALS_PARAM) ||
			// This is a very special case to not have white Albedo AOV if the
			// material is mirror. Mirror has no ray split so it can be render
			// without any noise.
			(mats[bsdf->materialIndex].type != MIRROR);
}

OPENCL_FORCE_INLINE uint BSDF_GetObjectID(__global BSDF *bsdf, __global const SceneObject* restrict sceneObjs) {
	return sceneObjs[bsdf->sceneObjectIndex].objectID;
}

OPENCL_FORCE_INLINE uint BSDF_GetMaterialID(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].matID;
}

OPENCL_FORCE_INLINE uint BSDF_GetLightID(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].lightID;
}

#if defined(PARAM_HAS_VOLUMES)
OPENCL_FORCE_INLINE uint BSDF_GetMaterialInteriorVolume(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetInteriorVolume(bsdf->materialIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE uint BSDF_GetMaterialExteriorVolume(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetExteriorVolume(bsdf->materialIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			MATERIALS_PARAM);
}
#endif
