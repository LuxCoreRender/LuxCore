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

template<u_int CHILDREN_COUNT> static void *CreateLocalThreadDataFunc(void *userGlobalData) {
	EmbreeBuilderGlobalData *gd = (EmbreeBuilderGlobalData *)userGlobalData;
	return gd->GetLocalData();
}

template<u_int CHILDREN_COUNT> static void *CreateNodeFunc(void *userLocalThreadData) {
	EmbreeBuilderLocalData *ld = (EmbreeBuilderLocalData *)userLocalThreadData;
	ld->nodeCounter += 1;
	return new (ld->AllocMemory(sizeof(EmbreeBVHInnerNode<CHILDREN_COUNT>))) EmbreeBVHInnerNode<CHILDREN_COUNT>();
}

template<u_int CHILDREN_COUNT> static void *CreateLeafFunc(void *userLocalThreadData, const RTCPrimRef *prim) {
	EmbreeBuilderLocalData *ld = (EmbreeBuilderLocalData *)userLocalThreadData;
	ld->nodeCounter += 1;
	return new (ld->AllocMemory(sizeof(EmbreeBVHLeafNode<CHILDREN_COUNT>))) EmbreeBVHLeafNode<CHILDREN_COUNT>(prim->primID);
}

template<u_int CHILDREN_COUNT> static void *NodeChildrenPtrFunc(void *n, const size_t i) {
	EmbreeBVHInnerNode<CHILDREN_COUNT> *node = (EmbreeBVHInnerNode<CHILDREN_COUNT> *)n;

	return &node->children[i];
}

template<u_int CHILDREN_COUNT> static void NodeChildrenSetBBoxFunc(void *n, const size_t i, const float lower[3], const float upper[3]) {
	EmbreeBVHInnerNode<CHILDREN_COUNT> *node = (EmbreeBVHInnerNode<CHILDREN_COUNT> *)n;

	node->bbox[i].pMin.x = lower[0];
	node->bbox[i].pMin.y = lower[1];
	node->bbox[i].pMin.z = lower[2];

	node->bbox[i].pMax.x = upper[0];
	node->bbox[i].pMax.y = upper[1];
	node->bbox[i].pMax.z = upper[2];
}

template<u_int CHILDREN_COUNT> static luxrays::ocl::BVHArrayNode *BuildEmbreeBVH(
		u_int *nNodes, const std::deque<const Mesh *> *meshes,
		std::vector<BVHTreeNode *> &leafList) {
	//const double t1 = WallClockTime();

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

	//const double t2 = WallClockTime();
	//cout << "BuildEmbreeBVH preprocessing time: " << int((t2 - t1) * 1000) << "ms\n";

	RTCBVHBuilderConfig config;
	rtcDefaultBVHBuilderConfig(&config);

	EmbreeBuilderGlobalData *globalData = new EmbreeBuilderGlobalData();
	EmbreeBVHNode<CHILDREN_COUNT> *root = (EmbreeBVHNode<CHILDREN_COUNT> *)rtcBVHBuilderBinnedSAH(&config,
			&prims[0], prims.size(),
			globalData,
			&CreateLocalThreadDataFunc<CHILDREN_COUNT>, &CreateNodeFunc<CHILDREN_COUNT>, &CreateLeafFunc<CHILDREN_COUNT>,
			&NodeChildrenPtrFunc<CHILDREN_COUNT>, &NodeChildrenSetBBoxFunc<CHILDREN_COUNT>);

	*nNodes = globalData->GetNodeCount();

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

	RTCDevice embreeDevice = rtcNewDevice(NULL);

	luxrays::ocl::BVHArrayNode *bvhArrayTree;

	if (params.treeType == 2)
		bvhArrayTree = BuildEmbreeBVH<2>(nNodes, meshes, leafList);
	else if (params.treeType == 4)
		bvhArrayTree = BuildEmbreeBVH<4>(nNodes, meshes, leafList);
	else if (params.treeType == 8)
		bvhArrayTree = BuildEmbreeBVH<8>(nNodes, meshes, leafList);
	else
		throw runtime_error("Unsupported tree type in BuildEmbreeBVH(): " + ToString(params.treeType));

	rtcDeleteDevice(embreeDevice);

	return bvhArrayTree;
}

}
