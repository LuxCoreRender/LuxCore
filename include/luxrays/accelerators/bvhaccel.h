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

#ifndef _LUXRAYS_BVHACCEL_H
#define	_LUXRAYS_BVHACCEL_H

#include <vector>

#include "luxrays/luxrays.h"
#include "luxrays/core/acceleretor.h"

namespace luxrays {

struct BVHAccelTreeNode {
	BBox bbox;
	unsigned int primitive;
	BVHAccelTreeNode *leftChild;
	BVHAccelTreeNode *rightSibling;
};

struct BVHAccelArrayNode {
	BBox bbox;
	unsigned int primitive;
	unsigned int skipIndex;
};

// BVHAccel Declarations
class BVHAccel : public Accelerator {
public:
	// BVHAccel Public Methods
	BVHAccel(const Context *context,
			const unsigned int triangleCount, const Triangle *p, const Point *v,
			const unsigned int treetype, const int csamples, const int icost,
			const int tcost, const float ebonus);
	~BVHAccel();

	AcceleratorType GetType() const { return ACCEL_BVH; }

	bool Intersect(const Ray *ray, RayHit *hit) const;

	// BVHAccel Private Data
	unsigned int treeType;
	int costSamples, isectCost, traversalCost;
	float emptyBonus;
	unsigned int nPrims;
	const Point *vertices;
	const Triangle *triangles;
	unsigned int nNodes;
	BVHAccelArrayNode *bvhTree;

private:
	// BVHAccel Private Methods
	BVHAccelTreeNode *BuildHierarchy(std::vector<BVHAccelTreeNode *> &list, unsigned int begin, unsigned int end, unsigned int axis);
	void FindBestSplit(std::vector<BVHAccelTreeNode *> &list, unsigned int begin, unsigned int end, float *splitValue, unsigned int *bestAxis);
	unsigned int BuildArray(BVHAccelTreeNode *node, unsigned int offset);
	void FreeHierarchy(BVHAccelTreeNode *node);
};

}

#endif	/* _LUXRAYS_BVHACCEL_H */
