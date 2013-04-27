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
			uint index;
		} bvhLeaf;
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

#if (MBVH_NODES_PAGE_COUNT > 1)
void NextNode(uint *pageIndex, uint *nodeIndex) {
	++(*nodeIndex);
	if (*nodeIndex >= MBVH_NODES_PAGE_SIZE) {
		*nodeIndex = 0;
		++(*pageIndex);
	}
}
#endif

__kernel void Intersect(
		__global Ray *rays,
		__global RayHit *rayHits,
		const uint rayCount
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

	// TODO

	// Write result
	__global RayHit *rayHit = &rayHits[gid];
	rayHit->t = maxt;
	rayHit->b1 = b1;
	rayHit->b2 = b2;
	rayHit->index = hitIndex;
}
