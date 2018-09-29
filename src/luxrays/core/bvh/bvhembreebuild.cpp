/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <vector>
#include <boost/foreach.hpp>
#include <boost/thread/mutex.hpp>

#include <embree3/rtcore.h>
#include <embree3/rtcore_builder.h>

#include "luxrays/core/bvh/bvhbuild.h"
#include "luxrays/utils/atomic.h"

using namespace std;

namespace luxrays {

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

class EmbreeBuilderGlobalData {
public:
	EmbreeBuilderGlobalData();
	~EmbreeBuilderGlobalData();

	RTCDevice embreeDevice;
	RTCBVH embreeBVH;

	u_int nodeCounter;
};

// EmbreeBuilderGlobalData
EmbreeBuilderGlobalData::EmbreeBuilderGlobalData() {
	embreeDevice = rtcNewDevice(NULL);
	embreeBVH = rtcNewBVH(embreeDevice);

	nodeCounter = 0;
}

EmbreeBuilderGlobalData::~EmbreeBuilderGlobalData() {
	rtcReleaseBVH(embreeBVH);
	rtcReleaseDevice(embreeDevice);
}

//------------------------------------------------------------------------------
// BuildEmbreeBVHArray
//------------------------------------------------------------------------------

static inline void CopyBBox(const float *src, float *dst) {
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;

	*dst++ = *src++;
	*dst++ = *src++;
	*dst = *src;
}

template<u_int CHILDREN_COUNT> static u_int BuildEmbreeBVHArray(const deque<const Mesh *> *meshes,
		const EmbreeBVHNode<CHILDREN_COUNT> *node, vector<BVHTreeNode *> &leafList,
		u_int offset, luxrays::ocl::BVHArrayNode *bvhArrayTree) {
	if (node) {
		luxrays::ocl::BVHArrayNode *arrayNode = &bvhArrayTree[offset];

		const EmbreeBVHInnerNode<CHILDREN_COUNT> *innerNode = dynamic_cast<const EmbreeBVHInnerNode<CHILDREN_COUNT> *>(node);

		if (innerNode) {
			// It is an inner node

			++offset;

			BBox bbox;
			for (u_int i = 0; i < CHILDREN_COUNT; ++i) {
				if (innerNode->children[i]) {
					// Add the child tree to the array
					const u_int childIndex = offset;
					offset = BuildEmbreeBVHArray<CHILDREN_COUNT>(meshes, innerNode->children[i], leafList, childIndex, bvhArrayTree);
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
			const BVHTreeNode *leafTree = leafList[leaf->index];

			if (meshes) {
				// It is a BVH of triangles
				const Triangle *triangles = (*meshes)[leafTree->triangleLeaf.meshIndex]->GetTriangles();
				const Triangle *triangle = &triangles[leafTree->triangleLeaf.triangleIndex];
				arrayNode->triangleLeaf.v[0] = triangle->v[0];
				arrayNode->triangleLeaf.v[1] = triangle->v[1];
				arrayNode->triangleLeaf.v[2] = triangle->v[2];
				arrayNode->triangleLeaf.meshIndex = leafTree->triangleLeaf.meshIndex;
				arrayNode->triangleLeaf.triangleIndex = leafTree->triangleLeaf.triangleIndex;
			} else {
				// It is a BVH of BVHs (i.e. MBVH)
				arrayNode->bvhLeaf.leafIndex = leafTree->bvhLeaf.leafIndex;
				arrayNode->bvhLeaf.transformIndex = leafTree->bvhLeaf.transformIndex;
				arrayNode->bvhLeaf.motionIndex = leafTree->bvhLeaf.motionIndex;
				arrayNode->bvhLeaf.meshOffsetIndex = leafTree->bvhLeaf.meshOffsetIndex;
			}

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

template<u_int CHILDREN_COUNT> static void *CreateNodeFunc(RTCThreadLocalAllocator allocator,
		unsigned int numChildren, void *userPtr) {
	assert (numChildren <= CHILDREN_COUNT);

	EmbreeBuilderGlobalData *gd = (EmbreeBuilderGlobalData *)userPtr;
	AtomicInc(&gd->nodeCounter);

	return new (rtcThreadLocalAlloc(allocator, sizeof(EmbreeBVHInnerNode<CHILDREN_COUNT>), 16)) EmbreeBVHInnerNode<CHILDREN_COUNT>();
}

template<u_int CHILDREN_COUNT> static void *CreateLeafFunc(RTCThreadLocalAllocator allocator,
		const RTCBuildPrimitive *prims, size_t numPrims, void *userPtr) {
	// RTCBuildSettings::maxLeafSize is set to 1 
	assert (numPrims == 1);

	EmbreeBuilderGlobalData *gd = (EmbreeBuilderGlobalData *)userPtr;
	AtomicInc(&gd->nodeCounter);

	return new (rtcThreadLocalAlloc(allocator, sizeof(EmbreeBVHLeafNode<CHILDREN_COUNT>), 16)) EmbreeBVHLeafNode<CHILDREN_COUNT>(prims[0].primID);
}

template<u_int CHILDREN_COUNT> static void NodeSetChildrensPtrFunc(void *nodePtr, void **children, unsigned int numChildren, void *userPtr) {
	assert (numChildren <= CHILDREN_COUNT);

	EmbreeBVHInnerNode<CHILDREN_COUNT> *node = (EmbreeBVHInnerNode<CHILDREN_COUNT> *)nodePtr;

	for (u_int i = 0; i < numChildren; ++i)
		node->children[i] = (EmbreeBVHNode<CHILDREN_COUNT> *)children[i];
}

template<u_int CHILDREN_COUNT> static void NodeSetChildrensBBoxFunc(void *nodePtr,
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

template<u_int CHILDREN_COUNT> static luxrays::ocl::BVHArrayNode *BuildEmbreeBVH(
		RTCBuildQuality quality, u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList) {
	//const double t1 = WallClockTime();

	// Initialize RTCPrimRef vector
	vector<RTCBuildPrimitive> prims(leafList.size());
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < prims.size(); ++i) {
		RTCBuildPrimitive &prim = prims[i];
		const BVHTreeNode *node = leafList[i];

		prim.lower_x = node->bbox.pMin.x;
		prim.lower_y = node->bbox.pMin.y;
		prim.lower_z = node->bbox.pMin.z;
		prim.geomID = 0;

		prim.upper_x = node->bbox.pMax.x;
		prim.upper_y = node->bbox.pMax.y;
		prim.upper_z = node->bbox.pMax.z;
		prim.primID = i;
	}

	//const double t2 = WallClockTime();
	//cout << "BuildEmbreeBVH preprocessing time: " << int((t2 - t1) * 1000) << "ms\n";

	RTCBuildArguments buildArgs = rtcDefaultBuildArguments();
	buildArgs.buildQuality = quality;
	buildArgs.maxBranchingFactor = CHILDREN_COUNT;
	buildArgs.maxLeafSize = 1;
	
	EmbreeBuilderGlobalData *globalData = new EmbreeBuilderGlobalData();
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

	luxrays::ocl::BVHArrayNode *bvhArrayTree = new luxrays::ocl::BVHArrayNode[*nNodes];
	bvhArrayTree[0].nodeData = BuildEmbreeBVHArray<CHILDREN_COUNT>(meshes, root, leafList, 0, bvhArrayTree);
	// If root was a leaf, mark the node
	if (dynamic_cast<const EmbreeBVHLeafNode<CHILDREN_COUNT> *>(root))
		bvhArrayTree[0].nodeData |= 0x80000000u;

	//const double t4 = WallClockTime();
	//cout << "BuildEmbreeBVH BuildEmbreeBVHArray time: " << int((t4 - t3) * 1000) << "ms\n";

	delete globalData;

	return bvhArrayTree;
}

//------------------------------------------------------------------------------
// BuildEmbreeBVHBinnedSAH
//------------------------------------------------------------------------------

luxrays::ocl::BVHArrayNode *BuildEmbreeBVHBinnedSAH(const BVHParams &params,
		u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList) {
	// Performance analysis.
	//
	// Version #1:
	//  BuildEmbreeBVH rtcNewDevice time: 0ms
	//  BuildEmbreeBVH preprocessing time: 266ms
	//  BuildEmbreeBVH rtcBVHBuilderBinnedSAH time: 10060ms
	//  BuildEmbreeBVH TransformEmbreeBVH time: 4061ms
	//  BuildEmbreeBVH FreeEmbreeBVHTree time: 1739ms
	//  BuildEmbreeBVH rtcDeleteDevice time: 0ms
	//  [LuxRays][20.330] BVH build hierarchy time: 16823ms
	// Version #2 (using rtcNewAllocator()):
	//  BuildEmbreeBVH rtcNewDevice time: 0ms
	//  BuildEmbreeBVH preprocessing time: 217ms
	//  BuildEmbreeBVH rtcBVHBuilderBinnedSAH time: 2761ms
	//  BuildEmbreeBVH TransformEmbreeBVH time: 3254ms
	//  BuildEmbreeBVH rtcDeleteAllocator time: 4ms
	//  BuildEmbreeBVH rtcDeleteDevice time: 0ms
	//  [LuxRays][9.712] BVH build hierarchy time: 6752ms
	// Version #3 (using BuildEmbreeBVHArray()):
	//  BuildEmbreeBVH rtcNewDevice time: 0ms
	//  BuildEmbreeBVH preprocessing time: 219ms
	//  BuildEmbreeBVH rtcBVHBuilderBinnedSAH time: 2741ms
	//  BuildEmbreeBVH CountEmbreeBVHNodes time: 1017ms
	//  BuildEmbreeBVH BuildEmbreeBVHArray time: 2111ms
	//  BuildEmbreeBVH rtcDeleteAllocator time: 5ms
	//  BuildEmbreeBVH rtcDeleteDevice time: 0ms
	//  [LuxRays][9.025] BVH build hierarchy time: 6096ms
	// Version #4 (removed the need of CountEmbreeBVHNodes()):
	//  BuildEmbreeBVH rtcNewDevice time: 0ms
	//  BuildEmbreeBVH preprocessing time: 214ms
	//  BuildEmbreeBVH rtcBVHBuilderBinnedSAH time: 2828ms
	//  BuildEmbreeBVH BuildEmbreeBVHArray time: 2134ms
	//  BuildEmbreeBVH rtcDeleteDevice time: 5ms
	//  [LuxRays][8.229] BVH build hierarchy time: 5183ms

	luxrays::ocl::BVHArrayNode *bvhArrayTree;

	if (params.treeType == 2)
		bvhArrayTree = BuildEmbreeBVH<2>(RTC_BUILD_QUALITY_HIGH, nNodes, meshes, leafList);
	else if (params.treeType == 4)
		bvhArrayTree = BuildEmbreeBVH<4>(RTC_BUILD_QUALITY_HIGH, nNodes, meshes, leafList);
	else if (params.treeType == 8)
		bvhArrayTree = BuildEmbreeBVH<8>(RTC_BUILD_QUALITY_HIGH, nNodes, meshes, leafList);
	else
		throw runtime_error("Unsupported tree type in BuildEmbreeBVHBinnedSAH(): " + ToString(params.treeType));

	return bvhArrayTree;
}

//------------------------------------------------------------------------------
// BuildEmbreeBVHMorton
//------------------------------------------------------------------------------

luxrays::ocl::BVHArrayNode *BuildEmbreeBVHMorton(const BVHParams &params,
		u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList) {
	luxrays::ocl::BVHArrayNode *bvhArrayTree;

	if (params.treeType == 2)
		bvhArrayTree = BuildEmbreeBVH<2>(RTC_BUILD_QUALITY_LOW, nNodes, meshes, leafList);
	else if (params.treeType == 4)
		bvhArrayTree = BuildEmbreeBVH<4>(RTC_BUILD_QUALITY_LOW, nNodes, meshes, leafList);
	else if (params.treeType == 8)
		bvhArrayTree = BuildEmbreeBVH<8>(RTC_BUILD_QUALITY_LOW, nNodes, meshes, leafList);
	else
		throw runtime_error("Unsupported tree type in BuildEmbreeBVHMorton(): " + ToString(params.treeType));

	return bvhArrayTree;
}

}
