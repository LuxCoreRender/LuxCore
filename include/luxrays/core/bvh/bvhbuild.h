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

#ifndef _LUXRAYS_BVHBUILD_H
#define	_LUXRAYS_BVHBUILD_H

#include <vector>
#include <ostream>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/bbox.h"

namespace luxrays {

typedef struct {
	u_int treeType;
	int costSamples, isectCost, traversalCost;
	float emptyBonus;
} BVHParams;

struct BVHTreeNode {
	BBox bbox;
	union {
		struct {
			u_int meshIndex, triangleIndex;
		} triangleLeaf;
		struct {
			u_int leafIndex;
			u_int transformIndex, motionIndex; // transformIndex or motionIndex have to be NULL_INDEX
			u_int meshOffsetIndex;
			bool isMotionMesh; // If I have to use motionIndex or transformIndex
		} bvhLeaf;
	};

	BVHTreeNode *leftChild;
	BVHTreeNode *rightSibling;
};

// Old classic BVH build
extern BVHTreeNode *BuildBVH(u_int *nNodes, const BVHParams &params,
	std::vector<BVHTreeNode *> &list);
// Embree BVH build
extern BVHTreeNode *BuildEmbreeBVH(const BVHParams &params,
	std::vector<BVHTreeNode *> &list);

// Common functions
extern void FreeBVH(BVHTreeNode *node);
extern u_int CountBVHNodes(BVHTreeNode *node);
extern void PrintBVHNodes(std::ostream &stream, BVHTreeNode *node);

}

#endif	/* _LUXRAYS_BVHACCEL_H */
