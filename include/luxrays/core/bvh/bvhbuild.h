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
#include "luxrays/core/trianglemesh.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/bvh/bvhbuild_types.cl"
}

#define BVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define BVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)

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
	std::vector<BVHTreeNode *> &leafList);
extern u_int BuildBVHArray(const std::deque<const Mesh *> *meshes, BVHTreeNode *node,
		u_int offset, luxrays::ocl::BVHArrayNode *bvhArrayTree);
extern luxrays::ocl::BVHArrayNode *BuildBVH(const BVHParams &params,
		u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList);


// Embree BVH build
extern luxrays::ocl::BVHArrayNode *BuildEmbreeBVH(const BVHParams &params,
		u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList);

// Common functions
extern void FreeBVH(BVHTreeNode *node);
extern u_int CountBVHNodes(BVHTreeNode *node);
extern void PrintBVHNodes(std::ostream &stream, BVHTreeNode *node);

}

#endif	/* _LUXRAYS_BVHACCEL_H */
