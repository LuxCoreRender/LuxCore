#line 2 "exttrianglemesh_funcs.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

	switch (meshDesc->type) {
		case TYPE_EXT_TRIANGLE: {
			// geometryN is already in world coordinates for TYPE_EXT_TRIANGLE
			// and
			// pre-computed geometry normals already factor appliedTransSwapsHandedness
			break;
		}
		case TYPE_EXT_TRIANGLE_INSTANCE: {
			// Transform to global coordinates
			geometryN = (meshDesc->instance.transSwapsHandedness ? -1.f : 1.f) * normalize(Transform_ApplyNormal(localToWorld, geometryN));
			break;
		}
		case TYPE_EXT_TRIANGLE_MOTION: {
			const bool swapsHandedness = Transform_SwapsHandedness(localToWorld); 
			// Transform to global coordinates
			geometryN = (swapsHandedness ? -1.f : 1.f) * normalize(Transform_ApplyNormal(localToWorld, geometryN));
			break;
		}
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

		switch (meshDesc->type) {
			case TYPE_EXT_TRIANGLE: {
				// interpolatedN is already in world coordinates for TYPE_EXT_TRIANGLE
				interpolatedN = (meshDesc->triangle.appliedTransSwapsHandedness ? -1.f : 1.f) * interpolatedN;
				break;
			}
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				// Transform to global coordinates
				interpolatedN = (meshDesc->instance.transSwapsHandedness ? -1.f : 1.f) * normalize(Transform_ApplyNormal(localToWorld, interpolatedN));
				break;
			}
			case TYPE_EXT_TRIANGLE_MOTION: {
				const bool swapsHandedness = Transform_SwapsHandedness(localToWorld); 
				// Transform to global coordinates
				interpolatedN = (swapsHandedness ? -1.f : 1.f) * normalize(Transform_ApplyNormal(localToWorld, interpolatedN));
				break;
			}
		}
	} else
		interpolatedN = ExtMesh_GetGeometryNormal(localToWorld, meshIndex, triangleIndex EXTMESH_PARAM);

	return interpolatedN;
}

OPENCL_FORCE_INLINE float2 ExtMesh_GetInterpolateUV(
		const uint meshIndex, const uint triangleIndex,
		const float b1, const float b2, const uint dataIndex
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	
	float2 uv = MAKE_FLOAT2(0.f, 0.f);
	if (meshDesc->uvsOffset[dataIndex] != NULL_INDEX) {
		__global const UV* restrict uvs = &vertUVs[meshDesc->uvsOffset[dataIndex]];
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
		const float b1, const float b2, const uint dataIndex
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];

	float3 c = WHITE;
	if (meshDesc->colsOffset[dataIndex] != NULL_INDEX) {
		__global const Spectrum* restrict vcs = &vertCols[meshDesc->colsOffset[dataIndex]];
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
		const float b1, const float b2, const uint dataIndex
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	
	float a = 1.f;
	if (meshDesc->alphasOffset[dataIndex] != NULL_INDEX) {
		__global const float* restrict vas = &vertAlphas[meshDesc->alphasOffset[dataIndex]];
		__global const Triangle* restrict tri = &triangles[meshDesc->trisOffset + triangleIndex];
		const float a0 = vas[tri->v[0]];
		const float a1 = vas[tri->v[1]];
		const float a2 = vas[tri->v[2]];

		const float b0 = 1.f - b1 - b2;
		a =  Triangle_InterpolateAlpha(a0, a1, a2, b0, b1, b2);
	}

	return a;
}

OPENCL_FORCE_INLINE float ExtMesh_GetInterpolateVertexAOV(
		const uint meshIndex, const uint triangleIndex,
		const float b1, const float b2, const uint dataIndex
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	
	float v = 0.f;
	if (meshDesc->vertexAOVOffset[dataIndex] != NULL_INDEX) {
		__global const float* restrict vs = &vertexAOVs[meshDesc->vertexAOVOffset[dataIndex]];
		__global const Triangle* restrict tri = &triangles[meshDesc->trisOffset + triangleIndex];
		const float v0 = vs[tri->v[0]];
		const float v1 = vs[tri->v[1]];
		const float v2 = vs[tri->v[2]];

		const float b0 = 1.f - b1 - b2;
		v =  Triangle_InterpolateVertexAOV(v0, v1, v2, b0, b1, b2);
	}

	return v;
}

OPENCL_FORCE_INLINE float ExtMesh_GetTriAOV(
		const uint meshIndex, const uint triangleIndex,
		const uint dataIndex
		EXTMESH_PARAM_DECL) {
	__global const ExtMesh* restrict meshDesc = &meshDescs[meshIndex];
	
	float t = 0.f;
	if (meshDesc->triAOVOffset[dataIndex] != NULL_INDEX) {
		__global const float* restrict ts = &triAOVs[meshDesc->triAOVOffset[dataIndex]];
		
		t = ts[triangleIndex];
	}

	return t;
}

OPENCL_FORCE_INLINE void ExtMesh_GetDifferentials(
		__global const Transform* restrict localToWorld,
		const uint meshIndex,
		const uint triangleIndex,
		float3 shadeNormal,
		const uint dataIndex,
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
	if (meshDesc->uvsOffset[dataIndex] != NULL_INDEX) {
		// Ok, UV coordinates are available, use them to build the reference
		// system around the shading normal.

		__global const UV* restrict iVertUVs = &vertUVs[meshDesc->uvsOffset[dataIndex]];
		uv0 = VLOAD2F(&iVertUVs[vi0].u);
		uv1 = VLOAD2F(&iVertUVs[vi1].u);
		uv2 = VLOAD2F(&iVertUVs[vi2].u);
	} else {
		uv0 = MAKE_FLOAT2(.5f, .5f);
		uv1 = MAKE_FLOAT2(.5f, .5f);
		uv2 = MAKE_FLOAT2(.5f, .5f);
	}

	// Compute deltas for triangle partial derivatives
	const float du1 = uv0.x - uv2.x;
	const float du2 = uv1.x - uv2.x;
	const float dv1 = uv0.y - uv2.y;
	const float dv2 = uv1.y - uv2.y;
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
				// Transform to global coordinates
				*dndu = Transform_ApplyNormal(localToWorld, *dndu);
				*dndv = Transform_ApplyNormal(localToWorld, *dndv);
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
