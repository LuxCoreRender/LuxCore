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

#ifndef _LUXRAYS_BBOX_H
#define _LUXRAYS_BBOX_H

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/ray.h"

namespace luxrays {

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

	bool Overlaps(const BBox &b) const {
		bool x = (pMax.x >= b.pMin.x) && (pMin.x <= b.pMax.x);
		bool y = (pMax.y >= b.pMin.y) && (pMin.y <= b.pMax.y);
		bool z = (pMax.z >= b.pMin.z) && (pMin.z <= b.pMax.z);
		return (x && y && z);
	}

	bool Inside(const Point &pt) const {
		return (pt.x >= pMin.x && pt.x <= pMax.x &&
				pt.y >= pMin.y && pt.y <= pMax.y &&
				pt.z >= pMin.z && pt.z <= pMax.z);
	}

	void Expand(const float delta) {
		pMin -= Vector(delta, delta, delta);
		pMax += Vector(delta, delta, delta);
	}

	float Volume() const {
		Vector d = pMax - pMin;
		return d.x * d.y * d.z;
	}

	float SurfaceArea() const {
		Vector d = pMax - pMin;
		return 2.f * (d.x * d.y + d.y * d.z + d.z * d.x);
	}

	int MaximumExtent() const {
		Vector diag = pMax - pMin;
		if (diag.x > diag.y && diag.x > diag.z)
			return 0;
		else if (diag.y > diag.z)
			return 1;
		else
			return 2;
	}
	void BoundingSphere(Point *c, float *rad) const;
	bool IntersectP(const Ray &ray,
			float *hitt0 = NULL,
			float *hitt1 = NULL) const;

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

}

#endif	/* _LUXRAYS_BOX_H */
