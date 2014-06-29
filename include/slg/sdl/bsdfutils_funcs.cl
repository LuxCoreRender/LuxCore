#line 2 "bsdfutils_funcs.cl"

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

BSDFEvent BSDF_GetEventTypes(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetEventTypes(bsdf->materialIndex
			MATERIALS_PARAM);
}

bool BSDF_IsDelta(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsDelta(bsdf->materialIndex
			MATERIALS_PARAM);
}

uint BSDF_GetMaterialID(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].matID;
}

uint BSDF_GetLightID(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return mats[bsdf->materialIndex].lightID;
}

#if defined(PARAM_HAS_VOLUMES)
uint BSDF_GetMaterialInteriorVolume(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetInteriorVolume(bsdf->materialIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			MATERIALS_PARAM);
}

uint BSDF_GetMaterialExteriorVolume(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetExteriorVolume(bsdf->materialIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, bsdf->hitPoint.passThroughEvent
#endif
			MATERIALS_PARAM);
}
#endif
