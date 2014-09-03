#line 2 "qbvh_types.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

typedef struct QuadRay {
	float4 ox, oy, oz;
	float4 dx, dy, dz;
	float4 mint, maxt;
} QuadRay;

typedef struct {
	float4 origx, origy, origz;
	float4 edge1x, edge1y, edge1z;
	float4 edge2x, edge2y, edge2z;
	uint4 meshIndex, triangleIndex;
} QuadTiangle;

typedef struct {
	float4 bboxes[2][3];
	int4 children;
} QBVHNode;

#define emptyLeafNode 0xffffffff

#define QBVHNode_IsLeaf(index) (index < 0)
#define QBVHNode_IsEmpty(index) (index == emptyLeafNode)
#define QBVHNode_NbQuadPrimitives(index) ((uint)(((index >> 27) & 0xf) + 1))
#define QBVHNode_FirstQuadIndex(index) (index & 0x07ffffff)

// Using invDir0/invDir1/invDir2 instead of an
// array because I dont' trust OpenCL compiler =)
int4 QBVHNode_BBoxIntersect(
        const float4 bboxes_minX, const float4 bboxes_maxX,
        const float4 bboxes_minY, const float4 bboxes_maxY,
        const float4 bboxes_minZ, const float4 bboxes_maxZ,
        const QuadRay *ray4,
		const float4 invDir0, const float4 invDir1, const float4 invDir2) {
	float4 tMin = ray4->mint;
	float4 tMax = ray4->maxt;

	// X coordinate
	tMin = fmax(tMin, (bboxes_minX - ray4->ox) * invDir0);
	tMax = fmin(tMax, (bboxes_maxX - ray4->ox) * invDir0);

	// Y coordinate
	tMin = fmax(tMin, (bboxes_minY - ray4->oy) * invDir1);
	tMax = fmin(tMax, (bboxes_maxY - ray4->oy) * invDir1);

	// Z coordinate
	tMin = fmax(tMin, (bboxes_minZ - ray4->oz) * invDir2);
	tMax = fmin(tMax, (bboxes_maxZ - ray4->oz) * invDir2);

	// Return the visit flags
	return  (tMax >= tMin);
}

void QuadTriangle_Intersect(
    const float4 origx, const float4 origy, const float4 origz,
    const float4 edge1x, const float4 edge1y, const float4 edge1z,
    const float4 edge2x, const float4 edge2y, const float4 edge2z,
    const uint4 meshIndex,  const uint4 triangleIndex,
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
    uint mIndex, tIndex;

    int4 cond = isnotequal(divisor, (float4)0.f) & isgreaterequal(b0, (float4)0.f) &
			isgreaterequal(b1, (float4)0.f) & isgreaterequal(b2, (float4)0.f) &
			isgreater(t, ray4->mint);

    const int cond0 = cond.s0 && (t.s0 < maxt);
    maxt = select(maxt, t.s0, cond0);
    _b1 = select(0.f, b1.s0, cond0);
    _b2 = select(0.f, b2.s0, cond0);
    mIndex = select(NULL_INDEX, meshIndex.s0, cond0);
	tIndex = select(NULL_INDEX, triangleIndex.s0, cond0);

    const int cond1 = cond.s1 && (t.s1 < maxt);
    maxt = select(maxt, t.s1, cond1);
    _b1 = select(_b1, b1.s1, cond1);
    _b2 = select(_b2, b2.s1, cond1);
    mIndex = select(mIndex, meshIndex.s1, cond1);
	tIndex = select(tIndex, triangleIndex.s1, cond1);

    const int cond2 = cond.s2 && (t.s2 < maxt);
    maxt = select(maxt, t.s2, cond2);
    _b1 = select(_b1, b1.s2, cond2);
    _b2 = select(_b2, b2.s2, cond2);
    mIndex = select(mIndex, meshIndex.s2, cond2);
	tIndex = select(tIndex, triangleIndex.s2, cond2);

    const int cond3 = cond.s3 && (t.s3 < maxt);
    maxt = select(maxt, t.s3, cond3);
    _b1 = select(_b1, b1.s3, cond3);
    _b2 = select(_b2, b2.s3, cond3);
    mIndex = select(mIndex, meshIndex.s3, cond3);
	tIndex = select(tIndex, triangleIndex.s3, cond3);

	if (mIndex == NULL_INDEX)
		return;

	ray4->maxt = (float4)maxt;

	rayHit->t = maxt;
	rayHit->b1 = _b1;
	rayHit->b2 = _b2;
	rayHit->meshIndex = mIndex;
	rayHit->triangleIndex = tIndex;
}
