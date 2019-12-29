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

// Used when hitting a surface
OPENCL_FORCE_INLINE void HitPoint_Init(__global HitPoint *hitPoint, const bool throughShadowTransp,
		const uint meshIndex, const uint triIndex,
		const float3 pnt, const float3 fixedDir,
		const float b1, const float b2,
		const float passThroughEvnt
		MATERIALS_PARAM_DECL) {
	hitPoint->throughShadowTransparency = throughShadowTransp;
	hitPoint->passThroughEvent = passThroughEvnt;

	VSTORE3F(pnt, &hitPoint->p.x);
	VSTORE3F(fixedDir, &hitPoint->fixedDir.x);

#if defined(PARAM_ENABLE_TEX_OBJECTID) || defined(PARAM_ENABLE_TEX_OBJECTID_COLOR) || defined(PARAM_ENABLE_TEX_OBJECTID_NORMALIZED)
	hitPoint->objectID = sceneObjs[meshIndex].objectID;
#endif

	// Interpolate face normal
	const float3 geometryN = ExtMesh_GetGeometryNormal(&hitPoint->localToWorld, meshIndex, triIndex EXTMESH_PARAM);
	VSTORE3F(geometryN,  &hitPoint->geometryN.x);
	const float3 interpolatedN = ExtMesh_GetInterpolateNormal(&hitPoint->localToWorld, meshIndex, triIndex, b1, b2 EXTMESH_PARAM);
	VSTORE3F(interpolatedN,  &hitPoint->interpolatedN.x);
	const float3 shadeN = interpolatedN;
	VSTORE3F(shadeN,  &hitPoint->shadeN.x);

	hitPoint->intoObject = (dot(-fixedDir, geometryN) < 0.f);

	for (uint i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		// Interpolate UV coordinates
		const float2 uv = ExtMesh_GetInterpolateUV(meshIndex, triIndex, b1, b2, i EXTMESH_PARAM);
		VSTORE2F(uv, &hitPoint->uv[i].u);

		// Interpolate color
		const float3 color = ExtMesh_GetInterpolateColor(meshIndex, triIndex, b1, b2, i EXTMESH_PARAM);
		VSTORE3F(color, &hitPoint->color[i].c[0]);

		// Interpolate alpha
		hitPoint->alpha[i] = ExtMesh_GetInterpolateAlpha(meshIndex, triIndex, b1, b2, i EXTMESH_PARAM);
	}

	// Compute geometry differentials
	float3 dndu, dndv, dpdu, dpdv;
	ExtMesh_GetDifferentials(
			&hitPoint->localToWorld,
			meshIndex,
			triIndex,
			shadeN, 0,
			&dpdu, &dpdv,
			&dndu, &dndv
			EXTMESH_PARAM);
	VSTORE3F(dpdu, &hitPoint->dpdu.x);
	VSTORE3F(dpdv, &hitPoint->dpdv.x);
	VSTORE3F(dndu, &hitPoint->dndu.x);
	VSTORE3F(dndv, &hitPoint->dndv.x);
}

// Initialize all fields
OPENCL_FORCE_INLINE void HitPoint_InitDefault(__global HitPoint *hitPoint) {
	hitPoint->throughShadowTransparency = false;
	hitPoint->passThroughEvent = 0.f;

	VSTORE3F((float3)(0.f, 0.f, 0.f), &hitPoint->p.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &hitPoint->fixedDir.x);

#if defined(PARAM_ENABLE_TEX_OBJECTID) || defined(PARAM_ENABLE_TEX_OBJECTID_COLOR) || defined(PARAM_ENABLE_TEX_OBJECTID_NORMALIZED)
	hitPoint->objectID = 0;
#endif
	
	VSTORE3F((float3)(0.f, 0.f, 0.f),  &hitPoint->geometryN.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f),  &hitPoint->interpolatedN.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f),  &hitPoint->shadeN.x);

	hitPoint->intoObject = true;

	for (uint i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		VSTORE2F((float2)(0.f, 0.f), &hitPoint->uv[i].u);
		VSTORE3F(WHITE, &hitPoint->color[i].c[0]);
		hitPoint->alpha[i] = 1.f;
	}
	
	VSTORE3F((float3)(0.f, 0.f, 0.f), &hitPoint->dpdu.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &hitPoint->dpdv.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &hitPoint->dndu.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &hitPoint->dndv.x);

	Transform_Init(&hitPoint->localToWorld);

#if defined(PARAM_HAS_VOLUMES)
	hitPoint->interiorVolumeIndex = NULL_INDEX;
	hitPoint->exteriorVolumeIndex = NULL_INDEX;
	hitPoint->interiorIorTexIndex = NULL_INDEX;
	hitPoint->exteriorIorTexIndex = NULL_INDEX;
#endif
}

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
