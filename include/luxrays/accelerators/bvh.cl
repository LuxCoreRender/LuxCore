#line 2 "bvh_kernel.cl"

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

#define BVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define BVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)
#if (BVH_NODES_PAGE_COUNT > 1)
#define BVHNodeData_GetPageIndex(nodeData) (((nodeData) & 0x70000000u) >> 28)
#define BVHNodeData_GetNodeIndex(nodeData) ((nodeData) & 0x0fffffffu)
#endif

#if (BVH_NODES_PAGE_COUNT > 1)
void NextNode(uint *pageIndex, uint *nodeIndex) {
	++(*nodeIndex);
	if (*nodeIndex >= BVH_NODES_PAGE_SIZE) {
		*nodeIndex = 0;
		++(*pageIndex);
	}
}
#endif

#if (BVH_NODES_PAGE_COUNT == 8)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const Point* restrict accelVertPage1, __global const Point* restrict accelVertPage2, __global const Point* restrict accelVertPage3, __global const Point* restrict accelVertPage4, __global const Point* restrict accelVertPage5, __global const Point* restrict accelVertPage6, __global const Point* restrict accelVertPage7, __global const BVHAccelArrayNode* restrict accelNodePage0, __global const BVHAccelArrayNode* restrict accelNodePage1, __global const BVHAccelArrayNode* restrict accelNodePage2, __global const BVHAccelArrayNode* restrict accelNodePage3, __global const BVHAccelArrayNode* restrict accelNodePage4, __global const BVHAccelArrayNode* restrict accelNodePage5, __global const BVHAccelArrayNode* restrict accelNodePage6, __global const BVHAccelArrayNode* restrict accelNodePage7
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelVertPage1, accelVertPage2, accelVertPage3, accelVertPage4, accelVertPage5, accelVertPage6, accelVertPage7, accelNodePage0, accelNodePage1, accelNodePage2, accelNodePage3, accelNodePage4, accelNodePage5, accelNodePage6, accelNodePage7
#elif (BVH_NODES_PAGE_COUNT == 7)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const Point* restrict accelVertPage1, __global const Point* restrict accelVertPage2, __global const Point* restrict accelVertPage3, __global const Point* restrict accelVertPage4, __global const Point* restrict accelVertPage5, __global const Point* restrict accelVertPage6, __global const BVHAccelArrayNode* restrict accelNodePage0, __global const BVHAccelArrayNode* restrict accelNodePage1, __global const BVHAccelArrayNode* restrict accelNodePage2, __global const BVHAccelArrayNode* restrict accelNodePage3, __global const BVHAccelArrayNode* restrict accelNodePage4, __global const BVHAccelArrayNode* restrict accelNodePage5, __global const BVHAccelArrayNode* restrict accelNodePage6
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelVertPage1, accelVertPage2, accelVertPage3, accelVertPage4, accelVertPage5, accelVertPage6, accelNodePage0, accelNodePage1, accelNodePage2, accelNodePage3, accelNodePage4, accelNodePage5, accelNodePage6
#elif (BVH_NODES_PAGE_COUNT == 6)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const Point* restrict accelVertPage1, __global const Point* restrict accelVertPage2, __global const Point* restrict accelVertPage3, __global const Point* restrict accelVertPage4, __global const Point* restrict accelVertPage5, __global const BVHAccelArrayNode* restrict accelNodePage0, __global const BVHAccelArrayNode* restrict accelNodePage1, __global const BVHAccelArrayNode* restrict accelNodePage2, __global const BVHAccelArrayNode* restrict accelNodePage3, __global const BVHAccelArrayNode* restrict accelNodePage4, __global const BVHAccelArrayNode* restrict accelNodePage5
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelVertPage1, accelVertPage2, accelVertPage3, accelVertPage4, accelVertPage5, accelNodePage0, accelNodePage1, accelNodePage2, accelNodePage3, accelNodePage4, accelNodePage5
#elif (BVH_NODES_PAGE_COUNT == 5)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const Point* restrict accelVertPage1, __global const Point* restrict accelVertPage2, __global const Point* restrict accelVertPage3, __global const Point* restrict accelVertPage4, __global const BVHAccelArrayNode* restrict accelNodePage0, __global const BVHAccelArrayNode* restrict accelNodePage1, __global const BVHAccelArrayNode* restrict accelNodePage2, __global const BVHAccelArrayNode* restrict accelNodePage3, __global const BVHAccelArrayNode* restrict accelNodePage4
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelVertPage1, accelVertPage2, accelVertPage3, accelVertPage4, accelNodePage0, accelNodePage1, accelNodePage2, accelNodePage3, accelNodePage4
#elif (BVH_NODES_PAGE_COUNT == 4)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const Point* restrict accelVertPage1, __global const Point* restrict accelVertPage2, __global const Point* restrict accelVertPage3, __global const BVHAccelArrayNode* restrict accelNodePage0, __global const BVHAccelArrayNode* restrict accelNodePage1, __global const BVHAccelArrayNode* restrict accelNodePage2, __global const BVHAccelArrayNode* restrict accelNodePage3
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelVertPage1, accelVertPage2, accelVertPage3, accelNodePage0, accelNodePage1, accelNodePage2, accelNodePage3
#elif (BVH_NODES_PAGE_COUNT == 3)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const Point* restrict accelVertPage1, __global const Point* restrict accelVertPage2, __global const BVHAccelArrayNode* restrict accelNodePage0, __global const BVHAccelArrayNode* restrict accelNodePage1, __global const BVHAccelArrayNode* restrict accelNodePage2
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelVertPage1, accelVertPage2, accelNodePage0, accelNodePage1, accelNodePage2
#elif (BVH_NODES_PAGE_COUNT == 2)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const Point* restrict accelVertPage1, __global const BVHAccelArrayNode* restrict accelNodePage0, __global const BVHAccelArrayNode* restrict accelNodePage1
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelVertPage1, accelNodePage0, accelNodePage1
#elif (BVH_NODES_PAGE_COUNT == 1)
#define ACCELERATOR_INTERSECT_PARAM_DECL , __global const Point* restrict accelVertPage0, __global const BVHAccelArrayNode* restrict accelNodePage0
#define ACCELERATOR_INTERSECT_PARAM , accelVertPage0, accelNodePage0
#elif
ERROR: unsuported BVH_NODES_PAGE_COUNT !!!
#endif

void Accelerator_Intersect(
		const Ray *ray,
		RayHit *rayHit
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	// Initialize vertex page references
#if (BVH_VERTS_PAGE_COUNT > 1)
	__global const Point* restrict accelVertPages[BVH_VERTS_PAGE_COUNT];
#if defined(BVH_VERTS_PAGE0)
	accelVertPages[0] = accelVertPage0;
#endif
#if defined(BVH_VERTS_PAGE1)
	accelVertPages[1] = accelVertPage1;
#endif
#if defined(BVH_VERTS_PAGE2)
	accelVertPages[2] = accelVertPage2;
#endif
#if defined(BVH_VERTS_PAGE3)
	accelVertPages[3] = accelVertPage3;
#endif
#if defined(BVH_VERTS_PAGE4)
	accelVertPages[4] = accelVertPage4;
#endif
#if defined(BVH_VERTS_PAGE5)
	accelVertPages[5] = accelVertPage5;
#endif
#if defined(BVH_VERTS_PAGE6)
	accelVertPages[6] = accelVertPage6;
#endif
#if defined(BVH_VERTS_PAGE7)
	accelVertPages[7] = accelVertPage7;
#endif
#endif

	// Initialize node page references
#if (BVH_NODES_PAGE_COUNT > 1)
	__global const BVHAccelArrayNode* restrict accelNodePages[BVH_NODES_PAGE_COUNT];
#if defined(BVH_NODES_PAGE0)
	accelNodePages[0] = accelNodePage0;
#endif
#if defined(BVH_NODES_PAGE1)
	accelNodePages[1] = accelNodePage1;
#endif
#if defined(BVH_NODES_PAGE2)
	accelNodePages[2] = accelNodePage2;
#endif
#if defined(BVH_NODES_PAGE3)
	accelNodePages[3] = accelNodePage3;
#endif
#if defined(BVH_NODES_PAGE4)
	accelNodePages[4] = accelNodePage4;
#endif
#if defined(BVH_NODES_PAGE5)
	accelNodePages[5] = accelNodePage5;
#endif
#if defined(BVH_NODES_PAGE6)
	accelNodePages[6] = accelNodePage6;
#endif
#if defined(BVH_NODES_PAGE7)
	accelNodePages[7] = accelNodePage7;
#endif

	const uint rootNodeData = accelNodePage0[0].nodeData;
	const uint stopPage = BVHNodeData_GetPageIndex(rootNodeData);
	const uint stopNode = BVHNodeData_GetNodeIndex(rootNodeData); // Non-existent
	uint currentPage = 0; // Root Node Page
#else
	const uint stopNode = BVHNodeData_GetSkipIndex(accelNodePage0[0].nodeData); // Non-existent
#endif

	const float3 rayOrig = (float3)(ray->o.x, ray->o.y, ray->o.z);
	const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);
	const float mint = ray->mint;
	float maxt = ray->maxt;

	const float3 invRayDir = 1.f / rayDir;

	uint hitMeshIndex = NULL_INDEX;
	uint hitTriangleIndex = NULL_INDEX;
	uint currentNode = 0; // Root Node

	float b1, b2;
#if (BVH_NODES_PAGE_COUNT == 1)
	while (currentNode < stopNode) {
		__global const BVHAccelArrayNode* restrict node = &accelNodePage0[currentNode];
#else
	while ((currentPage < stopPage) || (currentNode < stopNode)) {
		__global const BVHAccelArrayNode *accelNodePage = accelNodePages[currentPage];
		__global const BVHAccelArrayNode *node = &accelNodePage[currentNode];
#endif
		// Read the node
		__global float4 *data = (__global float4 *)node;
		const float4 data0 = *data++;
		const float4 data1 = *data;

		//const uint nodeData = node->nodeData;
		const uint nodeData = as_uint(data1.s2);
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the triangle

			//const uint i0 = node->triangleLeaf.v[0];
			//const uint i1 = node->triangleLeaf.v[1];
			//const uint i2 = node->triangleLeaf.v[2];
			const uint v0 = as_uint(data0.s0);
			const uint v1 = as_uint(data0.s1);
			const uint v2 = as_uint(data0.s2);

#if (BVH_VERTS_PAGE_COUNT == 1)
			// Fast path for when there is only one memory page
			const float3 p0 = VLOAD3F(&accelVertPage0[v0].x);
			const float3 p1 = VLOAD3F(&accelVertPage0[v1].x);
			const float3 p2 = VLOAD3F(&accelVertPage0[v2].x);
#else
			const uint pv0 = (v0 & 0xe0000000u) >> 29;
			const uint iv0 = (v0 & 0x1fffffffu);
			__global Point *vp0 = accelVertPages[pv0];
			const float3 p0 = VLOAD3F(&vp0[iv0].x);

			const uint pv1 = (v1 & 0xe0000000u) >> 29;
			const uint iv1 = (v1 & 0x1fffffffu);
			__global Point *vp1 = accelVertPages[pv1];
			const float3 p1 = VLOAD3F(&vp1[iv1].x);

			const uint pv2 = (v2 & 0xe0000000u) >> 29;
			const uint iv2 = (v2 & 0x1fffffffu);
			__global Point *vp2 = accelVertPages[pv2];
			const float3 p2 = VLOAD3F(&vp2[iv2].x);
#endif

			//const uint triangleIndex = node->triangleLeaf.triangleIndex;
			const uint meshIndex = as_uint(data0.s3);
			const uint triangleIndex = as_uint(data1.s0);

			Triangle_Intersect(rayOrig, rayDir, mint, &maxt, &hitMeshIndex, &hitTriangleIndex,
					&b1, &b2, meshIndex, triangleIndex, p0, p1, p2);
#if (BVH_NODES_PAGE_COUNT == 1)
			++currentNode;
#else
			NextNode(&currentPage, &currentNode);
#endif
		} else {
			// It is a node, check the bounding box
			//const float3 pMin = VLOAD3F(&node->bvhNode.bboxMin[0]);
			//const float3 pMax = VLOAD3F(&node->bvhNode.bboxMax[0]);
			const float3 pMin = (float3)(data0.s0, data0.s1, data0.s2);
			const float3 pMax = (float3)(data0.s3, data1.s0, data1.s1);

			if (BBox_IntersectP(pMin, pMax, rayOrig, invRayDir, mint, maxt)) {
#if (BVH_NODES_PAGE_COUNT == 1)
				++currentNode;
#else
				NextNode(&currentPage, &currentNode);
#endif
			} else {
#if (BVH_NODES_PAGE_COUNT == 1)
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the flag (i.e. the last bit) is 0
				currentNode = nodeData;
#else
				currentPage = BVHNodeData_GetPageIndex(nodeData);
				currentNode = BVHNodeData_GetNodeIndex(nodeData);
#endif
			}
		}
	}

	rayHit->t = maxt;
	rayHit->b1 = b1;
	rayHit->b2 = b2;
	rayHit->meshIndex = hitMeshIndex;
	rayHit->triangleIndex = hitTriangleIndex;
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Accelerator_Intersect_RayBuffer(
		__global const Ray* restrict rays,
		__global RayHit *rayHits,
		const uint rayCount
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	// Select the ray to check
	const int gid = get_global_id(0);
	if (gid >= rayCount)
		return;

	Ray ray;
	Ray_ReadAligned4_Private(&rays[gid], &ray);

	RayHit rayHit;
	Accelerator_Intersect(
		&ray,
		&rayHit
		ACCELERATOR_INTERSECT_PARAM
		);

	// Write result
	__global RayHit *memRayHit = &rayHits[gid];
	memRayHit->t = rayHit.t;
	memRayHit->b1 = rayHit.b1;
	memRayHit->b2 = rayHit.b2;
	memRayHit->meshIndex = rayHit.meshIndex;
	memRayHit->triangleIndex = rayHit.triangleIndex;
}
