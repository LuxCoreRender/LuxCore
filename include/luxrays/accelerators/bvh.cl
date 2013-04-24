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
			uint index;
		} bvhLeaf;
	};
	// Most significant bit is used to mark leafs
	uint nodeData;
} BVHAccelArrayNode;

#define BVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define BVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)

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

__kernel void Intersect(
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Point *verts,
		__global BVHAccelArrayNode *bvhTree,
		const uint rayCount) {
	// Select the ray to check
	const int gid = get_global_id(0);
	if (gid >= rayCount)
		return;

	__global Ray *ray = &rays[gid];
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
	const float mint = ray->mint;
	float maxt = ray->maxt;

	const float3 invRayDir = 1.f / rayDir;

	uint hitIndex = NULL_INDEX;
	uint currentNode = 0; // Root Node
	const uint stopNode = BVHNodeData_GetSkipIndex(bvhTree[0].nodeData); // Non-existent

	float b1, b2;
	while (currentNode < stopNode) {
		__global BVHAccelArrayNode *node = &bvhTree[currentNode];

		const uint nodeData = node->nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the triangle
			const float3 p0 = VLOAD3F(&verts[node->triangleLeaf.v[0]].x);
			const float3 p1 = VLOAD3F(&verts[node->triangleLeaf.v[1]].x);
			const float3 p2 = VLOAD3F(&verts[node->triangleLeaf.v[2]].x);

			Triangle_Intersect(rayOrig, rayDir, mint, &maxt, &hitIndex, &b1, &b2,
					node->triangleLeaf.triangleIndex, p0, p1, p2);
			++currentNode;
		} else {
			// It is a node, check the bounding box
			const float3 pMin = VLOAD3F(&node->bvhNode.bboxMin[0]);
			const float3 pMax = VLOAD3F(&node->bvhNode.bboxMax[0]);

			if (BBox_IntersectP(rayOrig, invRayDir, mint, maxt, pMin, pMax))
				++currentNode;
			else {
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the flag is 0
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
