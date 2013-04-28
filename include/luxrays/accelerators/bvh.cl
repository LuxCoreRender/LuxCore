#line 2 "bvh_kernel.cl"

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

typedef struct {
	union {
		struct {
			// I can not use BBox here because objects with a constructor are not
			// allowed inside an union.
			float bboxMin[3];
			float bboxMax[3];
		} bvhNode;
		struct {
			uint v[3];
			uint triangleIndex;
		} triangleLeaf;
		struct {
			uint leafIndex;
			uint transformIndex;
			uint triangleOffsetIndex;
		} bvhLeaf; // Used by MBVH
	};
	// Most significant bit is used to mark leafs
	uint nodeData;
} BVHAccelArrayNode;

#define BVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define BVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)
#if (BVH_NODES_PAGE_COUNT > 1)
#define BVHNodeData_GetPageIndex(nodeData) (((nodeData) & 0x70000000u) >> 28)
#define BVHNodeData_GetNodeIndex(nodeData) ((nodeData) & 0x0fffffffu)
#endif

void Triangle_Intersect(
		const float3 rayOrig,
		const float3 rayDir,
		const float mint,
		float *maxt,
		uint *hitIndex,
		float *hitB1,
		float *hitB2,
		const uint currentIndex,
		const float3 v0,
		const float3 v1,
		const float3 v2) {

	// Calculate intersection
	const float3 e1 = v1 - v0;
	const float3 e2 = v2 - v0;
	const float3 s1 = cross(rayDir, e2);

	const float divisor = dot(s1, e1);
	if (divisor == 0.f)
		return;

	const float invDivisor = 1.f / divisor;

	// Compute first barycentric coordinate
	const float3 d = rayOrig - v0;
	const float b1 = dot(d, s1) * invDivisor;
	if (b1 < 0.f)
		return;

	// Compute second barycentric coordinate
	const float3 s2 = cross(d, e1);
	const float b2 = dot(rayDir, s2) * invDivisor;
	if (b2 < 0.f)
		return;

	const float b0 = 1.f - b1 - b2;
	if (b0 < 0.f)
		return;

	// Compute _t_ to intersection point
	const float t = dot(e2, s2) * invDivisor;
	if (t < mint || t > *maxt)
		return;

	*maxt = t;
	*hitB1 = b1;
	*hitB2 = b2;
	*hitIndex = currentIndex;
}

int BBox_IntersectP(
		const float3 rayOrig, const float3 invRayDir,
		const float mint, const float maxt,
		const float3 pMin, const float3 pMax) {
	const float3 l1 = (pMin - rayOrig) * invRayDir;
	const float3 l2 = (pMax - rayOrig) * invRayDir;
	const float3 tNear = fmin(l1, l2);
	const float3 tFar = fmax(l1, l2);

	float t0 = fmax(fmax(fmax(tNear.x, tNear.y), fmax(tNear.x, tNear.z)), mint);
    float t1 = fmin(fmin(fmin(tFar.x, tFar.y), fmin(tFar.x, tFar.z)), maxt);

	return (t1 > t0);
}

#if (BVH_NODES_PAGE_COUNT > 1)
void NextNode(uint *pageIndex, uint *nodeIndex) {
	++(*nodeIndex);
	if (*nodeIndex >= BVH_NODES_PAGE_SIZE) {
		*nodeIndex = 0;
		++(*pageIndex);
	}
}
#endif

__kernel void Intersect(
		__global Ray *rays,
		__global RayHit *rayHits,
		const uint rayCount
#if defined(BVH_VERTS_PAGE0)
		, __global Point *vertPage0
#endif
#if defined(BVH_VERTS_PAGE1)
		, __global Point *vertPage1
#endif
#if defined(BVH_VERTS_PAGE2)
		, __global Point *vertPage2
#endif
#if defined(BVH_VERTS_PAGE3)
		, __global Point *vertPage3
#endif
#if defined(BVH_VERTS_PAGE4)
		, __global Point *vertPage4
#endif
#if defined(BVH_VERTS_PAGE5)
		, __global Point *vertPage5
#endif
#if defined(BVH_VERTS_PAGE6)
		, __global Point *vertPage6
#endif
#if defined(BVH_VERTS_PAGE7)
		, __global Point *vertPage7
#endif
#if defined(BVH_NODES_PAGE0)
		, __global BVHAccelArrayNode *nodePage0
#endif
#if defined(BVH_NODES_PAGE1)
		, __global BVHAccelArrayNode *nodePage1
#endif
#if defined(BVH_NODES_PAGE2)
		, __global BVHAccelArrayNode *nodePage2
#endif
#if defined(BVH_NODES_PAGE3)
		, __global BVHAccelArrayNode *nodePage3
#endif
#if defined(BVH_NODES_PAGE4)
		, __global BVHAccelArrayNode *nodePage4
#endif
#if defined(BVH_NODES_PAGE5)
		, __global BVHAccelArrayNode *nodePage5
#endif
#if defined(BVH_NODES_PAGE6)
		, __global BVHAccelArrayNode *nodePage6
#endif
#if defined(BVH_NODES_PAGE7)
		, __global BVHAccelArrayNode *nodePage7
#endif
		) {
	// Select the ray to check
	const int gid = get_global_id(0);
	if (gid >= rayCount)
		return;

	// Initialize vertex page references
#if (BVH_VERTS_PAGE_COUNT > 1)
	__global Point *vertPages[BVH_VERTS_PAGE_COUNT];
#if defined(BVH_VERTS_PAGE0)
	vertPages[0] = vertPage0;
#endif
#if defined(BVH_VERTS_PAGE1)
	vertPages[1] = vertPage1;
#endif
#if defined(BVH_VERTS_PAGE2)
	vertPages[2] = vertPage2;
#endif
#if defined(BVH_VERTS_PAGE3)
	vertPages[3] = vertPage3;
#endif
#if defined(BVH_VERTS_PAGE4)
	vertPages[4] = vertPage4;
#endif
#if defined(BVH_VERTS_PAGE5)
	vertPages[5] = vertPage5;
#endif
#if defined(BVH_VERTS_PAGE6)
	vertPages[6] = vertPage6;
#endif
#if defined(BVH_VERTS_PAGE7)
	vertPages[7] = vertPage7;
#endif
#endif

	// Initialize node page references
#if (BVH_NODES_PAGE_COUNT > 1)
	__global BVHAccelArrayNode *nodePages[BVH_NODES_PAGE_COUNT];
#if defined(BVH_NODES_PAGE0)
	nodePages[0] = nodePage0;
#endif
#if defined(BVH_NODES_PAGE1)
	nodePages[1] = nodePage1;
#endif
#if defined(BVH_NODES_PAGE2)
	nodePages[2] = nodePage2;
#endif
#if defined(BVH_NODES_PAGE3)
	nodePages[3] = nodePage3;
#endif
#if defined(BVH_NODES_PAGE4)
	nodePages[4] = nodePage4;
#endif
#if defined(BVH_NODES_PAGE5)
	nodePages[5] = nodePage5;
#endif
#if defined(BVH_NODES_PAGE6)
	nodePages[6] = nodePage6;
#endif
#if defined(BVH_NODES_PAGE7)
	nodePages[7] = nodePage7;
#endif

	const uint stopPage = BVHNodeData_GetPageIndex(nodePage0[0].nodeData);
	const uint stopNode = BVHNodeData_GetNodeIndex(nodePage0[0].nodeData); // Non-existent
	uint currentPage = 0; // Root Node Page
#else
	const uint stopNode = BVHNodeData_GetSkipIndex(nodePage0[0].nodeData); // Non-existent
#endif

	__global Ray *ray = &rays[gid];
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
	const float mint = ray->mint;
	float maxt = ray->maxt;

	const float3 invRayDir = 1.f / rayDir;

	uint hitIndex = NULL_INDEX;
	uint currentNode = 0; // Root Node

	float b1, b2;
#if (BVH_NODES_PAGE_COUNT == 1)
	while (currentNode < stopNode) {
		__global BVHAccelArrayNode *node = &nodePage0[currentNode];
#else
	while ((currentPage < stopPage) || (currentNode < stopNode)) {
		__global BVHAccelArrayNode *nodePage = nodePages[currentPage];
		__global BVHAccelArrayNode *node = &nodePage[currentNode];
#endif

		const uint nodeData = node->nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the triangle

#if (BVH_VERTS_PAGE_COUNT == 1)
			// Fast path for when there is only one memory page
			const float3 p0 = VLOAD3F(&vertPage0[node->triangleLeaf.v[0]].x);
			const float3 p1 = VLOAD3F(&vertPage0[node->triangleLeaf.v[1]].x);
			const float3 p2 = VLOAD3F(&vertPage0[node->triangleLeaf.v[2]].x);
#else
			const uint v0 = node->triangleLeaf.v[0];
			const uint pv0 = (v0 & 0xe0000000u) >> 29;
			const uint iv0 = (v0 & 0x1fffffffu);
			__global Point *vp0 = vertPages[pv0];
			const float3 p0 = VLOAD3F(&vp0[iv0].x);

			const uint v1 = node->triangleLeaf.v[1];
			const uint pv1 = (v1 & 0xe0000000u) >> 29;
			const uint iv1 = (v1 & 0x1fffffffu);
			__global Point *vp1 = vertPages[pv1];
			const float3 p1 = VLOAD3F(&vp1[iv1].x);

			const uint v2 = node->triangleLeaf.v[2];
			const uint pv2 = (v2 & 0xe0000000u) >> 29;
			const uint iv2 = (v2 & 0x1fffffffu);
			__global Point *vp2 = vertPages[pv2];
			const float3 p2 = VLOAD3F(&vp2[iv2].x);
#endif

			Triangle_Intersect(rayOrig, rayDir, mint, &maxt, &hitIndex, &b1, &b2,
					node->triangleLeaf.triangleIndex, p0, p1, p2);
#if (BVH_NODES_PAGE_COUNT == 1)
			++currentNode;
#else
			NextNode(&currentPage, &currentNode);
#endif
		} else {
			// It is a node, check the bounding box
			const float3 pMin = VLOAD3F(&node->bvhNode.bboxMin[0]);
			const float3 pMax = VLOAD3F(&node->bvhNode.bboxMax[0]);

			if (BBox_IntersectP(rayOrig, invRayDir, mint, maxt, pMin, pMax)) {
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

	// Write result
	__global RayHit *rayHit = &rayHits[gid];
	rayHit->t = maxt;
	rayHit->b1 = b1;
	rayHit->b2 = b2;
	rayHit->index = hitIndex;
}
