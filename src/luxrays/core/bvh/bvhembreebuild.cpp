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

class EmbreeBVHNode {
public:
	EmbreeBVHNode() { }
	virtual ~EmbreeBVHNode() { }
};

class EmbreeBVHInnerNode : public EmbreeBVHNode {
public:
	EmbreeBVHInnerNode() {
		children[0] = NULL;
		children[1] = NULL;
	}
	virtual ~EmbreeBVHInnerNode() { }
	
	BBox bbox[2];
	EmbreeBVHNode *children[2];
};

class EmbreeBVHLeafNode : public EmbreeBVHNode {
public:
	EmbreeBVHLeafNode(const size_t i) : index(i) { }
	virtual ~EmbreeBVHLeafNode() { }

	size_t index;
};

static void FreeEmbreeBVHTree(EmbreeBVHNode *n) {
	if (n) {
		EmbreeBVHInnerNode *node = dynamic_cast<EmbreeBVHInnerNode *>(n);
		if (node) {
			FreeEmbreeBVHTree(node->children[0]);
			FreeEmbreeBVHTree(node->children[1]);
		}

		delete n;
	}
}

static BVHTreeNode *TransformEmbreeBVH(const EmbreeBVHNode *n, vector<BVHTreeNode *> &list) {
	if (n) {
		const EmbreeBVHInnerNode *node = dynamic_cast<const EmbreeBVHInnerNode *>(n);

		if (node) {
			// It is an inner node

			BVHTreeNode *bvhLeftChild = TransformEmbreeBVH(node->children[0], list);
			BVHTreeNode *bvhRightChild = TransformEmbreeBVH(node->children[1], list);

			if (bvhLeftChild) {
				BVHTreeNode *bvhNode = new BVHTreeNode();
				bvhNode->leftChild = bvhLeftChild;

				bvhLeftChild->bbox = node->bbox[0];

				if (bvhRightChild) {
					bvhNode->bbox = Union(node->bbox[0], node->bbox[1]);

					bvhLeftChild->rightSibling = bvhRightChild;

					bvhRightChild->bbox = node->bbox[1];
					bvhRightChild->rightSibling = NULL;
				} else {
					// This should never happen
					bvhNode->bbox = node->bbox[0];

					bvhLeftChild->rightSibling = NULL;
				}

				return bvhNode;
			} else {
				if (bvhRightChild) {
					// This should never happen
					BVHTreeNode *bvhNode = new BVHTreeNode();

					bvhNode->leftChild = bvhRightChild;
					bvhNode->bbox = node->bbox[1];

					bvhRightChild->bbox = node->bbox[1];
					bvhRightChild->rightSibling = NULL;

					return bvhNode;
				} else {
					// This should never happen
					return NULL;
				}
			}
		} else {
			// Must be a leaf
			const EmbreeBVHLeafNode *leaf = (const EmbreeBVHLeafNode *)n;

			BVHTreeNode *node = new BVHTreeNode();
			*node = *(list[leaf->index]);
			return node;
		}
	} else
		return NULL;
}

static void *NodeAllocFunc() {
	return new EmbreeBVHInnerNode();
}

static void *LeafAllocFunc(const RTCPrimRef *prim) {
	return new EmbreeBVHLeafNode(prim->primID);
}

static void *NodeChildrenPtrFunc(void *n, const size_t i) {
	EmbreeBVHInnerNode *node = (EmbreeBVHInnerNode *)n;

	return &node->children[i];
}

static void NodeChildrenSetBBoxFunc(void *n, const size_t i, const float lower[3], const float upper[3]) {
	EmbreeBVHInnerNode *node = (EmbreeBVHInnerNode *)n;

	node->bbox[i].pMin.x = lower[0];
	node->bbox[i].pMin.y = lower[1];
	node->bbox[i].pMin.z = lower[2];

	node->bbox[i].pMax.x = upper[0];
	node->bbox[i].pMax.y = upper[1];
	node->bbox[i].pMax.z = upper[2];
}

BVHTreeNode *BuildEmbreeBVH(const BVHParams &params, vector<BVHTreeNode *> &list) {
	RTCDevice embreeDevice = rtcNewDevice(NULL);

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
		prim.geomID = 0;

		prim.upper_x = node->bbox.pMax.x;
		prim.upper_y = node->bbox.pMax.y;
		prim.upper_z = node->bbox.pMax.z;
		prim.primID = i;
	}

	EmbreeBVHNode *root = (EmbreeBVHNode *)rtcBVHBuilderBinnedSAH(&prims[0], prims.size(),
			&NodeAllocFunc, &LeafAllocFunc, &NodeChildrenPtrFunc, &NodeChildrenSetBBoxFunc);

	// rtcBVHBuilderBinnedSAH() builds a BVH2, I have to transform the
	// tree in BVH N format
	BVHTreeNode *bvhTree = TransformEmbreeBVH(root, list);

	// Free Embree BVH tree
	FreeEmbreeBVHTree(root);

	rtcDeleteDevice(embreeDevice);

	return bvhTree;
}

}
