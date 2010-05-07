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

typedef struct {
	float x, y, z;
} Point;

typedef struct {
	float x, y, z;
} Vector;

typedef struct {
	Point o;
	Vector d;
	float mint, maxt;
} Ray;

typedef struct {
	float t;
	float b1, b2; // Barycentric coordinates of the hit point
	unsigned int index;
} RayHit;

typedef struct {
	Point pMin, pMax;
} BBox;

typedef struct QuadRay {
	float4 ox, oy, oz;
	float4 dx, dy, dz;
	float4 mint, maxt;
} QuadRay;

typedef struct {
	float4 origx, origy, origz;
	float4 edge1x, edge1y, edge1z;
	float4 edge2x, edge2y, edge2z;
	unsigned int primitives[4];
} QuadTiangle;

typedef struct {
	float4 bboxes[2][3];
	int4 children;
} QBVHNode;

#define emptyLeafNode 0xffffffff

#define QBVHNode_IsLeaf(index) (index < 0)
#define QBVHNode_IsEmpty(index) (index == emptyLeafNode)
#define QBVHNode_NbQuadPrimitives(index) ((unsigned int)(((index >> 27) & 0xf) + 1))
#define QBVHNode_FirstQuadIndex(index) (index & 0x07ffffff)

// Using invDir0/invDir1/invDir2 and sign0/sign1/sign2 instead of an
// array because I dont' trust OpenCL compiler =)
static int4 QBVHNode_BBoxIntersect(__global QBVHNode *node, const QuadRay *ray4,
		const float4 invDir0, const float4 invDir1, const float4 invDir2,
		const int signs0, const int signs1, const int signs2) {
	float4 tMin = ray4->mint;
	float4 tMax = ray4->maxt;

	// X coordinate
	tMin = max(tMin, (node->bboxes[signs0][0] - ray4->ox) * invDir0);
	tMax = min(tMax, (node->bboxes[1 - signs0][0] - ray4->ox) * invDir0);

	// Y coordinate
	tMin = max(tMin, (node->bboxes[signs1][1] - ray4->oy) * invDir1);
	tMax = min(tMax, (node->bboxes[1 - signs1][1] - ray4->oy) * invDir1);

	// Z coordinate
	tMin = max(tMin, (node->bboxes[signs2][2] - ray4->oz) * invDir2);
	tMax = min(tMax, (node->bboxes[1 - signs2][2] - ray4->oz) * invDir2);

	// Return the visit flags
	return  (tMax >= tMin);
}

static void QuadTriangle_Intersect(const __global QuadTiangle *qt, QuadRay *ray4, RayHit *rayHit) {
	//--------------------------------------------------------------------------
	// Calc. b1 coordinate

	// Read data from memory
	const float4 edge1x = qt->edge1x;
	const float4 edge1y = qt->edge1y;
	const float4 edge1z = qt->edge1z;

	const float4 edge2x = qt->edge2x;
	const float4 edge2y = qt->edge2y;
	const float4 edge2z = qt->edge2z;

	const float4 s1x = (ray4->dy * edge2z) - (ray4->dz * edge2y);
	const float4 s1y = (ray4->dz * edge2x) - (ray4->dx * edge2z);
	const float4 s1z = (ray4->dx * edge2y) - (ray4->dy * edge2x);

	const float4 divisor = (s1x * edge1x) + (s1y * edge1y) + (s1z * edge1z);

	const float4 dx = ray4->ox - qt->origx;
	const float4 dy = ray4->oy - qt->origy;
	const float4 dz = ray4->oz - qt->origz;

	const float4 b1 = ((dx * s1x) + (dy * s1y) + (dz * s1z)) / divisor;

	//--------------------------------------------------------------------------
	// Calc. b2 coordinate

	const float4 s2x = (dy * edge1z) - (dz * edge1y);
	const float4 s2y = (dz * edge1x) - (dx * edge1z);
	const float4 s2z = (dx * edge1y) - (dy * edge1x);

	const float4 b2 = ((ray4->dx * s2x) + (ray4->dy * s2y) + (ray4->dz * s2z)) / divisor;

	//--------------------------------------------------------------------------
	// Calc. b0 coordinate

	const float4 b0 = ((float4)1.f) - b1 - b2;

	//--------------------------------------------------------------------------

	const float4 t = ((edge2x * s2x) + (edge2y * s2y) + (edge2z * s2z)) / divisor;

    // The '&&' operator on int4 is still bugged in the ATI compiler
	// It looks like other logic operators don't work on HD4xxx family too

	unsigned int hit = 4;
	float _b1, _b2;
	float maxt = ray4->maxt.s0;
	if ((divisor.s0 != 0.f) && (b0.s0 >= 0.f) && (b1.s0 >= 0.f) && (b2.s0 >= 0.f) && (t.s0 > ray4->mint.s0) && (t.s0 < maxt)) {
		hit = 0;
		maxt = t.s0;
		_b1 = b1.s0;
		_b2 = b2.s0;
	}
	if ((divisor.s1 != 0.f) && (b0.s1 >= 0.f) && (b1.s1 >= 0.f) && (b2.s1 >= 0.f) && (t.s1 > ray4->mint.s0) && (t.s1 < maxt)) {
		hit = 1;
		maxt = t.s1;
		_b1 = b1.s1;
		_b2 = b2.s1;
	}
	if ((divisor.s2 != 0.f) && (b0.s2 >= 0.f) && (b1.s2 >= 0.f) && (b2.s2 >= 0.f) && (t.s2 > ray4->mint.s0) && (t.s2 < maxt)) {
		hit = 2;
		maxt = t.s2;
		_b1 = b1.s2;
		_b2 = b2.s2;
	}
	if ((divisor.s3 != 0.f) && (b0.s3 >= 0.f) && (b1.s3 >= 0.f) && (b2.s3 >= 0.f) && (t.s3 > ray4->mint.s0) && (t.s3 < maxt)) {
		hit = 3;
		maxt = t.s3;
		_b1 = b1.s3;
		_b2 = b2.s3;
	}

	if (hit == 4)
		return;

	ray4->maxt = (float4)maxt;

	rayHit->t = maxt;
	rayHit->b1 = _b1;
	rayHit->b2 = _b2;
	rayHit->index = qt->primitives[hit];
}

__kernel void Intersect(
		__global Ray *rays,
		__global RayHit *rayHits,
		__global QBVHNode *nodes,
		__global QuadTiangle *quadTris,
		const unsigned int rayCount,
		__local int *nodeStacks) {
	// Select the ray to check
	const int gid = get_global_id(0);
	if (gid >= rayCount)
		return;

	// Prepare the ray for intersection
	QuadRay ray4;
	{
			__global float4 *basePtr =(__global float4 *)&rays[gid];
			float4 data0 = (*basePtr++);
			float4 data1 = (*basePtr);

			ray4.ox = (float4)data0.x;
			ray4.oy = (float4)data0.y;
			ray4.oz = (float4)data0.z;

			ray4.dx = (float4)data0.w;
			ray4.dy = (float4)data1.x;
			ray4.dz = (float4)data1.y;

			ray4.mint = (float4)data1.z;
			ray4.maxt = (float4)data1.w;
	}

	const float4 invDir0 = (float4)(1.f / ray4.dx.s0);
	const float4 invDir1 = (float4)(1.f / ray4.dy.s0);
	const float4 invDir2 = (float4)(1.f / ray4.dz.s0);

	const int signs0 = (ray4.dx.s0 < 0.f);
	const int signs1 = (ray4.dy.s0 < 0.f);
	const int signs2 = (ray4.dz.s0 < 0.f);

	RayHit rayHit;
	rayHit.index = 0xffffffffu;

	//------------------------------
	// Main loop
	int todoNode = 0; // the index in the stack
	__local int *nodeStack = &nodeStacks[24 * get_local_id(0)];
	nodeStack[0] = 0; // first node to handle: root node

	while (todoNode >= 0) {
		const int nodeData = nodeStack[todoNode];
		--todoNode;

		// Leaves are identified by a negative index
		if (!QBVHNode_IsLeaf(nodeData)) {
			__global QBVHNode *node = &nodes[nodeData];
			const int4 visit = QBVHNode_BBoxIntersect(node, &ray4,
				invDir0, invDir1, invDir2,
				signs0, signs1, signs2);

			const int4 children = node->children;

			// For some reason doing logic operations with int4 is very slow
			nodeStack[todoNode + 1] = children.s3;
			todoNode += (visit.s3 && !QBVHNode_IsEmpty(children.s3)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s2;
			todoNode += (visit.s2 && !QBVHNode_IsEmpty(children.s2)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s1;
			todoNode += (visit.s1 && !QBVHNode_IsEmpty(children.s1)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s0;
			todoNode += (visit.s0 && !QBVHNode_IsEmpty(children.s0)) ? 1 : 0;
		} else {
			// Perform intersection
			const unsigned int nbQuadPrimitives = QBVHNode_NbQuadPrimitives(nodeData);
			const unsigned int offset = QBVHNode_FirstQuadIndex(nodeData);

			for (unsigned int primNumber = offset; primNumber < (offset + nbQuadPrimitives); ++primNumber)
				QuadTriangle_Intersect(&quadTris[primNumber], &ray4, &rayHit);
		}
	}

	// Write result
	rayHits[gid].t = rayHit.t;
	rayHits[gid].b1 = rayHit.b1;
	rayHits[gid].b2 = rayHit.b2;
	rayHits[gid].index = rayHit.index;
}
