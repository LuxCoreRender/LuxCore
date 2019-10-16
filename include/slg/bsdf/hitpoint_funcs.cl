#line 2 "hitpoint_funcs.cl"

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

OPENCL_FORCE_INLINE void HitPoint_GetFrame(__global const HitPoint *hitPoint, Frame *frame) {
	Frame_Set_Private(frame, VLOAD3F(&hitPoint->dpdu.x), VLOAD3F(&hitPoint->dpdv.x), VLOAD3F(&hitPoint->shadeN.x));
}

OPENCL_FORCE_INLINE float3 HitPoint_GetGeometryN(__global const HitPoint *hitPoint) {
	return (hitPoint->intoObject ? 1.f : -1.f) * VLOAD3F(&hitPoint->geometryN.x);
}

OPENCL_FORCE_INLINE float3 HitPoint_GetInterpolatedN(__global const HitPoint *hitPoint) {
	return (hitPoint->intoObject ? 1.f : -1.f) * VLOAD3F(&hitPoint->interpolatedN.x);
}

OPENCL_FORCE_INLINE float3 HitPoint_GetShadeN(__global const HitPoint *hitPoint) {
	return (hitPoint->intoObject ? 1.f : -1.f) * VLOAD3F(&hitPoint->shadeN.x);
}
