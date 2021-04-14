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

#ifndef _LUXRAYS_BVHBUILD_H
#define	_LUXRAYS_BVHBUILD_H

#include <vector>
#include <ostream>

#include <embree3/rtcore.h>
#include <embree3/rtcore_builder.h>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/bbox.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/utils/atomic.h"

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
extern luxrays::ocl::BVHArrayNode *BuildEmbreeBVHBinnedSAH(const BVHParams &params,
		u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList);
extern luxrays::ocl::BVHArrayNode *BuildEmbreeBVHMorton(const BVHParams &params,
		u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList);

// Common functions
extern void FreeBVH(BVHTreeNode *node);
extern u_int CountBVHNodes(BVHTreeNode *node);
extern void PrintBVHNodes(std::ostream &stream, BVHTreeNode *node);

namespace buildembreebvh {

//------------------------------------------------------------------------------
// Embree BVH node tree classes
//------------------------------------------------------------------------------

template<u_int CHILDREN_COUNT> class EmbreeBVHNode {
public:
	EmbreeBVHNode() { }
	virtual ~EmbreeBVHNode() { }
};

template<u_int CHILDREN_COUNT> class EmbreeBVHInnerNode : public EmbreeBVHNode<CHILDREN_COUNT> {
public:
	EmbreeBVHInnerNode() {
		for (u_int i = 0; i < CHILDREN_COUNT; ++i)
			children[i] = NULL;
	}
	virtual ~EmbreeBVHInnerNode() { }
	
	BBox bbox[CHILDREN_COUNT];
	EmbreeBVHNode<CHILDREN_COUNT> *children[CHILDREN_COUNT];
};

template<u_int CHILDREN_COUNT> class EmbreeBVHLeafNode : public EmbreeBVHNode<CHILDREN_COUNT> {
public:
	EmbreeBVHLeafNode(const size_t i) : index(i) { }
	virtual ~EmbreeBVHLeafNode() { }

	size_t index;
};

//------------------------------------------------------------------------------
// Embree builder data
//------------------------------------------------------------------------------

class IndexEmbreeBuilderGlobalData {
public:
	IndexEmbreeBuilderGlobalData();
	~IndexEmbreeBuilderGlobalData();

	RTCDevice embreeDevice;
	RTCBVH embreeBVH;

	u_int nodeCounter;
};

// IndexEmbreeBuilderGlobalData
inline IndexEmbreeBuilderGlobalData::IndexEmbreeBuilderGlobalData() {
	embreeDevice = rtcNewDevice(NULL);
	embreeBVH = rtcNewBVH(embreeDevice);

	nodeCounter = 0;
}

inline IndexEmbreeBuilderGlobalData::~IndexEmbreeBuilderGlobalData() {
	rtcReleaseBVH(embreeBVH);
	rtcReleaseDevice(embreeDevice);
}

//------------------------------------------------------------------------------
// BuildEmbreeBVHArray
//------------------------------------------------------------------------------

inline void CopyBBox(const float *src, float *dst) {
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;

	*dst++ = *src++;
	*dst++ = *src++;
	*dst = *src;
}

template<u_int CHILDREN_COUNT> inline u_int BuildEmbreeBVHArray(
		const EmbreeBVHNode<CHILDREN_COUNT> *node,
		u_int offset, luxrays::ocl::IndexBVHArrayNode *bvhArrayTree) {
	if (node) {
		luxrays::ocl::IndexBVHArrayNode *arrayNode = &bvhArrayTree[offset];

		const EmbreeBVHInnerNode<CHILDREN_COUNT> *innerNode = dynamic_cast<const EmbreeBVHInnerNode<CHILDREN_COUNT> *>(node);

		if (innerNode) {
			// It is an inner node

			++offset;

			BBox bbox;
			for (u_int i = 0; i < CHILDREN_COUNT; ++i) {
				if (innerNode->children[i]) {
					// Add the child tree to the array
					const u_int childIndex = offset;
					offset = BuildEmbreeBVHArray<CHILDREN_COUNT>(innerNode->children[i], childIndex, bvhArrayTree);
					if (dynamic_cast<const EmbreeBVHInnerNode<CHILDREN_COUNT> *>(innerNode->children[i])) {
						// If the child was an inner node, set the skip index
						bvhArrayTree[childIndex].nodeData = offset;
					}
					
					bbox = Union(bbox, innerNode->bbox[i]);
				}
			}

			CopyBBox(&bbox.pMin.x, &arrayNode->bvhNode.bboxMin[0]);
		} else {
			// Must be a leaf
			const EmbreeBVHLeafNode<CHILDREN_COUNT> *leaf = (const EmbreeBVHLeafNode<CHILDREN_COUNT> *)node;
			arrayNode->entryLeaf.entryIndex = leaf->index;

			++offset;
			// Mark as a leaf
			arrayNode->nodeData = offset | 0x80000000u;
		}
	}

	return offset;
}

//------------------------------------------------------------------------------
// BuildEmbreeBVH
//------------------------------------------------------------------------------

template<u_int CHILDREN_COUNT> inline void *CreateNodeFunc(RTCThreadLocalAllocator allocator,
		unsigned int numChildren, void *userPtr) {
	assert (numChildren <= CHILDREN_COUNT);

	IndexEmbreeBuilderGlobalData *gd = (IndexEmbreeBuilderGlobalData *)userPtr;
	AtomicInc(&gd->nodeCounter);

	return new (rtcThreadLocalAlloc(allocator, sizeof(EmbreeBVHInnerNode<CHILDREN_COUNT>), 16)) EmbreeBVHInnerNode<CHILDREN_COUNT>();
}

template<u_int CHILDREN_COUNT> inline void *CreateLeafFunc(RTCThreadLocalAllocator allocator,
		const RTCBuildPrimitive *prims, size_t numPrims, void *userPtr) {
	// RTCBuildSettings::maxLeafSize is set to 1 
	assert (numPrims == 1);

	IndexEmbreeBuilderGlobalData *gd = (IndexEmbreeBuilderGlobalData *)userPtr;
	AtomicInc(&gd->nodeCounter);

	return new (rtcThreadLocalAlloc(allocator, sizeof(EmbreeBVHLeafNode<CHILDREN_COUNT>), 16)) EmbreeBVHLeafNode<CHILDREN_COUNT>(prims[0].primID);
}

template<u_int CHILDREN_COUNT> inline void NodeSetChildrensPtrFunc(void *nodePtr, void **children, unsigned int numChildren, void *userPtr) {
	assert (numChildren <= CHILDREN_COUNT);

	EmbreeBVHInnerNode<CHILDREN_COUNT> *node = (EmbreeBVHInnerNode<CHILDREN_COUNT> *)nodePtr;

	for (u_int i = 0; i < numChildren; ++i)
		node->children[i] = (EmbreeBVHNode<CHILDREN_COUNT> *)children[i];
}

template<u_int CHILDREN_COUNT> inline void NodeSetChildrensBBoxFunc(void *nodePtr,
		const RTCBounds **bounds, unsigned int numChildren, void *userPtr) {
	EmbreeBVHInnerNode<CHILDREN_COUNT> *node = (EmbreeBVHInnerNode<CHILDREN_COUNT> *)nodePtr;

	for (u_int i = 0; i < numChildren; ++i) {
		node->bbox[i].pMin.x = bounds[i]->lower_x;
		node->bbox[i].pMin.y = bounds[i]->lower_y;
		node->bbox[i].pMin.z = bounds[i]->lower_z;

		node->bbox[i].pMax.x = bounds[i]->upper_x;
		node->bbox[i].pMax.y = bounds[i]->upper_y;
		node->bbox[i].pMax.z = bounds[i]->upper_z;
	}
}

//------------------------------------------------------------------------------
// BuildEmbreeBVH
//------------------------------------------------------------------------------

template<u_int CHILDREN_COUNT> inline luxrays::ocl::IndexBVHArrayNode *BuildEmbreeBVH(
		const RTCBuildQuality quality, std::vector<RTCBuildPrimitive> &prims, u_int *nNodes) {
	RTCBuildArguments buildArgs = rtcDefaultBuildArguments();
	buildArgs.buildQuality = quality;
	buildArgs.maxBranchingFactor = CHILDREN_COUNT;
	buildArgs.maxLeafSize = 1;
	
	IndexEmbreeBuilderGlobalData *globalData = new IndexEmbreeBuilderGlobalData();
	buildArgs.bvh = globalData->embreeBVH;
	buildArgs.primitives = &prims[0];
	buildArgs.primitiveCount = prims.size();
	buildArgs.primitiveArrayCapacity = prims.size();
	buildArgs.createNode = &CreateNodeFunc<CHILDREN_COUNT>;
	buildArgs.setNodeChildren = &NodeSetChildrensPtrFunc<CHILDREN_COUNT>;
	buildArgs.setNodeBounds = &NodeSetChildrensBBoxFunc<CHILDREN_COUNT>;
	buildArgs.createLeaf = &CreateLeafFunc<CHILDREN_COUNT>;
	buildArgs.splitPrimitive = NULL;
	buildArgs.buildProgress = NULL;
	buildArgs.userPtr = globalData;

	EmbreeBVHNode<CHILDREN_COUNT> *root = (EmbreeBVHNode<CHILDREN_COUNT> *)rtcBuildBVH(&buildArgs);

	*nNodes = globalData->nodeCounter;

	//const double t3 = WallClockTime();
	//cout << "BuildEmbreeBVH rtcBVHBuilderBinnedSAH time: " << int((t3 - t2) * 1000) << "ms\n";

	luxrays::ocl::IndexBVHArrayNode *bvhArrayTree = new luxrays::ocl::IndexBVHArrayNode[*nNodes];
	bvhArrayTree[0].nodeData = BuildEmbreeBVHArray<CHILDREN_COUNT>(root, 0, bvhArrayTree);
	// If root was a leaf, mark the node
	if (dynamic_cast<const EmbreeBVHLeafNode<CHILDREN_COUNT> *>(root))
		bvhArrayTree[0].nodeData |= 0x80000000u;

	//const double t4 = WallClockTime();
	//cout << "BuildEmbreeBVH BuildEmbreeBVHArray time: " << int((t4 - t3) * 1000) << "ms\n";

	delete globalData;

	return bvhArrayTree;
}

}

}

#endif	/* _LUXRAYS_BVHACCEL_H */
