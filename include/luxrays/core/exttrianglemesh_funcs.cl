#line 2 "exttrianglemesh_funcs.cl"

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

OPENCL_FORCE_INLINE float3 ExtMesh_GetGeometryNormal(
		__global const Transform* restrict localToWorld,
		const uint meshIndex, const uint triangleIndex
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	__global const Normal* restrict tn = &triNormals[meshDesc->triNormalsOffset];

	float3 geometryN = VLOAD3F(&tn[triangleIndex].x);

	// geometryN is already in world coordinates for TYPE_EXT_TRIANGLE
	if (meshDesc->type != TYPE_EXT_TRIANGLE) {
		// Transform to global coordinates (normal requires Inv transformation)
		geometryN = normalize(Transform_InvApplyNormal(localToWorld, geometryN));
	}

	return geometryN;
}

OPENCL_FORCE_INLINE float3 ExtMesh_GetInterpolateNormal(
		__global const Transform* restrict localToWorld,
		const uint meshIndex, const uint triangleIndex,
		const float b1, const float b2
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];

	float3 interpolatedN;
	if (meshDesc->normalsOffset != NULL_INDEX) {
		// Shading normal expressed in local coordinates
		__global const Triangle* restrict tri = &triangles[meshDesc->trisOffset + triangleIndex];
		__global const Normal* restrict vns = &vertNormals[meshDesc->normalsOffset];
		const float3 n0 = VLOAD3F(&vns[tri->v[0]].x);
		const float3 n1 = VLOAD3F(&vns[tri->v[1]].x);
		const float3 n2 = VLOAD3F(&vns[tri->v[2]].x);

		const float b0 = 1.f - b1 - b2;
		interpolatedN =  Triangle_InterpolateNormal(n0, n1, n2, b0, b1, b2);

		// interpolatedN is already in world coordinates for TYPE_EXT_TRIANGLE
		if (meshDesc->type != TYPE_EXT_TRIANGLE) {
			// Transform to global coordinates (normal requires Inv transformation)
			interpolatedN = normalize(Transform_InvApplyNormal(localToWorld, interpolatedN));
		}
	} else
		interpolatedN = ExtMesh_GetGeometryNormal(localToWorld, meshIndex, triangleIndex EXTMESH_PARAM);

	return interpolatedN;
}

OPENCL_FORCE_INLINE float2 ExtMesh_GetInterpolateUV(
		const uint meshIndex, const uint triangleIndex,
		const float b1, const float b2
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	
	float2 uv = 0.f;
	if (meshDesc->uvsOffset != NULL_INDEX) {
		__global const UV* restrict uvs = &vertUVs[meshDesc->uvsOffset];
		__global const Triangle* restrict tri = &triangles[meshDesc->trisOffset + triangleIndex];

		const float2 uv0 = VLOAD2F(&uvs[tri->v[0]].u);
		const float2 uv1 = VLOAD2F(&uvs[tri->v[1]].u);
		const float2 uv2 = VLOAD2F(&uvs[tri->v[2]].u);

		const float b0 = 1.f - b1 - b2;
		uv = Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);
	}

	return uv;
}

OPENCL_FORCE_INLINE float3 ExtMesh_GetInterpolateColor(
		const uint meshIndex, const uint triangleIndex,
		const float b1, const float b2
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	
	float3 c = 0.f;
	if (meshDesc->colsOffset != NULL_INDEX) {
		__global const Spectrum* restrict vcs = &vertCols[meshDesc->colsOffset];
		__global const Triangle* restrict tri = &triangles[meshDesc->trisOffset + triangleIndex];
		const float3 rgb0 = VLOAD3F(vcs[tri->v[0]].c);
		const float3 rgb1 = VLOAD3F(vcs[tri->v[1]].c);
		const float3 rgb2 = VLOAD3F(vcs[tri->v[2]].c);

		const float b0 = 1.f - b1 - b2;
		c = Triangle_InterpolateColor(rgb0, rgb1, rgb2, b0, b1, b2);
	}
	
	return c;
}

OPENCL_FORCE_INLINE float ExtMesh_GetInterpolateAlpha(
		const uint meshIndex, const uint triangleIndex,
		const float b1, const float b2
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	
	float a = 0.f;
	if (meshDesc->alphasOffset != NULL_INDEX) {
		__global const float* restrict vas = &vertAlphas[meshDesc->alphasOffset];
		__global const Triangle* restrict tri = &triangles[meshDesc->trisOffset + triangleIndex];
		const float a0 = vas[tri->v[0]];
		const float a1 = vas[tri->v[1]];
		const float a2 = vas[tri->v[2]];

		const float b0 = 1.f - b1 - b2;
		a =  Triangle_InterpolateAlpha(a0, a1, a2, b0, b1, b2);
	}

	return a;
}

OPENCL_FORCE_INLINE void ExtMesh_GetDifferentials(
		__global const Transform* restrict localToWorld,
		const uint meshIndex,
		const uint triangleIndex,
		float3 shadeNormal,
		float3 *dpdu, float3 *dpdv,
        float3 *dndu, float3 *dndv
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
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

		// dp1 and dp2 are already in world coordinates for TYPE_EXT_TRIANGLE
		if (meshDesc->type != TYPE_EXT_TRIANGLE) {
			// Transform to global coordinates
			dp1 = Transform_ApplyVector(localToWorld, dp1);
			dp2 = Transform_ApplyVector(localToWorld, dp2);
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
			__global const Normal* restrict iVertNormals = &vertNormals[meshDesc->normalsOffset];
			// Shading normals expressed in local coordinates
			const float3 n0 = VLOAD3F(&iVertNormals[tri->v[0]].x);
			const float3 n1 = VLOAD3F(&iVertNormals[tri->v[1]].x);
			const float3 n2 = VLOAD3F(&iVertNormals[tri->v[2]].x);
			const float3 dn1 = n0 - n2;
			const float3 dn2 = n1 - n2;

			*dndu = ( dv2 * dn1 - dv1 * dn2) * invdet;
			*dndv = (-du2 * dn1 + du1 * dn2) * invdet;
			
			// dndu and dndv are already in world coordinates for TYPE_EXT_TRIANGLE
			if (meshDesc->type != TYPE_EXT_TRIANGLE) {
				// Transform to global coordinates (normal requires Inv transformation)
				*dndu = Transform_InvApplyNormal(localToWorld, *dndu);
				*dndv = Transform_InvApplyNormal(localToWorld, *dndv);
			}
		} else {
			*dndu = ZERO;
			*dndv = ZERO;
		}
	}
}

OPENCL_FORCE_INLINE void ExtMesh_Sample(
		__global const Transform* restrict localToWorld,
		const uint meshIndex, const uint triangleIndex,
		const float u0, const float u1,
		float3 *samplePoint, float *b0, float *b1, float *b2
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	__global const Point* restrict triVerts = &vertices[meshDesc->vertsOffset];
	__global const Triangle* restrict tri = &triangles[meshDesc->trisOffset + triangleIndex];

	// Vertices are in local object space
	float3 p0 = VLOAD3F(&triVerts[tri->v[0]].x);
	float3 p1 = VLOAD3F(&triVerts[tri->v[1]].x);
	float3 p2 = VLOAD3F(&triVerts[tri->v[2]].x);
	*samplePoint = Triangle_Sample(
			p0, p1, p2,
			u0, u1,
			b0, b1, b2);

	// samplePoint is already in world coordinates for TYPE_EXT_TRIANGLE
	if (meshDesc->type != TYPE_EXT_TRIANGLE) {
		// Transform to global coordinates
		*samplePoint = Transform_ApplyPoint(localToWorld, *samplePoint);
	}
}

OPENCL_FORCE_INLINE void ExtMesh_GetLocal2World(const float time,
		const uint meshIndex, const uint triangleIndex,
		__global Transform *local2World
		EXTMESH_PARAM_DECL) {
	// Initialized world to local object space transformation
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];

	switch (meshDesc->type) {
		case TYPE_EXT_TRIANGLE:
			*local2World = meshDesc->triangle.appliedTrans;
			break;
		case TYPE_EXT_TRIANGLE_INSTANCE:
			*local2World = meshDesc->instance.trans;
			break;
		case TYPE_EXT_TRIANGLE_MOTION: {
			Matrix4x4 m;

			MotionSystem_Sample(&meshDesc->motion.motionSystem, time, interpolatedTransforms, &m);
			local2World->mInv = m;

			MotionSystem_SampleInverse(&meshDesc->motion.motionSystem, time, interpolatedTransforms, &m);
			local2World->m = m;

			break;
		}
		default:
			break;
	}
}
