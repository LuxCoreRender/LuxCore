/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _LUXRAYS_BBOX_H
#define _LUXRAYS_BBOX_H

#include <vector>

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/bsphere.h"

namespace luxrays {

	// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/bbox_types.cl"
}

class Normal;
class Ray;

class BBox {
public:
	// BBox Public Methods

	BBox() {
		pMin = Point(INFINITY, INFINITY, INFINITY);
		pMax = Point(-INFINITY, -INFINITY, -INFINITY);
	}

	BBox(const Point &p) : pMin(p), pMax(p) {
	}

	BBox(const Point &p1, const Point &p2) {
		pMin = Point(Min(p1.x, p2.x),
				Min(p1.y, p2.y),
				Min(p1.z, p2.z));
		pMax = Point(Max(p1.x, p2.x),
				Max(p1.y, p2.y),
				Max(p1.z, p2.z));
	}

	friend bool Overlaps(BBox &result, const BBox &b1, const BBox &b2);

	bool Overlaps(const BBox &b) const {
		const bool x = (pMax.x >= b.pMin.x) && (pMin.x <= b.pMax.x);
		const bool y = (pMax.y >= b.pMin.y) && (pMin.y <= b.pMax.y);
		const bool z = (pMax.z >= b.pMin.z) && (pMin.z <= b.pMax.z);
		return (x && y && z);
	}

	bool Inside(const Point &pt) const {
		return (pt.x >= pMin.x && pt.x <= pMax.x &&
				pt.y >= pMin.y && pt.y <= pMax.y &&
				pt.z >= pMin.z && pt.z <= pMax.z);
	}

	bool Inside(const BBox &bb) const {
		return (bb.pMin.x >= pMin.x && bb.pMax.x <= pMax.x &&
				bb.pMin.y >= pMin.y && bb.pMax.y <= pMax.y &&
				bb.pMin.z >= pMin.z && bb.pMax.z <= pMax.z);
	}

	void Expand(const float delta) {
		pMin -= Vector(delta, delta, delta);
		pMax += Vector(delta, delta, delta);
	}
	
	void Expand(const Point &p) {
		pMin = Point(Min(pMin.x, p.x),
				Min(pMin.y, p.y),
				Min(pMin.z, p.z));
		pMax = Point(Max(pMax.x, p.x),
				Max(pMax.y, p.y),
				Max(pMax.z, p.z));
	}

	float Volume() const {
		Vector d = pMax - pMin;
		return d.x * d.y * d.z;
	}

	float SurfaceArea() const {
		Vector d = pMax - pMin;
		return 2.f * (d.x * d.y + d.y * d.z + d.z * d.x);
	}

	u_int MaximumExtent() const {
		Vector diag = pMax - pMin;
		if (diag.x > diag.y && diag.x > diag.z)
			return 0;
		else if (diag.y > diag.z)
			return 1;
		else
			return 2;
	}
	void BoundingSphere(Point *c, float *rad) const;
	BSphere BoundingSphere() const;

	bool IntersectP(const Ray &ray,
			float *hitt0 = NULL,
			float *hitt1 = NULL) const;
	static bool IntersectP(const Ray &ray,
			const Point &pMin, const Point &pMax,
			float *hitt0 = NULL,
			float *hitt1 = NULL);

	// Returns the list of vertices of the clipped polygon
	// against this bounding box
	std::vector<Point> ClipPolygon(const std::vector<Point> &vertexList) const;
	bool IsValid() const {
		return (pMin.x <= pMax.x) && (pMin.y <= pMax.y) && (pMin.z <= pMax.z);
	}

	Point Center() const {
		return (pMin + pMax) * .5f;
	}

	friend inline std::ostream &operator<<(std::ostream &os, const BBox &b);
	friend BBox Union(const BBox &b, const Point &p);
	friend BBox Union(const BBox &b, const BBox &b2);

	// BBox Public Data
	Point pMin, pMax;
};

extern BBox Union(const BBox &b, const Point &p);
extern BBox Union(const BBox &b, const BBox &b2);

inline std::ostream &operator<<(std::ostream &os, const BBox &b) {
	os << "BBox[" << b.pMin << ", " << b.pMax << "]";
	return os;
}

extern Point PlaneClipEdge(const Point &planeOrig, const Normal &planeNormal,
		const Point &a, const Point &b);
extern std::vector<Point> PlaneClipPolygon(const Point &clippingPlaneOrigin,
		const Normal &clippingPlaneNormal,
		const std::vector<Point> &vertexList);

}

#endif	/* _LUXRAYS_BOX_H */
