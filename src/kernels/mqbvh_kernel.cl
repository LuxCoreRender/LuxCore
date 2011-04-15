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
	uint index;
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
	uint4 primitives;
} QuadTiangle;

typedef struct {
	float4 bboxes[2][3];
	int4 children;
} QBVHNode;

typedef struct {
    float m[4][4];
} Matrix4x4;

void TransformP(Point *ptrans, Point *p, __global Matrix4x4 *m) {
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

void TransformV(Vector *ptrans, Vector *p, __global Matrix4x4 *m) {
    const float x = p->x;
    const float y = p->y;
    const float z = p->z;

	ptrans->x = m->m[0][0] * x + m->m[0][1] * y + m->m[0][2] * z;
	ptrans->y = m->m[1][0] * x + m->m[1][1] * y + m->m[1][2] * z;
	ptrans->z = m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z;
}

#define emptyLeafNode 0xffffffff

#define QBVHNode_IsLeaf(index) (index < 0)
#define QBVHNode_IsEmpty(index) (index == emptyLeafNode)
#define QBVHNode_NbQuadPrimitives(index) ((uint)(((index >> 27) & 0xf) + 1))
#define QBVHNode_FirstQuadIndex(index) (index & 0x07ffffff)

// Using invDir0/invDir1/invDir2 and sign0/sign1/sign2 instead of an
// array because I dont' trust OpenCL compiler =)
int4 QBVHNode_BBoxIntersect(
        const float4 bboxes_minX, const float4 bboxes_maxX,
        const float4 bboxes_minY, const float4 bboxes_maxY,
        const float4 bboxes_minZ, const float4 bboxes_maxZ,
        const QuadRay *ray4,
		const float4 invDir0, const float4 invDir1, const float4 invDir2,
		const int signs0, const int signs1, const int signs2) {
	float4 tMin = ray4->mint;
	float4 tMax = ray4->maxt;

	// X coordinate
	tMin = max(tMin, (bboxes_minX - ray4->ox) * invDir0);
	tMax = min(tMax, (bboxes_maxX - ray4->ox) * invDir0);

	// Y coordinate
	tMin = max(tMin, (bboxes_minY - ray4->oy) * invDir1);
	tMax = min(tMax, (bboxes_maxY - ray4->oy) * invDir1);

	// Z coordinate
	tMin = max(tMin, (bboxes_minZ - ray4->oz) * invDir2);
	tMax = min(tMax, (bboxes_maxZ - ray4->oz) * invDir2);

	// Return the visit flags
	return  (tMax >= tMin);
}

void QuadTriangle_Intersect(
    const float4 origx, const float4 origy, const float4 origz,
    const float4 edge1x, const float4 edge1y, const float4 edge1z,
    const float4 edge2x, const float4 edge2y, const float4 edge2z,
    const uint4 primitives,
    QuadRay *ray4, RayHit *rayHit) {
	//--------------------------------------------------------------------------
	// Calc. b1 coordinate

	const float4 s1x = (ray4->dy * edge2z) - (ray4->dz * edge2y);
	const float4 s1y = (ray4->dz * edge2x) - (ray4->dx * edge2z);
	const float4 s1z = (ray4->dx * edge2y) - (ray4->dy * edge2x);

	const float4 divisor = (s1x * edge1x) + (s1y * edge1y) + (s1z * edge1z);

	const float4 dx = ray4->ox - origx;
	const float4 dy = ray4->oy - origy;
	const float4 dz = ray4->oz - origz;

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

    float _b1, _b2;
	float maxt = ray4->maxt.s0;
    uint index;

    int4 cond = isnotequal(divisor, (float4)0.f) & isgreaterequal(b0, (float4)0.f) &
			isgreaterequal(b1, (float4)0.f) & isgreaterequal(b2, (float4)0.f) &
			isgreater(t, ray4->mint);

    const int cond0 = cond.s0 && (t.s0 < maxt);
    maxt = select(maxt, t.s0, cond0);
    _b1 = select(0.f, b1.s0, cond0);
    _b2 = select(0.f, b2.s0, cond0);
    index = select(0xffffffffu, primitives.s0, cond0);

    const int cond1 = cond.s1 && (t.s1 < maxt);
    maxt = select(maxt, t.s1, cond1);
    _b1 = select(_b1, b1.s1, cond1);
    _b2 = select(_b2, b2.s1, cond1);
    index = select(index, primitives.s1, cond1);

    const int cond2 = cond.s2 && (t.s2 < maxt);
    maxt = select(maxt, t.s2, cond2);
    _b1 = select(_b1, b1.s2, cond2);
    _b2 = select(_b2, b2.s2, cond2);
    index = select(index, primitives.s2, cond2);

    const int cond3 = cond.s3 && (t.s3 < maxt);
    maxt = select(maxt, t.s3, cond3);
    _b1 = select(_b1, b1.s3, cond3);
    _b2 = select(_b2, b2.s3, cond3);
    index = select(index, primitives.s3, cond3);

	if (index == 0xffffffffu)
		return;

	ray4->maxt = (float4)maxt;

	rayHit->t = maxt;
	rayHit->b1 = _b1;
	rayHit->b2 = _b2;
	rayHit->index = index;
}

void LeafIntersect(
		const Ray *ray,
		RayHit *rayHit,
		__global QBVHNode *nodes,
		__global QuadTiangle *quadTris) {
	// Prepare the ray for intersection
	QuadRay ray4;
    ray4.ox = (float4)ray->o.x;
    ray4.oy = (float4)ray->o.y;
    ray4.oz = (float4)ray->o.z;

    ray4.dx = (float4)ray->d.x;
    ray4.dy = (float4)ray->d.y;
    ray4.dz = (float4)ray->d.z;

    ray4.mint = (float4)ray->mint;
    ray4.maxt = (float4)ray->maxt;

	const float4 invDir0 = (float4)(1.f / ray4.dx.s0);
	const float4 invDir1 = (float4)(1.f / ray4.dy.s0);
	const float4 invDir2 = (float4)(1.f / ray4.dz.s0);

	const int signs0 = (ray4.dx.s0 < 0.f);
	const int signs1 = (ray4.dy.s0 < 0.f);
	const int signs2 = (ray4.dz.s0 < 0.f);

	rayHit->index = 0xffffffffu;

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
            const int4 visit = QBVHNode_BBoxIntersect(
                node->bboxes[signs0][0], node->bboxes[1 - signs0][0],
                node->bboxes[signs1][1], node->bboxes[1 - signs1][1],
                node->bboxes[signs2][2], node->bboxes[1 - signs2][2],
                &ray4,
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
			const uint nbQuadPrimitives = QBVHNode_NbQuadPrimitives(nodeData);
			const uint offset = QBVHNode_FirstQuadIndex(nodeData);

			for (uint primNumber = offset; primNumber < (offset + nbQuadPrimitives); ++primNumber) {
                __global QuadTiangle *quadTri = &quadTris[primNumber];
                const float4 origx = quadTri->origx;
                const float4 origy = quadTri->origy;
                const float4 origz = quadTri->origz;
                const float4 edge1x = quadTri->edge1x;
                const float4 edge1y = quadTri->edge1y;
                const float4 edge1z = quadTri->edge1z;
                const float4 edge2x = quadTri->edge2x;
                const float4 edge2y = quadTri->edge2y;
                const float4 edge2z = quadTri->edge2z;
                const uint4 primitives = quadTri->primitives;

				QuadTriangle_Intersect(
                    origx, origy, origz,
                    edge1x, edge1y, edge1z,
                    edge2x, edge2y, edge2z,
                    primitives,
                    &ray4, rayHit);
            }
		}
	}
}

__kernel void Intersect(
		__global Ray *rays,
		__global RayHit *rayHits,
		__global QBVHNode *nodes,
		const uint rayCount,
        __global unsigned int *qbvhMemMap,
		__global QBVHNode *leafNodes,
		__global QuadTiangle *leafQuadTris,
        __global Matrix4x4 *invTrans,
        __global unsigned int *leafsOffset) {
	// Select the ray to check
	const int gid = get_global_id(0);
	if (gid >= rayCount)
		return;

	// Prepare the ray for intersection
	QuadRay ray4;
    Point rayOrig;
    Vector rayDir;
	{
        __global float4 *basePtr =(__global float4 *)&rays[gid];
        float4 data0 = (*basePtr++);
        float4 data1 = (*basePtr);

        rayOrig.x = data0.x;
        rayOrig.y = data0.y;
        rayOrig.z = data0.z;

        rayDir.x = data0.w;
        rayDir.y = data1.x;
        rayDir.z = data1.y;

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
	int nodeStack[24];
	nodeStack[0] = 0; // first node to handle: root node

	while (todoNode >= 0) {
		const int nodeData = nodeStack[todoNode];
		--todoNode;

		// Leaves are identified by a negative index
		if (!QBVHNode_IsLeaf(nodeData)) {
			__global QBVHNode *node = &nodes[nodeData];
            const int4 visit = QBVHNode_BBoxIntersect(
                node->bboxes[signs0][0], node->bboxes[1 - signs0][0],
                node->bboxes[signs1][1], node->bboxes[1 - signs1][1],
                node->bboxes[signs2][2], node->bboxes[1 - signs2][2],
                &ray4,
				invDir0, invDir1, invDir2,
				signs0, signs1, signs2);

			const int4 children = node->children;

			// For some reason doing logic operations with int4 are very slow
			nodeStack[todoNode + 1] = children.s3;
			todoNode += (visit.s3 && !QBVHNode_IsEmpty(children.s3)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s2;
			todoNode += (visit.s2 && !QBVHNode_IsEmpty(children.s2)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s1;
			todoNode += (visit.s1 && !QBVHNode_IsEmpty(children.s1)) ? 1 : 0;
			nodeStack[todoNode + 1] = children.s0;
			todoNode += (visit.s0 && !QBVHNode_IsEmpty(children.s0)) ? 1 : 0;
		} else {
			// Perform intersection with QBVH leaf
			const uint leafIndex = QBVHNode_FirstQuadIndex(nodeData);

            Ray tray;
            TransformP(&tray.o, &rayOrig, &invTrans[leafIndex]);
            TransformV(&tray.d, &rayDir, &invTrans[leafIndex]);
            tray.mint = ray4.mint.s0;
            tray.maxt = ray4.maxt.s0;

            const unsigned int memIndex = leafIndex * 2;
            const unsigned int leafNodeOffset = qbvhMemMap[memIndex];
            __global QBVHNode *n = &leafNodes[leafNodeOffset];
            const unsigned int leafQuadTriOffset = qbvhMemMap[memIndex + 1];
            __global QuadTiangle *qt = &leafQuadTris[leafQuadTriOffset];

            RayHit tmpRayHit;
            LeafIntersect(&tray, &tmpRayHit, n, qt);

            if (tmpRayHit.index != 0xffffffffu) {
                rayHit.t = tmpRayHit.t;
                rayHit.b1 = tmpRayHit.b1;
                rayHit.b2 = tmpRayHit.b2;
                rayHit.index = tmpRayHit.index + leafsOffset[leafIndex];

                ray4.maxt = (float4)tmpRayHit.t;
            }
		}
	}

	// Write result
	rayHits[gid].t = rayHit.t;
	rayHits[gid].b1 = rayHit.b1;
	rayHits[gid].b2 = rayHit.b2;
	rayHits[gid].index = rayHit.index;
}
