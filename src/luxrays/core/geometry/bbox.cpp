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

#include "luxrays/core/geometry/bbox.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/ray.h"

namespace luxrays {

// BBox Method Definitions

BBox Union(const BBox &b, const Point &p) {
	BBox ret = b;
	ret.pMin.x = Min(b.pMin.x, p.x);
	ret.pMin.y = Min(b.pMin.y, p.y);
	ret.pMin.z = Min(b.pMin.z, p.z);
	ret.pMax.x = Max(b.pMax.x, p.x);
	ret.pMax.y = Max(b.pMax.y, p.y);
	ret.pMax.z = Max(b.pMax.z, p.z);
	return ret;
}

BBox Union(const BBox &b, const BBox &b2) {
	BBox ret;
	ret.pMin.x = Min(b.pMin.x, b2.pMin.x);
	ret.pMin.y = Min(b.pMin.y, b2.pMin.y);
	ret.pMin.z = Min(b.pMin.z, b2.pMin.z);
	ret.pMax.x = Max(b.pMax.x, b2.pMax.x);
	ret.pMax.y = Max(b.pMax.y, b2.pMax.y);
	ret.pMax.z = Max(b.pMax.z, b2.pMax.z);
	return ret;
}

bool Overlaps(BBox &result, const BBox &b1, const BBox &b2) {
	if (!b1.Overlaps(b2))
		return false;
	
	result.pMin.x = Max(b1.pMin.x, b2.pMin.x);
	result.pMin.y = Max(b1.pMin.y, b2.pMin.y);
	result.pMin.z = Max(b1.pMin.z, b2.pMin.z);
	result.pMax.x = Min(b1.pMax.x, b2.pMax.x);
	result.pMax.y = Min(b1.pMax.y, b2.pMax.y);
	result.pMax.z = Min(b1.pMax.z, b2.pMax.z);

	return true;
}

void BBox::BoundingSphere(Point *c, float *rad) const {
	*c = .5f * (pMin + pMax);
	*rad = Inside(*c) ? Distance(*c, pMax) : 0.f;
}

BSphere BBox::BoundingSphere() const {
	const Point c = .5f * (pMin + pMax);
	const float rad = Inside(c) ? Distance(c, pMax) : 0.f;

	return BSphere(c, rad);
}

Point PlaneClipEdge(const Point &planeOrig, const Normal &planeNormal,
		const Point &a, const Point &b) {
	const float distA = Dot(a - planeOrig, planeNormal);
	const float distB = Dot(b - planeOrig, planeNormal);

	const float s = distA / (distA - distB);

	return Point(a.x + s * (b.x - a.x),
			a.y + s * (b.y - a.y),
			a.z + s * (b.z - a.z));
}

vector<Point> PlaneClipPolygon(const Point &clippingPlaneOrigin,
		const Normal &clippingPlaneNormal,
		const vector<Point> &vertexList) {
	if (vertexList.size() == 0)
		return vector<Point>();

	vector<Point> outputList;
	Point S = vertexList[vertexList.size() - 1];
	for (size_t j = 0; j < vertexList.size(); ++j) {
		const Point &E = vertexList[j];

		if (Dot(E - clippingPlaneOrigin, clippingPlaneNormal) >= 0.f) {
			if (Dot(S - clippingPlaneOrigin, clippingPlaneNormal) < 0.f)
				outputList.push_back(PlaneClipEdge(clippingPlaneOrigin, clippingPlaneNormal, S, E));

			outputList.push_back(E);
		} else if (Dot(S - clippingPlaneOrigin, clippingPlaneNormal) >= 0.f) {
			outputList.push_back(PlaneClipEdge(clippingPlaneOrigin, clippingPlaneNormal, S, E));
		}

		S = E;
	}

	return outputList;
}

vector<Point> BBox::ClipPolygon(const vector<Point> &vertexList) const {
	const Point clippingPlaneOrigin[6] = {
		pMin,
		pMin,
		pMin,
		pMax,
		pMax,
		pMax
	};

	static const Normal clippingPlaneNormal[6] = {
		Normal(1.f, 0.f, 0.f),
		Normal(0.f, 1.f, 0.f),
		Normal(0.f, 0.f, 1.f),
		Normal(-1.f, 0.f, 0.f),
		Normal(0.f, -1.f, 0.f),
		Normal(0.f, 0.f, -1.f),
	};

	vector<Point> vlist = vertexList;
	// For each bounding box plane
	for (size_t i = 0; i < 6; ++i)
		vlist = PlaneClipPolygon(clippingPlaneOrigin[i], clippingPlaneNormal[i], vlist);

	return vlist;
}

// NOTE - lordcrc - BBox::IntersectP relies on IEEE 754 behaviour of infinity and /fp:fast breaks this
#if defined(WIN32) && !defined(__CYGWIN__)
#pragma float_control(push)
#pragma float_control(precise, on)
#endif
bool BBox::IntersectP(const Ray &ray,
		const Point &pMin, const Point &pMax,
		float *hitt0, float *hitt1) {
	float t0 = ray.mint, t1 = ray.maxt;
	for (int i = 0; i < 3; ++i) {
		// Update interval for _i_th bounding box slab
		const float invRayDir = 1.f / ray.d[i];
		float tNear = (pMin[i] - ray.o[i]) * invRayDir;
		float tFar = (pMax[i] - ray.o[i]) * invRayDir;
		// Update parametric interval from slab intersection $t$s
		if (tNear > tFar) Swap(tNear, tFar);
		t0 = tNear > t0 ? tNear : t0;
		t1 = tFar < t1 ? tFar : t1;
		if (t0 > t1) return false;
	}
	if (hitt0) *hitt0 = t0;
	if (hitt1) *hitt1 = t1;
	return true;
}

bool BBox::IntersectP(const Ray &ray, float *hitt0, float *hitt1) const {
	return IntersectP(ray, pMin, pMax, hitt0, hitt1);
}
#if defined(WIN32) && !defined(__CYGWIN__)
#pragma float_control(pop)
#endif

}
