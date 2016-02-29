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
#include <boost/thread/mutex.hpp>

#include <embree2/rtcore.h>
#include <embree2/rtcore_bvh_builder.h>

#include "luxrays/core/bvh/bvhbuild.h"

using namespace std;

namespace luxrays {

//------------------------------------------------------------------------------
// Embree BVH node tree classes
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// Embree builder data
//------------------------------------------------------------------------------

class EmbreeBuilderLocalData;

class EmbreeBuilderGlobalData {
public:
	EmbreeBuilderGlobalData();
	~EmbreeBuilderGlobalData();

	EmbreeBuilderLocalData *GetLocalData();
	u_int GetNodeCount() const;

	RTCAllocator fastAllocator;

	boost::mutex mutex;
	vector<EmbreeBuilderLocalData *> localData;
};

class EmbreeBuilderLocalData {
public:
	EmbreeBuilderLocalData(EmbreeBuilderGlobalData *data);
	~EmbreeBuilderLocalData();

	void *AllocMemory(const size_t size);

	EmbreeBuilderGlobalData *globalData;
	RTCThreadLocalAllocator fastLocalAllocator;

	u_int nodeCounter;
};

// EmbreeBuilderGlobalData
EmbreeBuilderGlobalData::EmbreeBuilderGlobalData() {
	fastAllocator = rtcNewAllocator();
}

EmbreeBuilderGlobalData::~EmbreeBuilderGlobalData() {
	rtcDeleteAllocator(fastAllocator);

	BOOST_FOREACH(EmbreeBuilderLocalData *ld, localData)
		delete ld;
}

EmbreeBuilderLocalData *EmbreeBuilderGlobalData::GetLocalData() {
	boost::unique_lock<boost::mutex> lock(mutex);

	EmbreeBuilderLocalData *ld = new EmbreeBuilderLocalData(this);
	localData.push_back(ld);

	return ld;
}

u_int EmbreeBuilderGlobalData::GetNodeCount() const {
	u_int count = 0;
	BOOST_FOREACH(EmbreeBuilderLocalData *ld, localData)
		count += ld->nodeCounter;

	return count;
}

// EmbreeBuilderLocalData
EmbreeBuilderLocalData::EmbreeBuilderLocalData(EmbreeBuilderGlobalData *data) {
	globalData = data;
	fastLocalAllocator = rtcNewThreadAllocator(globalData->fastAllocator);
	nodeCounter = 0;
}

EmbreeBuilderLocalData::~EmbreeBuilderLocalData() {
}

void *EmbreeBuilderLocalData::AllocMemory(const size_t size) {
	return rtcThreadAlloc(fastLocalAllocator, size);
}

//------------------------------------------------------------------------------
// BuildEmbreeBVHArray
//------------------------------------------------------------------------------

static u_int BuildEmbreeBVHArray(const deque<const Mesh *> *meshes,
		const EmbreeBVHNode *node, vector<BVHTreeNode *> &leafList,
		u_int offset, luxrays::ocl::BVHArrayNode *bvhArrayTree) {
	if (node) {
		luxrays::ocl::BVHArrayNode *arrayNode = &bvhArrayTree[offset];

		const EmbreeBVHInnerNode *innerNode = dynamic_cast<const EmbreeBVHInnerNode *>(node);

		if (innerNode) {
			// It is an inner node

			++offset;

			// Add the left child tree to the array
			const u_int leftChildIndex = offset;
			offset = BuildEmbreeBVHArray(meshes, innerNode->children[0], leafList, leftChildIndex, bvhArrayTree);
			if (dynamic_cast<const EmbreeBVHInnerNode *>(innerNode->children[0])) {
				// If the left child was an inner node, set the skip index
				bvhArrayTree[leftChildIndex].nodeData = offset;
			}

			// Add the right child tree to the array
			const u_int rightChildIndex = offset;
			offset = BuildEmbreeBVHArray(meshes, innerNode->children[1], leafList, rightChildIndex, bvhArrayTree);
			if (dynamic_cast<const EmbreeBVHInnerNode *>(innerNode->children[1])) {
				// If the right child was an inner node, set the skip index
				bvhArrayTree[rightChildIndex].nodeData = offset;
			}

			// Set the current node bounding box
			if (innerNode->children[0]) {
				if (innerNode->children[1]) {
					const BBox bbox = Union(innerNode->bbox[0], innerNode->bbox[1]);
					memcpy(&arrayNode->bvhNode.bboxMin[0], &bbox, sizeof(float) * 6);
				} else
					memcpy(&arrayNode->bvhNode.bboxMin[0], &innerNode->bbox[0], sizeof(float) * 6);
			} else {
				if (innerNode->children[1]) {
					// This should never happen
					memcpy(&arrayNode->bvhNode.bboxMin[0], &innerNode->bbox[1], sizeof(float) * 6);
				} else {
					// This should never happen
					const BBox bbox;
					memcpy(&arrayNode->bvhNode.bboxMin[0], &bbox, sizeof(float) * 6);
				}
			}
		} else {
			// Must be a leaf
			const EmbreeBVHLeafNode *leaf = (const EmbreeBVHLeafNode *)node;
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

static void *CreateLocalThreadDataFunc(void *userGlobalData) {
	EmbreeBuilderGlobalData *gd = (EmbreeBuilderGlobalData *)userGlobalData;
	return gd->GetLocalData();
}

static void *CreateNodeFunc(void *userLocalThreadData) {
	EmbreeBuilderLocalData *ld = (EmbreeBuilderLocalData *)userLocalThreadData;
	ld->nodeCounter += 1;
	return new (ld->AllocMemory(sizeof(EmbreeBVHInnerNode))) EmbreeBVHInnerNode();
}

static void *CreateLeafFunc(void *userLocalThreadData, const RTCPrimRef *prim) {
	EmbreeBuilderLocalData *ld = (EmbreeBuilderLocalData *)userLocalThreadData;
	ld->nodeCounter += 1;
	return new (ld->AllocMemory(sizeof(EmbreeBVHLeafNode))) EmbreeBVHLeafNode(prim->primID);
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

luxrays::ocl::BVHArrayNode *BuildEmbreeBVH(const BVHParams &params,
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

	const double t0 = WallClockTime();

	RTCDevice embreeDevice = rtcNewDevice(NULL);

	const double t1 = WallClockTime();
	cout << "BuildEmbreeBVH rtcNewDevice time: " << int((t1 - t0) * 1000) << "ms\n";

	// Initialize RTCPrimRef vector
	vector<RTCPrimRef> prims(leafList.size());
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < prims.size(); ++i) {
		RTCPrimRef &prim = prims[i];
		BVHTreeNode *node = leafList[i];

		prim.lower_x = node->bbox.pMin.x;
		prim.lower_y = node->bbox.pMin.y;
		prim.lower_z = node->bbox.pMin.z;
		prim.geomID = 0;

		prim.upper_x = node->bbox.pMax.x;
		prim.upper_y = node->bbox.pMax.y;
		prim.upper_z = node->bbox.pMax.z;
		prim.primID = i;
	}

	const double t2 = WallClockTime();
	cout << "BuildEmbreeBVH preprocessing time: " << int((t2 - t1) * 1000) << "ms\n";

	EmbreeBuilderGlobalData *globalData = new EmbreeBuilderGlobalData();
	EmbreeBVHNode *root = (EmbreeBVHNode *)rtcBVHBuilderBinnedSAH(&prims[0], prims.size(),
			globalData,
			&CreateLocalThreadDataFunc, &CreateNodeFunc, &CreateLeafFunc,
			&NodeChildrenPtrFunc, &NodeChildrenSetBBoxFunc);

	*nNodes = globalData->GetNodeCount();

	const double t3 = WallClockTime();
	cout << "BuildEmbreeBVH rtcBVHBuilderBinnedSAH time: " << int((t3 - t2) * 1000) << "ms\n";

	luxrays::ocl::BVHArrayNode *bvhArrayTree = new luxrays::ocl::BVHArrayNode[*nNodes];
	bvhArrayTree[0].nodeData = BuildEmbreeBVHArray(meshes, root, leafList, 0, bvhArrayTree);

	const double t4 = WallClockTime();
	cout << "BuildEmbreeBVH BuildEmbreeBVHArray time: " << int((t4 - t3) * 1000) << "ms\n";

	delete globalData;
	rtcDeleteDevice(embreeDevice);

	const double t5 = WallClockTime();
	cout << "BuildEmbreeBVH rtcDeleteDevice time: " << int((t5 - t4) * 1000) << "ms\n";

	return bvhArrayTree;
}

}
