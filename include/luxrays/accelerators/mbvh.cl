#line 2 "mbvh_kernel.cl"

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
	int pad0; // To align to float4
} BVHAccelArrayNode;

#define BVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define BVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)
#if (MBVH_NODES_PAGE_COUNT > 1)
#define BVHNodeData_GetPageIndex(nodeData) (((nodeData) & 0x70000000u) >> 28)
#define BVHNodeData_GetNodeIndex(nodeData) ((nodeData) & 0x0fffffffu)
#endif

#if (MBVH_NODES_PAGE_COUNT > 1)
void NextNode(uint *pageIndex, uint *nodeIndex) {
	++(*nodeIndex);
	if (*nodeIndex >= MBVH_NODES_PAGE_SIZE) {
		*nodeIndex = 0;
		++(*pageIndex);
	}
}
#endif

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Intersect(
		__global Ray *rays,
		__global RayHit *rayHits,
		const uint rayCount
#if defined(MBVH_HAS_TRANSFORMATIONS)
		, __global Matrix4x4 *leafTransformations
#endif
#if defined(MBVH_VERTS_PAGE0)
		, __global Point *vertPage0
#endif
#if defined(MBVH_VERTS_PAGE1)
		, __global Point *vertPage1
#endif
#if defined(MBVH_VERTS_PAGE2)
		, __global Point *vertPage2
#endif
#if defined(MBVH_VERTS_PAGE3)
		, __global Point *vertPage3
#endif
#if defined(MBVH_VERTS_PAGE4)
		, __global Point *vertPage4
#endif
#if defined(MBVH_VERTS_PAGE5)
		, __global Point *vertPage5
#endif
#if defined(MBVH_VERTS_PAGE6)
		, __global Point *vertPage6
#endif
#if defined(MBVH_VERTS_PAGE7)
		, __global Point *vertPage7
#endif
#if defined(MBVH_NODES_PAGE0)
		, __global BVHAccelArrayNode *nodePage0
#endif
#if defined(MBVH_NODES_PAGE1)
		, __global BVHAccelArrayNode *nodePage1
#endif
#if defined(MBVH_NODES_PAGE2)
		, __global BVHAccelArrayNode *nodePage2
#endif
#if defined(MBVH_NODES_PAGE3)
		, __global BVHAccelArrayNode *nodePage3
#endif
#if defined(MBVH_NODES_PAGE4)
		, __global BVHAccelArrayNode *nodePage4
#endif
#if defined(MBVH_NODES_PAGE5)
		, __global BVHAccelArrayNode *nodePage5
#endif
#if defined(MBVH_NODES_PAGE6)
		, __global BVHAccelArrayNode *nodePage6
#endif
#if defined(MBVH_NODES_PAGE7)
		, __global BVHAccelArrayNode *nodePage7
#endif
		) {
	// Select the ray to check
	const int gid = get_global_id(0);
	if (gid >= rayCount)
		return;

	// Initialize vertex page references
#if (MBVH_VERTS_PAGE_COUNT > 1)
	__global Point *vertPages[MBVH_VERTS_PAGE_COUNT];
#if defined(MBVH_VERTS_PAGE0)
	vertPages[0] = vertPage0;
#endif
#if defined(MBVH_VERTS_PAGE1)
	vertPages[1] = vertPage1;
#endif
#if defined(MBVH_VERTS_PAGE2)
	vertPages[2] = vertPage2;
#endif
#if defined(MBVH_VERTS_PAGE3)
	vertPages[3] = vertPage3;
#endif
#if defined(MBVH_VERTS_PAGE4)
	vertPages[4] = vertPage4;
#endif
#if defined(MBVH_VERTS_PAGE5)
	vertPages[5] = vertPage5;
#endif
#if defined(MBVH_VERTS_PAGE6)
	vertPages[6] = vertPage6;
#endif
#if defined(MBVH_VERTS_PAGE7)
	vertPages[7] = vertPage7;
#endif
#endif

	// Initialize node page references
#if (MBVH_NODES_PAGE_COUNT > 1)
	__global BVHAccelArrayNode *nodePages[MBVH_NODES_PAGE_COUNT];
#if defined(MBVH_NODES_PAGE0)
	nodePages[0] = nodePage0;
#endif
#if defined(MBVH_NODES_PAGE1)
	nodePages[1] = nodePage1;
#endif
#if defined(MBVH_NODES_PAGE2)
	nodePages[2] = nodePage2;
#endif
#if defined(MBVH_NODES_PAGE3)
	nodePages[3] = nodePage3;
#endif
#if defined(MBVH_NODES_PAGE4)
	nodePages[4] = nodePage4;
#endif
#if defined(MBVH_NODES_PAGE5)
	nodePages[5] = nodePage5;
#endif
#if defined(MBVH_NODES_PAGE6)
	nodePages[6] = nodePage6;
#endif
#if defined(MBVH_NODES_PAGE7)
	nodePages[7] = nodePage7;
#endif
#endif

	bool insideLeafTree = false;
#if (MBVH_NODES_PAGE_COUNT == 1)
	uint currentRootNode = 0;
	uint rootStopNode = BVHNodeData_GetSkipIndex(nodePage0[0].nodeData); // Non-existent
	uint currentStopNode = rootStopNode; // Non-existent
	uint currentNode = currentRootNode;
#else
	uint currentRootPage = 0;
	uint currentRootNode = 0;

	const uint rootStopIndex = nodePage0[0].nodeData;
	const uint rootStopPage = BVHNodeData_GetPageIndex(rootStopIndex);
	const uint rootStopNode = BVHNodeData_GetNodeIndex(rootStopIndex); // Non-existent

	uint currentStopPage = rootStopPage; // Non-existent
	uint currentStopNode = rootStopNode; // Non-existent

	uint currentPage = 0; // Root Node Page
	uint currentNode = currentRootNode;
#endif
	
	uint currentTriangleOffset = 0;

	__global Ray *ray = &rays[gid];
	const float3 rootRayOrig = VLOAD3F(&ray->o.x);
	float3 currentRayOrig = rootRayOrig;
	const float3 rootRayDir = VLOAD3F(&ray->d.x);
	float3 currentRayDir = rootRayDir;
	const float mint = ray->mint;
	float maxt = ray->maxt;

	uint hitIndex = NULL_INDEX;

	float b1, b2;
	for (;;) {
#if (MBVH_NODES_PAGE_COUNT == 1)
		if (currentNode >= currentStopNode) {
#else
		if (!((currentPage < currentStopPage) || (currentNode < currentStopNode))) {
#endif
			if (insideLeafTree) {
				// Go back to the root tree
#if (MBVH_NODES_PAGE_COUNT == 1)
				currentNode = currentRootNode;
				currentStopNode = rootStopNode;
#else
				currentPage = currentRootPage;
				currentNode = currentRootNode;
				currentStopPage = rootStopPage;
				currentStopNode = rootStopNode;
#endif
				currentRayOrig = rootRayOrig;
				currentRayDir = rootRayDir;
				insideLeafTree = false;

				// Check if the leaf was the very last root node
#if (MBVH_NODES_PAGE_COUNT == 1)
				if (currentNode >= currentStopNode)
#else
				if (!((currentPage < currentStopPage) || (currentNode < currentStopNode)))
#endif
					break;
			} else {
				// Done
				break;
			}
		}

#if (MBVH_NODES_PAGE_COUNT == 1)
		__global BVHAccelArrayNode *node = &nodePage0[currentNode];
#else
		__global BVHAccelArrayNode *nodePage = nodePages[currentPage];
		__global BVHAccelArrayNode *node = &nodePage[currentNode];
#endif
		const uint nodeData = node->nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			if (insideLeafTree) {
				// I'm inside a leaf tree, I have to check the triangle
#if (MBVH_VERTS_PAGE_COUNT == 1)
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

				Triangle_Intersect(currentRayOrig, currentRayDir, mint, &maxt, &hitIndex, &b1, &b2,
					node->triangleLeaf.triangleIndex + currentTriangleOffset, p0, p1, p2);
#if (MBVH_NODES_PAGE_COUNT == 1)
				++currentNode;
#else
				NextNode(&currentPage, &currentNode);
#endif
			} else {
				// I have to check a leaf tree
#if defined(MBVH_HAS_TRANSFORMATIONS)
				// Transform the ray in the local coordinate system
				if (node->bvhLeaf.transformIndex != NULL_INDEX) {
					// Transform ray origin
					__global Matrix4x4 *m = &leafTransformations[node->bvhLeaf.transformIndex];
					currentRayOrig = Matrix4x4_ApplyPoint(m, rootRayOrig);
					currentRayDir = Matrix4x4_ApplyVector(m, rootRayDir);
				}
#endif 
				currentTriangleOffset = node->bvhLeaf.triangleOffsetIndex;

				const uint leafIndex = node->bvhLeaf.leafIndex;
#if (MBVH_NODES_PAGE_COUNT == 1)
				currentRootNode = currentNode + 1;
				currentNode = leafIndex;
				currentStopNode = BVHNodeData_GetSkipIndex(nodePage0[currentNode].nodeData);
#else
				currentRootPage = currentPage;
				currentRootNode = currentNode;
				NextNode(&currentRootPage, &currentRootNode);

				currentPage = BVHNodeData_GetPageIndex(leafIndex);
				currentNode = BVHNodeData_GetNodeIndex(leafIndex);

				__global BVHAccelArrayNode *stopNodePage = nodePages[currentPage];
				__global BVHAccelArrayNode *stopNode = &stopNodePage[currentNode];
				const uint stopIndex = stopNode->nodeData;
				currentStopPage = BVHNodeData_GetPageIndex(stopIndex);
				currentStopNode = BVHNodeData_GetNodeIndex(stopIndex);
#endif

				// Now, I'm inside a leaf tree
				insideLeafTree = true;
			}
		} else {
			// It is a node, check the bounding box
			const float3 pMin = VLOAD3F(&node->bvhNode.bboxMin[0]);
			const float3 pMax = VLOAD3F(&node->bvhNode.bboxMax[0]);

			if (BBox_IntersectP(pMin, pMax, currentRayOrig, 1.f / currentRayDir, mint, maxt)) {
#if (MBVH_NODES_PAGE_COUNT == 1)
				++currentNode;
#else
				NextNode(&currentPage, &currentNode);
#endif
			} else {
#if (MBVH_NODES_PAGE_COUNT == 1)
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
	RayHit_WriteAligned4(&rayHits[gid], maxt, b1, b2, hitIndex);
}
