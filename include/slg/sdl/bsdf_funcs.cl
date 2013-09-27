#line 2 "bsdf_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

void BSDF_Init(
		__global BSDF *bsdf,
		//const bool fromL,
		__global Mesh *meshDescs,
		__global uint *meshMats,
#if (PARAM_DL_LIGHT_COUNT > 0)
		__global uint *meshTriLightDefsOffset,
#endif
		__global Point *vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global Vector *vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global UV *vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
		__global Spectrum *vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
		__global float *vertAlphas,
#endif
		__global Triangle *triangles,
		__global Ray *ray,
		__global RayHit *rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float u0
#endif
#if defined(PARAM_HAS_BUMPMAPS) || defined(PARAM_HAS_NORMALMAPS)
		MATERIALS_PARAM_DECL
#endif
		) {
	//bsdf->fromLight = fromL;
#if defined(PARAM_HAS_PASSTHROUGH)
	bsdf->hitPoint.passThroughEvent = u0;
#endif

	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
	const float3 hitPointP = rayOrig + rayHit->t * rayDir;
	VSTORE3F(hitPointP, &bsdf->hitPoint.p.x);
	VSTORE3F(-rayDir, &bsdf->hitPoint.fixedDir.x);

	const uint meshIndex = rayHit->meshIndex;
	const uint triangleIndex = rayHit->triangleIndex;

	__global Mesh *meshDesc = &meshDescs[meshIndex];
	__global Point *iVertices = &vertices[meshDesc->vertsOffset];
	__global Triangle *iTriangles = &triangles[meshDesc->trisOffset];

	// Get the material
	const uint matIndex = meshMats[meshIndex];
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
		__global Vector *iVertNormals = &vertNormals[meshDesc->normalsOffset];
		// Shading normal expressed in local coordinates
		shadeN = Mesh_InterpolateNormal(iVertNormals, iTriangles, triangleIndex, b1, b2);
		// Transform to global coordinates
		shadeN = normalize(Transform_InvApplyNormal(&meshDesc->trans, shadeN));
	} else
#endif
		shadeN = geometryN;
	// Shade normal can not yet be stored because of normal and bump mapping

	//--------------------------------------------------------------------------
	// Get UV coordinate
	//--------------------------------------------------------------------------

	float2 hitPointUV;
#if defined(PARAM_HAS_UVS_BUFFER)
	if (meshDesc->uvsOffset != NULL_INDEX) {
		__global UV *iVertUVs = &vertUVs[meshDesc->uvsOffset];
		hitPointUV = Mesh_InterpolateUV(iVertUVs, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointUV = 0.f;
	VSTORE2F(hitPointUV, &bsdf->hitPoint.uv.u);

	//--------------------------------------------------------------------------
	// Get color value
	//--------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY)
	float3 hitPointColor;
#if defined(PARAM_HAS_COLS_BUFFER)
	if (meshDesc->colsOffset != NULL_INDEX) {
		__global Spectrum *iVertCols = &vertCols[meshDesc->colsOffset];
		hitPointColor = Mesh_InterpolateColor(iVertCols, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointColor = WHITE;
	VSTORE3F(hitPointColor, &bsdf->hitPoint.color.r);
#endif

	//--------------------------------------------------------------------------
	// Get alpha value
	//--------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	float hitPointAlpha;
#if defined(PARAM_HAS_ALPHAS_BUFFER)
	if (meshDesc->colsOffset != NULL_INDEX) {
		__global float *iVertAlphas = &vertAlphas[meshDesc->alphasOffset];
		hitPointAlpha = Mesh_InterpolateAlpha(iVertAlphas, iTriangles, triangleIndex, b1, b2);
	} else
#endif
		hitPointAlpha = 1.f;
	bsdf->hitPoint.alpha = hitPointAlpha;
#endif

	//--------------------------------------------------------------------------

#if (PARAM_DL_LIGHT_COUNT > 0)
	// Check if it is a light source
	bsdf->triangleLightSourceIndex = meshTriLightDefsOffset[meshIndex];
#endif

#if defined(PARAM_HAS_BUMPMAPS) || defined(PARAM_HAS_NORMALMAPS)
	__global Material *mat = &mats[matIndex];

#if defined(PARAM_HAS_NORMALMAPS)
	//--------------------------------------------------------------------------
	// Check if I have to apply normal mapping
	//--------------------------------------------------------------------------

	const float3 normalColor = Material_GetNormalTexValue(mat, &bsdf->hitPoint
		MATERIALS_PARAM);

	if (!Spectrum_IsBlack(normalColor)) {
		const float3 xyz = 2.f * normalColor - 1.f;

		float3 v1, v2;
		CoordinateSystem(shadeN, &v1, &v2);
		shadeN = normalize((float3)(
				v1.x * xyz.x + v2.x * xyz.y + shadeN.x * xyz.z,
				v1.y * xyz.x + v2.y * xyz.y + shadeN.y * xyz.z,
				v1.z * xyz.x + v2.z * xyz.y + shadeN.z * xyz.z));
	}
#endif

#if defined(PARAM_HAS_BUMPMAPS)
	//--------------------------------------------------------------------------
	// Check if I have to apply bump mapping
	//--------------------------------------------------------------------------

	const float2 bumpUV = Material_GetBumpTexValue(mat, &bsdf->hitPoint
		MATERIALS_PARAM);

	if ((bumpUV.s0 != 0.f) || (bumpUV.s1 != 0.f)) {
		float3 v1, v2;
		CoordinateSystem(shadeN, &v1, &v2);
		shadeN = normalize((float3)(
				v1.x * bumpUV.s0 + v2.x * bumpUV.s1 + shadeN.x,
				v1.y * bumpUV.s0 + v2.y * bumpUV.s1 + shadeN.y,
				v1.z * bumpUV.s0 + v2.z * bumpUV.s1 + shadeN.z));
	}
#endif
#endif

	Frame_SetFromZ(&bsdf->frame, shadeN);

	VSTORE3F(shadeN, &bsdf->hitPoint.shadeN.x);
}

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

	if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
			(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	__global Material *mat = &mats[bsdf->materialIndex];
	const float sideTest = dotEyeDirNG * dotLightDirNG;
	const BSDFEvent matEvent = Material_GetEventTypes(mat
			MATERIALS_PARAM);
	if (((sideTest > 0.f) && !(matEvent & REFLECT)) ||
			((sideTest < 0.f) && !(matEvent & TRANSMIT)))
		return BLACK;

	__global Frame *frame = &bsdf->frame;
	const float3 localLightDir = Frame_ToLocal(frame, lightDir);
	const float3 localEyeDir = Frame_ToLocal(frame, eyeDir);
	const float3 result = Material_Evaluate(mat, &bsdf->hitPoint,
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
		float3 *sampledDir, float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	const float3 fixedDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, fixedDir);
	float3 localSampledDir;

	const float3 result = Material_Sample(&mats[bsdf->materialIndex], &bsdf->hitPoint,
			localFixedDir, &localSampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
			bsdf->hitPoint.passThroughEvent,
#endif
			pdfW, cosSampledDir, event
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

BSDFEvent BSDF_GetEventTypes(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_GetEventTypes(&mats[bsdf->materialIndex]
			MATERIALS_PARAM);
}

bool BSDF_IsDelta(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsDelta(&mats[bsdf->materialIndex]
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

#if (PARAM_DL_LIGHT_COUNT > 0)
bool BSDF_IsLightSource(__global BSDF *bsdf) {
	return (bsdf->triangleLightSourceIndex != NULL_INDEX);
}

float3 BSDF_GetEmittedRadiance(__global BSDF *bsdf,
		__global TriangleLight *triLightDefs, float *directPdfA
		MATERIALS_PARAM_DECL) {
	const uint triangleLightSourceIndex = bsdf->triangleLightSourceIndex;
	if (triangleLightSourceIndex == NULL_INDEX)
		return BLACK;
	else
		return TriangleLight_GetRadiance(&triLightDefs[triangleLightSourceIndex],
				&bsdf->hitPoint, directPdfA
				MATERIALS_PARAM);
}
#endif

#if defined(PARAM_HAS_PASSTHROUGH)
float3 BSDF_GetPassThroughTransparency(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, VLOAD3F(&bsdf->hitPoint.fixedDir.x));

	return Material_GetPassThroughTransparency(&mats[bsdf->materialIndex],
			&bsdf->hitPoint, localFixedDir, bsdf->hitPoint.passThroughEvent
			MATERIALS_PARAM);
}
#endif
