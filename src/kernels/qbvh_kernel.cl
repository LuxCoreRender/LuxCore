/***************************************************************************
 *   Copyright (C) 1998-2009 by David Bucciarelli (davibu@interfree.it)    *
 *                                                                         *
 *   This file is part of SmallLuxGPU.                                     *
 *                                                                         *
 *   SmallLuxGPU is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   SmallLuxGPU is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   and Lux Renderer website : http://www.luxrender.net                   *
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

static int4 QBVHNode_BBoxIntersect(__global QBVHNode *node, const QuadRay *ray4,
		const float4 invDir[3], const int sign[3]) {
	float4 tMin = ray4->mint;
	float4 tMax = ray4->maxt;

	// X coordinate
	tMin = max(tMin, (node->bboxes[sign[0]][0] - ray4->ox) * invDir[0]);
	tMax = min(tMax, (node->bboxes[1 - sign[0]][0] - ray4->ox) * invDir[0]);

	// Y coordinate
	tMin = max(tMin, (node->bboxes[sign[1]][1] - ray4->oy) * invDir[1]);
	tMax = min(tMax, (node->bboxes[1 - sign[1]][1] - ray4->oy) * invDir[1]);

	// Z coordinate
	tMin = max(tMin, (node->bboxes[sign[2]][2] - ray4->oz) * invDir[2]);
	tMax = min(tMax, (node->bboxes[1 - sign[2]][2] - ray4->oz) * invDir[2]);

	//return the visit flags
	return  (tMax >= tMin);
}

static void QuadTriangle_Intersect(const __global QuadTiangle *qt, QuadRay *ray4, RayHit *rayHit) {
	const float4 zero = (float4)0.f;

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

	// The '&&' operator is still bugged in the ATI compiler
	const int4 test = (divisor != zero) &
		(b0 >= zero) & (b1 >= zero) & (b2 >= zero) &
		(t > ray4->mint) & (t < ray4->maxt);

	unsigned int hit = 4;
	float _b1, _b2;
	float maxt = ray4->maxt.s0;
	if (test.s0 && (t.s0 < maxt)) {
		hit = 0;
		maxt = t.s0;
		_b1 = b1.s0;
		_b2 = b2.s0;
	}
	if (test.s1 && (t.s1 < maxt)) {
		hit = 1;
		maxt = t.s1;
		_b1 = b1.s1;
		_b2 = b2.s1;
	}
	if (test.s2 && (t.s2 < maxt)) {
		hit = 2;
		maxt = t.s2;
		_b1 = b1.s2;
		_b2 = b2.s2;
	}
	if (test.s3 && (t.s3 < maxt)) {
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
		const unsigned int rayCount) {
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

	float4 invDir[3];
	invDir[0] = (float4)(1.f / ray4.dx.s0);
	invDir[1] = (float4)(1.f / ray4.dy.s0);
	invDir[2] = (float4)(1.f / ray4.dz.s0);

	int signs[3];
	signs[0] = (ray4.dx.s0 < 0.f);
	signs[1] = (ray4.dy.s0 < 0.f);
	signs[2] = (ray4.dz.s0 < 0.f);

	RayHit rayHit;
	rayHit.index = 0xffffffffu;

	//------------------------------
	// Main loop
	int todoNode = 0; // the index in the stack
	int nodeStack[24];
	nodeStack[0] = 0; // first node to handle: root node

	while (todoNode >= 0) {
		const int nodeData = nodeStack[todoNode];
		--todoNode;

		// Leaves are identified by a negative index
		if (!QBVHNode_IsLeaf(nodeData)) {
			__global QBVHNode *node = &nodes[nodeData];
			const int4 visit = QBVHNode_BBoxIntersect(node, &ray4, invDir, signs);

			const int4 children = node->children;
			if (visit.s3)
				nodeStack[++todoNode] = children.s3;
			if (visit.s2)
				nodeStack[++todoNode] = children.s2;
			if (visit.s1)
				nodeStack[++todoNode] = children.s1;
			if (visit.s0)
				nodeStack[++todoNode] = children.s0;
		} else {
			//----------------------
			// It is a leaf,
			// all the informations are encoded in the index

			if (QBVHNode_IsEmpty(nodeData))
				continue;

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
