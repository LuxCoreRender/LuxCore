#line 2 "bsdf_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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
#if defined(PARAM_ACCEL_MQBVH)
		__global uint *meshFirstTriangleOffset,
		__global Mesh *meshDescs,
#endif
		__global Material *mats,
		__global Texture *texs,
		__global uint *meshMats,
		__global uint *meshIDs,
		__global Vector *vertNormals,
		__global Point *vertices,
		__global Triangle *triangles,
		__global Ray *ray,
		__global RayHit *rayHit
#if defined(PARAM_HAS_IMAGEMAPS)
		, __global UV *vertUVs
#endif
#if defined(PARAM_HAS_PASSTHROUGHT)
		, const float u0
#endif
		) {
	//bsdf->fromLight = fromL;
#if defined(PARAM_HAS_PASSTHROUGHT)
	bsdf->passThroughEvent = u0;
#endif

	const float3 rayOrig = vload3(0, &ray->o.x);
	const float3 rayDir = vload3(0, &ray->d.x);
	vstore3(rayOrig + rayHit->t * rayDir, 0, &bsdf->hitPoint.x);
	vstore3(-rayDir, 0, &bsdf->fixedDir.x);

	const uint currentTriangleIndex = rayHit->index;
	const uint meshIndex = meshIDs[currentTriangleIndex];

#if defined(PARAM_ACCEL_MQBVH)
	TODO
	__global Mesh *meshDesc = &meshDescs[meshIndex];
	__global Point *iVertices = &vertices[meshDesc->vertsOffset];
	__global Vector *iVertNormals = &vertNormals[meshDesc->vertsOffset];
#if defined(PARAM_HAS_TEXTUREMAPS)
	__global UV *iVertUVs = &vertUVs[meshDesc->vertsOffset];
#endif
	bsdf->triangles = &triangles[meshDesc->trisOffset];
	bsdf->triIndex = currentTriangleIndex - meshFirstTriangleOffset[meshIndex];
#endif

	// Get the material
	const uint matIndex = meshMats[meshIndex];
	bsdf->materialIndex = matIndex;

	// Interpolate face normal
	vstore3(Mesh_GetGeometryNormal(vertices, triangles, currentTriangleIndex), 0, &bsdf->geometryN.x);
#if defined(PARAM_ACCEL_MQBVH)
	Mesh_InterpolateNormal(iVertNormals, iTriangles, triangleID, hitPointB1, hitPointB2, &N);
	TransformNormal(meshDesc->invTrans, &N);
#else
	float3 shadeN = Mesh_InterpolateNormal(vertNormals, triangles, currentTriangleIndex, rayHit->b1, rayHit->b2);
#endif

#if (PARAM_DL_LIGHT_COUNT > 0)
	// Check if it is a light source
	bsdf->triangleLightSourceIndex = mats[matIndex].emitTexIndex;
#endif

//	// Interpolate UV coordinates
//	vstore2(Mesh_InterpolateTriUV(triIndex, rayHit->b1, rayHit->b2), 0, bsdf->hitPointUV.u);

//	// Check if I have to apply bump mapping
//	if (material->HasNormalTex()) {
//		// Apply normal mapping
//		const Texture *nm = material->GetNormalTexture();
//		const Spectrum color = nm->GetColorValue(hitPointUV);
//
//		const float x = 2.f * (color.r - 0.5f);
//		const float y = 2.f * (color.g - 0.5f);
//		const float z = 2.f * (color.b - 0.5f);
//
//		Vector v1, v2;
//		CoordinateSystem(Vector(shadeN), &v1, &v2);
//		shadeN = Normalize(Normal(
//				v1.x * x + v2.x * y + shadeN.x * z,
//				v1.y * x + v2.y * y + shadeN.y * z,
//				v1.z * x + v2.z * y + shadeN.z * z));
//	}

//	// Check if I have to apply normal mapping
//	if (material->HasBumpTex()) {
//		// Apply normal mapping
//		const Texture *bm = material->GetBumpTexture();
//		const UV &dudv = bm->GetDuDv();
//
//		const float b0 = bm->GetGreyValue(hitPointUV);
//
//		const UV uvdu(hitPointUV.u + dudv.u, hitPointUV.v);
//		const float bu = bm->GetGreyValue(uvdu);
//
//		const UV uvdv(hitPointUV.u, hitPointUV.v + dudv.v);
//		const float bv = bm->GetGreyValue(uvdv);
//
//		const Vector bump(bu - b0, bv - b0, 1.f);
//
//		Vector v1, v2;
//		CoordinateSystem(Vector(shadeN), &v1, &v2);
//		shadeN = Normalize(Normal(
//				v1.x * bump.x + v2.x * bump.y + shadeN.x * bump.z,
//				v1.y * bump.x + v2.y * bump.y + shadeN.y * bump.z,
//				v1.z * bump.x + v2.z * bump.y + shadeN.z * bump.z));
//	}

	
	Frame_SetFromZ(&bsdf->frame, shadeN);

	vstore3(shadeN, 0, &bsdf->shadeN.x);
}

float3 BSDF_Sample(
		__global BSDF *bsdf,
		__global Material *mats,
		__global Texture *texs,
#if defined(PARAM_HAS_IMAGEMAPS)
		__global ImageMap *imageMapDescs,
#if defined(PARAM_IMAGEMAPS_PAGE_0)
		__global float *imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
		__global float *imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
		__global float *imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
		__global float *imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
		__global float *imageMapBuff4,
#endif
#endif
		const float u0, const float u1,
		float3 *sampledDir, float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	const float3 fixedDir = vload3(0, &bsdf->fixedDir.x);
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, fixedDir);
	float3 localSampledDir;

	const float3 result = Material_Sample(
			&mats[bsdf->materialIndex], texs,
			(float2)(0.f, 0.f), localFixedDir, &localSampledDir,
			u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
			bsdf->passThroughEvent,
#endif
			pdfW, cosSampledDir, event);
	if (all(isequal(result, BLACK)))
		return 0.f;

	*sampledDir = Frame_ToWorld(&bsdf->frame, localSampledDir);

//	// Adjoint BSDF
//	if (fromLight) {
//		const float absDotFixedDirNS = fabsf(localFixedDir.z);
//		const float absDotSampledDirNS = fabsf(localSampledDir.z);
//		const float absDotFixedDirNG = AbsDot(fixedDir, geometryN);
//		const float absDotSampledDirNG = AbsDot(*sampledDir, geometryN);
//		return result * ((absDotFixedDirNS * absDotSampledDirNG) / (absDotSampledDirNS * absDotFixedDirNG));
//	} else
		return result;
}

#if (PARAM_DL_LIGHT_COUNT > 0)
float3 BSDF_GetEmittedRadiance(__global BSDF *bsdf) {
	//TriangleLight_GetRadiance(scene, fixedDir, hitPointB1, hitPointB2);
	return WHITE;
}
#endif
