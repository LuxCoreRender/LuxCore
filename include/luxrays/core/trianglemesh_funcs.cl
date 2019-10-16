#line 2 "trianglemesh_funcs.cl"

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

OPENCL_FORCE_INLINE float3 Mesh_GetGeometryNormal(__global const Point* restrict vertices,
		__global const Triangle* restrict triangles, const uint triIndex) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float3 p0 = VLOAD3F(&vertices[tri->v[0]].x);
	const float3 p1 = VLOAD3F(&vertices[tri->v[1]].x);
	const float3 p2 = VLOAD3F(&vertices[tri->v[2]].x);

	return Triangle_GetGeometryNormal(p0, p1, p2);
}

OPENCL_FORCE_INLINE float3 Mesh_InterpolateNormal(__global const Vector* restrict normals,
		__global const Triangle* restrict triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float3 n0 = VLOAD3F(&normals[tri->v[0]].x);
	const float3 n1 = VLOAD3F(&normals[tri->v[1]].x);
	const float3 n2 = VLOAD3F(&normals[tri->v[2]].x);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateNormal(n0, n1, n2, b0, b1, b2);
}

OPENCL_FORCE_INLINE float2 Mesh_InterpolateUV(__global const UV* restrict vertUVs,
		__global const Triangle *triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float2 uv0 = VLOAD2F(&vertUVs[tri->v[0]].u);
	const float2 uv1 = VLOAD2F(&vertUVs[tri->v[1]].u);
	const float2 uv2 = VLOAD2F(&vertUVs[tri->v[2]].u);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);
}

OPENCL_FORCE_INLINE float3 Mesh_InterpolateColor(__global const Spectrum* restrict vertCols,
		__global const Triangle* restrict triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float3 rgb0 = VLOAD3F(vertCols[tri->v[0]].c);
	const float3 rgb1 = VLOAD3F(vertCols[tri->v[1]].c);
	const float3 rgb2 = VLOAD3F(vertCols[tri->v[2]].c);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateColor(rgb0, rgb1, rgb2, b0, b1, b2);
}

OPENCL_FORCE_INLINE float Mesh_InterpolateAlpha(__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float a0 = vertAlphas[tri->v[0]];
	const float a1 = vertAlphas[tri->v[1]];
	const float a2 = vertAlphas[tri->v[2]];

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateAlpha(a0, a1, a2, b0, b1, b2);
}

OPENCL_FORCE_NOT_INLINE void ExtMesh_GetDifferentials(
		__global const Mesh* restrict meshDescs,
		__global const Point* restrict vertices,
		__global const Vector* restrict vertNormals,
		__global const UV* restrict vertUVs,
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
	if (meshDesc->uvsOffset != NULL_INDEX) {
		// Ok, UV coordinates are available, use them to build the reference
		// system around the shading normal.

		__global const UV* restrict iVertUVs = &vertUVs[meshDesc->uvsOffset];
		uv0 = VLOAD2F(&iVertUVs[vi0].u);
		uv1 = VLOAD2F(&iVertUVs[vi1].u);
		uv2 = VLOAD2F(&iVertUVs[vi2].u);
	} else {
		uv0 = (float2)(.5f, .5f);
		uv1 = (float2)(.5f, .5f);
		uv2 = (float2)(.5f, .5f);
	}

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
		
		float3 dp1 = p0 - p2;
		float3 dp2 = p1 - p2;

		if (meshDesc->type != TYPE_EXT_TRIANGLE) {
			// Transform to global coordinates
			dp1 = Transform_ApplyVector(&meshDesc->trans, dp1);
			dp2 = Transform_ApplyVector(&meshDesc->trans, dp2);
		}

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
			
			if (meshDesc->type != TYPE_EXT_TRIANGLE) {
				// Transform to global coordinates
				*dndu = normalize(Transform_ApplyNormal(&meshDesc->trans, *dndu));
				*dndv = normalize(Transform_ApplyNormal(&meshDesc->trans, *dndv));
			}
		} else {
			*dndu = ZERO;
			*dndv = ZERO;
		}
	}
}
