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
} BVHAccelArrayNode;

#define BVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define BVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)
#if (MBVH_NODES_PAGE_COUNT > 1)
#define BVHNodeData_GetPageIndex(nodeData) (((nodeData) & 0x70000000u) >> 28)
#define BVHNodeData_GetNodeIndex(nodeData) ((nodeData) & 0x0fffffffu)
#endif

void TransformP(Point *ptrans, const Point *p, __global Matrix4x4 *m) {
    const float x = p->x;
    const float y = p->y;
    const float z = p->z;

	ptrans->x = m->m[0][0] * x + m->m[0][1] * y + m->m[0][2] * z + m->m[0][3];
	ptrans->y = m->m[1][0] * x + m->m[1][1] * y + m->m[1][2] * z + m->m[1][3];
	ptrans->z = m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z + m->m[2][3];
	const float w = m->m[3][0] * x + m->m[3][1] * y + m->m[3][2] * z + m->m[3][3];

    ptrans->x /= w;
    ptrans->y /= w;
    ptrans->z /= w;
}

void TransformV(Vector *ptrans, const Vector *p, __global Matrix4x4 *m) {
    const float x = p->x;
    const float y = p->y;
    const float z = p->z;

	ptrans->x = m->m[0][0] * x + m->m[0][1] * y + m->m[0][2] * z;
	ptrans->y = m->m[1][0] * x + m->m[1][1] * y + m->m[1][2] * z;
	ptrans->z = m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z;
}

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
		const float3 rayOrig, const float3 rayDir,
		const float mint, const float maxt,
		const float3 pMin, const float3 pMax) {
	const float3 l1 = (pMin - rayOrig) / rayDir;
	const float3 l2 = (pMax - rayOrig) / rayDir;
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
	uint currentRootNode = 0;
	uint rootStopNode = BVHNodeData_GetSkipIndex(nodePage0[0].nodeData); // Non-existent
	uint currentNode = currentRootNode;
	uint currentStopNode = rootStopNode; // Non-existent
	uint currentTriangleOffset = 0;
	__global BVHAccelArrayNode *currentTree = nodePage0;

	__global Ray *ray = &rays[gid];
	const float3 rootRayOrig = VLOAD3F(&ray->o.x);
	float3 currentRayOrig = rootRayOrig;
	const float3 rootRayDir = VLOAD3F(&ray->d.x);
	float3 currentRayDir = rootRayDir;
	const float mint = ray->mint;
	float maxt = ray->maxt;

	uint hitIndex = NULL_INDEX;

	float t, b1, b2;
	for (;;) {
		if (currentNode >= currentStopNode) {
			if (insideLeafTree) {
				// Go back to the root tree
				currentTree = nodePage0;
				currentNode = currentRootNode;
				currentStopNode = rootStopNode;
				currentRayOrig = rootRayOrig;
				currentRayDir = rootRayDir;
				insideLeafTree = false;

				// Check if the leaf was the very last root node
				if (currentNode >= currentStopNode)
					break;
			} else {
				// Done
				break;
			}
		}

		__global BVHAccelArrayNode *node = &currentTree[currentNode];

		const uint nodeData = node->nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			if (insideLeafTree) {
				// I'm inside a leaf tree, I have to check the triangle
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

				Triangle_Intersect(currentRayOrig, currentRayDir, mint, &maxt, &hitIndex, &b1, &b2,
					node->triangleLeaf.triangleIndex + currentTriangleOffset, p0, p1, p2);
				++currentNode;
			} else {
				// I have to check a leaf tree
				currentTree = &nodePage0[node->bvhLeaf.leafIndex];

#if defined(MBVH_HAS_TRANSFORMATIONS)
				// Transform the ray in the local coordinate system
				if (node->bvhLeaf.transformIndex != NULL_INDEX) {
					// Transform ray origin
					__global Matrix4x4 *m = &leafTransformations[node->bvhLeaf.transformIndex];
					TransformP(&currentRayOrig, &rootRayOrig, m);
					TransformV(&currentRayDir, &rootRayDir, m);
				}
#endif 
				currentTriangleOffset = node->bvhLeaf.triangleOffsetIndex;

				currentRootNode = currentNode + 1;
				currentNode = 0;
				currentStopNode = BVHNodeData_GetSkipIndex(currentTree[0].nodeData);

				// Now, I'm inside a leaf tree
				insideLeafTree = true;
			}
		} else {
			// It is a node, check the bounding box
			const float3 pMin = VLOAD3F(&node->bvhNode.bboxMin[0]);
			const float3 pMax = VLOAD3F(&node->bvhNode.bboxMax[0]);

			if (BBox_IntersectP(currentRayOrig, currentRayDir, mint, maxt, pMin, pMax))
				++currentNode;
			else {
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
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
