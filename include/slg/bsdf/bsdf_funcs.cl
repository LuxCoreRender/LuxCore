#line 2 "bsdf_funcs.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

// TODO: move in a separate extmesh_funcs.h file

void ExtMesh_GetDifferentials(
		__global const Mesh* restrict meshDescs,
		__global const Point* restrict vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global const Vector* restrict vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global const UV* restrict vertUVs,
#endif
		__global const Triangle* restrict triangles,
		const uint meshIndex,
		const uint triangleIndex,
		float3 shadeNormal,
		float3 *dpdu, float3 *dpdv,
        float3 *dndu, float3 *dndv) {
	__global const Mesh* restrict meshDesc = &meshDescs[meshIndex];
	__global const Point* restrict iVertices = &vertices[meshDesc->vertsOffset];
	__global const Triangle* restrict iTriangles = &triangles[meshDesc->trisOffset];

	// Compute triangle partial derivatives
	__global const Triangle* restrict tri = &iTriangles[triangleIndex];
	const uint vi0 = tri->v[0];
	const uint vi1 = tri->v[1];
	const uint vi2 = tri->v[2];

	float2 uv0, uv1, uv2;
#if defined(PARAM_HAS_UVS_BUFFER)
	if (meshDesc->uvsOffset != NULL_INDEX) {
		// Ok, UV coordinates are available, use them to build the reference
		// system around the shading normal.

		__global const UV* restrict iVertUVs = &vertUVs[meshDesc->uvsOffset];
		uv0 = VLOAD2F(&iVertUVs[vi0].u);
		uv1 = VLOAD2F(&iVertUVs[vi1].u);
		uv2 = VLOAD2F(&iVertUVs[vi2].u);
	} else {
#endif
		uv0 = (float2)(.5f, .5f);
		uv1 = (float2)(.5f, .5f);
		uv2 = (float2)(.5f, .5f);
#if defined(PARAM_HAS_UVS_BUFFER)
	}
#endif

	// Compute deltas for triangle partial derivatives
	const float du1 = uv0.s0 - uv2.s0;
	const float du2 = uv1.s0 - uv2.s0;
	const float dv1 = uv0.s1 - uv2.s1;
	const float dv2 = uv1.s1 - uv2.s1;
	const float determinant = du1 * dv2 - dv1 * du2;

	if (determinant == 0.f) {
		// Handle 0 determinant for triangle partial derivative matrix
		CoordinateSystem(shadeNormal, dpdu, dpdv);
		*dndu = ZERO;
		*dndv = ZERO;
	} else {
		const float invdet = 1.f / determinant;

		// Vertices expressed in local coordinates
		const float3 p0 = VLOAD3F(&iVertices[vi0].x);
		const float3 p1 = VLOAD3F(&iVertices[vi1].x);
		const float3 p2 = VLOAD3F(&iVertices[vi2].x);
		// Transform to global coordinates
		const float3 dp1 = Transform_InvApplyVector(&meshDesc->trans, p0 - p2);
		const float3 dp2 = Transform_InvApplyVector(&meshDesc->trans, p1 - p2);

		//------------------------------------------------------------------
		// Compute dpdu and dpdv
		//------------------------------------------------------------------

		const float3 geometryDpDu = ( dv2 * dp1 - dv1 * dp2) * invdet;
		const float3 geometryDpDv = (-du2 * dp1 + du1 * dp2) * invdet;

		*dpdu = cross(shadeNormal, cross(geometryDpDu, shadeNormal));
		*dpdv = cross(shadeNormal, cross(geometryDpDv, shadeNormal));

		//------------------------------------------------------------------
		// Compute dndu and dndv
		//------------------------------------------------------------------

#if defined(PARAM_HAS_NORMALS_BUFFER)
		if (meshDesc->normalsOffset != NULL_INDEX) {
			__global const Vector* restrict iVertNormals = &vertNormals[meshDesc->normalsOffset];
			// Shading normals expressed in local coordinates
			const float3 n0 = VLOAD3F(&iVertNormals[tri->v[0]].x);
			const float3 n1 = VLOAD3F(&iVertNormals[tri->v[1]].x);
			const float3 n2 = VLOAD3F(&iVertNormals[tri->v[2]].x);
			const float3 dn1 = n0 - n2;
			const float3 dn2 = n1 - n2;

			*dndu = ( dv2 * dn1 - dv1 * dn2) * invdet;
			*dndv = (-du2 * dn1 + du1 * dn2) * invdet;
			// Transform to global coordinates
			*dndu = normalize(Transform_InvApplyNormal(&meshDesc->trans, *dndu));
			*dndv = normalize(Transform_InvApplyNormal(&meshDesc->trans, *dndv));
		} else {
#endif
			*dndu = ZERO;
			*dndv = ZERO;
#if defined(PARAM_HAS_NORMALS_BUFFER)
		}
#endif
	}
}

// Used when hitting a surface
void BSDF_Init(
		__global BSDF *bsdf,
		//const bool fromL,
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global const uint* restrict meshTriLightDefsOffset,
#endif
		__global const Point* restrict vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global const Vector* restrict vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global const UV* restrict vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
		__global const Spectrum* restrict vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
		__global const float* restrict vertAlphas,
#endif
		__global const Triangle* restrict triangles,
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		Ray *ray,
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		const RayHit *rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float u0
#endif
#if defined(PARAM_HAS_VOLUMES)
		, __global PathVolumeInfo *volInfo
#endif
		MATERIALS_PARAM_DECL
		) {
	//bsdf->fromLight = fromL;
#if defined(PARAM_HAS_PASSTHROUGH)
	bsdf->hitPoint.passThroughEvent = u0;
#endif

#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
#else
	const float3 rayOrig = (float3)(ray->o.x, ray->o.y, ray->o.z);
	const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);
#endif
	const float3 hitPointP = rayOrig + rayHit->t * rayDir;
	VSTORE3F(hitPointP, &bsdf->hitPoint.p.x);
	VSTORE3F(-rayDir, &bsdf->hitPoint.fixedDir.x);

	const uint meshIndex = rayHit->meshIndex;
	const uint triangleIndex = rayHit->triangleIndex;

	__global const Mesh* restrict meshDesc = &meshDescs[meshIndex];
	__global const Point* restrict iVertices = &vertices[meshDesc->vertsOffset];
	__global const Triangle* restrict iTriangles = &triangles[meshDesc->trisOffset];

	// Save the scene object index
	bsdf->sceneObjectIndex = meshIndex;
	
	// Initialized world to local object space transformation
	bsdf->hitPoint.worldToLocal = meshDesc->trans.mInv;

	// Get the material
	const uint matIndex = sceneObjs[meshIndex].materialIndex;
	bsdf->materialIndex = matIndex;

	//--------------------------------------------------------------------------
	// Get face normal
	//--------------------------------------------------------------------------

	const float b1 = rayHit->b1;
	const float b2 = rayHit->b2;

	// Geometry normal expressed in local coordinates
	float3 geometryN = Mesh_GetGeometryNormal(iVertices, iTriangles, triangleIndex);
	// Transform to global coordinates
	geometryN = normalize(Transform_InvApplyNormal(&meshDesc->trans, geometryN));
	// Store the geometry normal
	VSTORE3F(geometryN, &bsdf->hitPoint.geometryN.x);

	// The shading normal
	float3 shadeN;
#if defined(PARAM_HAS_NORMALS_BUFFER)
	if (meshDesc->normalsOffset != NULL_INDEX) {
		__global const Vector* restrict iVertNormals = &vertNormals[meshDesc->normalsOffset];
		// Shading normal expressed in local coordinates
		shadeN = Mesh_InterpolateNormal(iVertNormals, iTriangles, triangleIndex, b1, b2);
		// Transform to global coordinates
		shadeN = normalize(Transform_InvApplyNormal(&meshDesc->trans, shadeN));
	} else
#endif
		shadeN = geometryN;
    VSTORE3F(shadeN, &bsdf->hitPoint.shadeN.x);

	//--------------------------------------------------------------------------
	// Set interior and exterior volumes
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_VOLUMES)
	bsdf->hitPoint.intoObject = (dot(rayDir, geometryN) < 0.f);

	PathVolumeInfo_SetHitPointVolumes(
			volInfo,
			&bsdf->hitPoint,
			Material_GetInteriorVolume(matIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
				, u0
#endif
			MATERIALS_PARAM),
			Material_GetExteriorVolume(matIndex, &bsdf->hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
				, u0
#endif
			MATERIALS_PARAM)
			MATERIALS_PARAM);
#endif

	//--------------------------------------------------------------------------
	// Get UV coordinate
	//--------------------------------------------------------------------------

	float2 hitPointUV;
#if defined(PARAM_HAS_UVS_BUFFER)
	if (meshDesc->uvsOffset != NULL_INDEX) {
		__global const UV* restrict iVertUVs = &vertUVs[meshDesc->uvsOffset];
		hitPointUV = Mesh_InterpolateUV(iVertUVs, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointUV = 0.f;
	VSTORE2F(hitPointUV, &bsdf->hitPoint.uv.u);

	//--------------------------------------------------------------------------
	// Get color value
	//--------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY) || defined(PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR)
	float3 hitPointColor;
#if defined(PARAM_HAS_COLS_BUFFER)
	if (meshDesc->colsOffset != NULL_INDEX) {
		__global const Spectrum* restrict iVertCols = &vertCols[meshDesc->colsOffset];
		hitPointColor = Mesh_InterpolateColor(iVertCols, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointColor = WHITE;
	VSTORE3F(hitPointColor, bsdf->hitPoint.color.c);
#endif

	//--------------------------------------------------------------------------
	// Get alpha value
	//--------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	float hitPointAlpha;
#if defined(PARAM_HAS_ALPHAS_BUFFER)
	if (meshDesc->alphasOffset != NULL_INDEX) {
		__global const float* restrict iVertAlphas = &vertAlphas[meshDesc->alphasOffset];
		hitPointAlpha = Mesh_InterpolateAlpha(iVertAlphas, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointAlpha = 1.f;
	bsdf->hitPoint.alpha = hitPointAlpha;
#endif

	//--------------------------------------------------------------------------

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	// Check if it is a light source
	bsdf->triangleLightSourceIndex = meshTriLightDefsOffset[meshIndex];
#endif

    //--------------------------------------------------------------------------
	// Build the local reference system
	//--------------------------------------------------------------------------

	float3 dndu, dndv, dpdu, dpdv;
	ExtMesh_GetDifferentials(
			meshDescs,
			vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
			vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
			vertUVs,
#endif
			triangles,
			meshIndex,
			triangleIndex,
			shadeN,
			&dpdu, &dpdv,
			&dndu, &dndv);
	
	//--------------------------------------------------------------------------
	// Apply bump or normal mapping
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_BUMPMAPS)
	VSTORE3F(dpdu, &bsdf->hitPoint.dpdu.x);
	VSTORE3F(dpdv, &bsdf->hitPoint.dpdv.x);
	VSTORE3F(dndu, &bsdf->hitPoint.dndu.x);
	VSTORE3F(dndv, &bsdf->hitPoint.dndv.x);
	Material_Bump(matIndex, &bsdf->hitPoint
			MATERIALS_PARAM);
	// Re-read the shadeN modified by Material_Bump()
	shadeN = VLOAD3F(&bsdf->hitPoint.shadeN.x);
	dpdu = VLOAD3F(&bsdf->hitPoint.dpdu.x);
	dpdv = VLOAD3F(&bsdf->hitPoint.dpdv.x);
#endif

	//--------------------------------------------------------------------------
	// Build the local reference system
	//--------------------------------------------------------------------------
	Frame_Set(&bsdf->frame, dpdu, dpdv, shadeN);

#if defined(PARAM_HAS_VOLUMES)
	bsdf->isVolume = false;
#endif
}

#if defined(PARAM_HAS_VOLUMES)
// Used when hitting a volume scatter point
void BSDF_InitVolume(
		__global BSDF *bsdf,
		__global const Material* restrict mats,
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		Ray *ray,
		const uint volumeIndex, const float t, const float passThroughEvent) {
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
#else
	const float3 rayOrig = (float3)(ray->o.x, ray->o.y, ray->o.z);
	const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);
#endif
	const float3 hitPointP = rayOrig + t * rayDir;
	VSTORE3F(hitPointP, &bsdf->hitPoint.p.x);
	const float3 shadeN = -rayDir;
	VSTORE3F(shadeN, &bsdf->hitPoint.fixedDir.x);

	bsdf->hitPoint.passThroughEvent = passThroughEvent;

	bsdf->sceneObjectIndex = NULL_INDEX;
	Matrix4x4_IdentityGlobal(&bsdf->hitPoint.worldToLocal);

	bsdf->materialIndex = volumeIndex;

	VSTORE3F(shadeN, &bsdf->hitPoint.geometryN.x);
	VSTORE3F(shadeN, &bsdf->hitPoint.shadeN.x);
#if defined(PARAM_HAS_BUMPMAPS)
	float3 dpdu, dpdv;
	CoordinateSystem(shadeN, &dpdu, &dpdv);
	VSTORE3F(dpdu, &bsdf->hitPoint.dpdu.x);
	VSTORE3F(dpdv, &bsdf->hitPoint.dpdv.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &bsdf->hitPoint.dndu.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &bsdf->hitPoint.dndv.x);
#endif

	bsdf->hitPoint.intoObject = true;
	bsdf->hitPoint.interiorVolumeIndex = volumeIndex;
	bsdf->hitPoint.exteriorVolumeIndex = volumeIndex;

	const uint iorTexIndex = (volumeIndex != NULL_INDEX) ? mats[volumeIndex].volume.iorTexIndex : NULL_INDEX;
	bsdf->hitPoint.interiorIorTexIndex = iorTexIndex;
	bsdf->hitPoint.exteriorIorTexIndex = iorTexIndex;

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY) || defined(PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR)
	VSTORE3F(WHITE, bsdf->hitPoint.color.c);
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	bsdf->hitPoint.alpha = 1.f;
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	bsdf->triangleLightSourceIndex = NULL_INDEX;
#endif

	VSTORE2F((float2)(0.f, 0.f), &bsdf->hitPoint.uv.u);

	bsdf->isVolume = true;

	// Build the local reference system
	Frame_SetFromZ(&bsdf->frame, shadeN);
}
#endif

float3 BSDF_Evaluate(__global BSDF *bsdf,
		const float3 generatedDir, BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	//const Vector &eyeDir = fromLight ? generatedDir : hitPoint.fixedDir;
	//const Vector &lightDir = fromLight ? hitPoint.fixedDir : generatedDir;
	const float3 eyeDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 lightDir = generatedDir;
	const float3 geometryN = VLOAD3F(&bsdf->hitPoint.geometryN.x);

	const float dotLightDirNG = dot(lightDir, geometryN);
	const float absDotLightDirNG = fabs(dotLightDirNG);
	const float dotEyeDirNG = dot(eyeDir, geometryN);
	const float absDotEyeDirNG = fabs(dotEyeDirNG);

#if defined(PARAM_HAS_VOLUMES)
	if (!bsdf->isVolume) {
		// These kind of tests make sense only for materials
#endif
		if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
				(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
			return BLACK;

		const float sideTest = dotEyeDirNG * dotLightDirNG;
		const BSDFEvent matEvent = Material_GetEventTypes(bsdf->materialIndex
				MATERIALS_PARAM);
		if (((sideTest > 0.f) && !(matEvent & REFLECT)) ||
				((sideTest < 0.f) && !(matEvent & TRANSMIT)))
			return BLACK;
#if defined(PARAM_HAS_VOLUMES)
	}
#endif

	__global Frame *frame = &bsdf->frame;
	const float3 localLightDir = Frame_ToLocal(frame, lightDir);
	const float3 localEyeDir = Frame_ToLocal(frame, eyeDir);
	const float3 result = Material_Evaluate(bsdf->materialIndex, &bsdf->hitPoint,
			localLightDir, localEyeDir,	event, directPdfW
			MATERIALS_PARAM);

	// Adjoint BSDF
//	if (fromLight) {
//		const float absDotLightDirNS = AbsDot(lightDir, shadeN);
//		const float absDotEyeDirNS = AbsDot(eyeDir, shadeN);
//		return result * ((absDotLightDirNS * absDotEyeDirNG) / (absDotEyeDirNS * absDotLightDirNG));
//	} else
		return result;
}

float3 BSDF_Sample(__global BSDF *bsdf, const float u0, const float u1,
		float3 *sampledDir, float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		MATERIALS_PARAM_DECL) {
	const float3 fixedDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, fixedDir);
	float3 localSampledDir;

	const float3 result = Material_Sample(bsdf->materialIndex, &bsdf->hitPoint,
			localFixedDir, &localSampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
			bsdf->hitPoint.passThroughEvent,
#endif
			pdfW, cosSampledDir, event,
			requestedEvent
			MATERIALS_PARAM);
	if (Spectrum_IsBlack(result))
		return 0.f;

	*sampledDir = Frame_ToWorld(&bsdf->frame, localSampledDir);

	// Adjoint BSDF
//	if (fromLight) {
//		const float absDotFixedDirNS = fabsf(localFixedDir.z);
//		const float absDotSampledDirNS = fabsf(localSampledDir.z);
//		const float absDotFixedDirNG = AbsDot(fixedDir, geometryN);
//		const float absDotSampledDirNG = AbsDot(*sampledDir, geometryN);
//		return result * ((absDotFixedDirNS * absDotSampledDirNG) / (absDotSampledDirNS * absDotFixedDirNG));
//	} else
		return result;
}

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
bool BSDF_IsLightSource(__global BSDF *bsdf) {
	return (bsdf->triangleLightSourceIndex != NULL_INDEX);
}

float3 BSDF_GetEmittedRadiance(__global BSDF *bsdf, float *directPdfA
		LIGHTS_PARAM_DECL) {
	const uint triangleLightSourceIndex = bsdf->triangleLightSourceIndex;
	if (triangleLightSourceIndex == NULL_INDEX)
		return BLACK;
	else
		return IntersectableLight_GetRadiance(&lights[triangleLightSourceIndex],
				&bsdf->hitPoint, directPdfA
				LIGHTS_PARAM);
}
#endif

#if defined(PARAM_HAS_PASSTHROUGH)
float3 BSDF_GetPassThroughTransparency(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, VLOAD3F(&bsdf->hitPoint.fixedDir.x));

	return Material_GetPassThroughTransparency(bsdf->materialIndex,
			&bsdf->hitPoint, localFixedDir, bsdf->hitPoint.passThroughEvent
			MATERIALS_PARAM);
}
#endif

//------------------------------------------------------------------------------
// Shadow catcher related functions
//------------------------------------------------------------------------------

bool BSDF_IsShadowCatcher(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const uint matIndex = bsdf->materialIndex;

	return (matIndex == NULL_INDEX) ? false : mats[matIndex].isShadowCatcher;
}

bool BSDF_IsShadowCatcherOnlyInfiniteLights(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const uint matIndex = bsdf->materialIndex;

	return (matIndex == NULL_INDEX) ? false : (mats[matIndex].isShadowCatcher && mats[matIndex].isShadowCatcherOnlyInfiniteLights);
}

float3 BSDF_ShadowCatcherSample(__global BSDF *bsdf,
		float3 *sampledDir, float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	// Just continue to trace the ray
	*sampledDir = -VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	*cosSampledDir = fabs(dot(*sampledDir, VLOAD3F(&bsdf->hitPoint.geometryN.x)));

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	const float3 result = WHITE;

	// Adjoint BSDF
//	if (hitPoint.fromLight) {
//		const float absDotFixedDirNG = AbsDot(hitPoint.fixedDir, hitPoint.geometryN);
//		const float absDotSampledDirNG = AbsDot(*sampledDir, hitPoint.geometryN);
//		return result * (absDotSampledDirNG / absDotFixedDirNG);
//	} else
		return result;
}
