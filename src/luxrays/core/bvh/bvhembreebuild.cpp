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

#include <vector>
#include <boost/foreach.hpp>

#include <embree2/rtcore.h>
#include <embree2/rtcore_bvh_builder.h>

#include "luxrays/core/bvh/bvhbuild.h"

using namespace std;

namespace luxrays {

static void *NodeAllocFunc() {
	BVHTreeNode *node = new BVHTreeNode();

	node->leftChild = NULL;
	node->rightSibling = NULL;

	return node;
}

static void *LeafAllocFunc(const RTCPrimRef *prim) {
	BVHTreeNode *node = new BVHTreeNode();
	
	node->bbox.pMin.x = prim->lower_x;
	node->bbox.pMin.y = prim->lower_y;
	node->bbox.pMin.z = prim->lower_z;
	node->triangleLeaf.meshIndex = prim->geomID;

	node->bbox.pMax.x = prim->upper_x;
	node->bbox.pMax.y = prim->upper_y;
	node->bbox.pMax.z = prim->upper_z;
	node->triangleLeaf.triangleIndex = prim->primID;

	node->leftChild = NULL;
	node->rightSibling = NULL;

	return node;
}

static void *NodeChildrenPtrFunc(void *n, const size_t i) {
	BVHTreeNode *node = (BVHTreeNode *)n;

	// I'm using rightSibling as right child here 
	return (i == 0) ? &(node->leftChild) : &(node->rightSibling);
}

static void NodeChildrenSetBBoxFunc(void *n, const size_t i, const float lower[3], const float upper[3]) {
	BVHTreeNode *node = (BVHTreeNode *)n;

	node->bbox.pMin.x = lower[0];
	node->bbox.pMin.y = lower[1];
	node->bbox.pMin.z = lower[2];

	node->bbox.pMax.x = upper[0];
	node->bbox.pMax.y = upper[1];
	node->bbox.pMax.z = upper[2];
}

BVHTreeNode *BuildEmbreeBVH(const BVHParams &params, vector<BVHTreeNode *> &list) {
	// Initialize RTCPrimRef vector
	vector<RTCPrimRef> prims(list.size());
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < prims.size(); ++i) {
		RTCPrimRef &prim = prims[i];
		BVHTreeNode *node = list[i];

		prim.lower_x = node->bbox.pMin.x;
		prim.lower_y = node->bbox.pMin.y;
		prim.lower_z = node->bbox.pMin.z;
		prim.geomID = node->triangleLeaf.meshIndex;

		prim.upper_x = node->bbox.pMax.x;
		prim.upper_y = node->bbox.pMax.y;
		prim.upper_z = node->bbox.pMax.z;
		prim.primID = node->triangleLeaf.triangleIndex;
	}

	BVHTreeNode *root = (BVHTreeNode *)rtcBVHBuilderBinnedSAH(&prims[0], prims.size(),
			&NodeAllocFunc, &LeafAllocFunc, &NodeChildrenPtrFunc, &NodeChildrenSetBBoxFunc);

	PrintBVHNodes(cout, root);

	exit(1);
}

}
