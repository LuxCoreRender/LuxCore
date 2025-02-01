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

#ifndef __SLG_INDEXKDTREE_H
#define	__SLG_INDEXKDTREE_H

#include <vector>

#include "luxrays/utils/serializationutils.h"

#include "slg/slg.h"

namespace slg {

//------------------------------------------------------------------------------
// Index Kd-Tree
//------------------------------------------------------------------------------

#define KdTreeNodeData_GetAxis(nodeData) (((nodeData) & 0xc0000000u) >> 30)
#define KdTreeNodeData_SetAxis(nodeData, ax) nodeData = ((nodeData) & 0x3fffffffu) | ((ax) << 30)
#define KdTreeNodeData_IsLeaf(nodeData) (KdTreeNodeData_GetAxis(nodeData) == 3)
#define KdTreeNodeData_SetLeaf(nodeData) KdTreeNodeData_SetAxis(nodeData, 3)
#define KdTreeNodeData_HasLeftChild(nodeData) ((nodeData) & 0x20000000u)
#define KdTreeNodeData_SetHasLeftChild(nodeData, v) nodeData = ((nodeData) & 0xdfffffffu) | ((v) << 29)
#define KdTreeNodeData_GetRightChild(nodeData) ((nodeData) & 0x1fffffffu)
#define KdTreeNodeData_SetRightChild(nodeData, index) nodeData = ((nodeData) & 0xe0000000u) | ((index) & 0x1fffffffu)
#define KdTreeNodeData_NULL_INDEX 0x1fffffffu

typedef struct IndexKdTreeArrayNode_t {
	float splitPos;
	u_int index;

	// Most significant 30 and 31 bits are used to encode the splitting axis and
	// if it is a leaf
	// 29 bit => if it has a left child
	// [0, 28] bits => the index of right child
	unsigned int nodeData;
	
	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & splitPos;
		ar & index;
		ar & nodeData;
	}
} IndexKdTreeArrayNode;

template <class T>
class IndexKdTree {
public:
	IndexKdTree(const std::vector<T> *entries);
	virtual ~IndexKdTree();

	size_t GetMemoryUsage() const { return allEntries->size() * sizeof(IndexKdTreeArrayNode); }

	friend class boost::serialization::access;

protected:
	// Used by serialization
	IndexKdTree();

	void Build(const u_int nodeIndex, const u_int start, const u_int end, u_int *buildNodes);

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & allEntries;

		ar & boost::serialization::make_array<IndexKdTreeArrayNode>(arrayNodes, allEntries->size());
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & allEntries;

		arrayNodes = new IndexKdTreeArrayNode[allEntries->size()];
		ar & boost::serialization::make_array<IndexKdTreeArrayNode>(arrayNodes, allEntries->size());
	}
	
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	const std::vector<T> *allEntries;
	IndexKdTreeArrayNode *arrayNodes;

	u_int nextFreeNode;
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::IndexKdTree)

BOOST_CLASS_VERSION(slg::IndexKdTreeArrayNode, 1)
		
BOOST_CLASS_EXPORT_KEY(slg::IndexKdTreeArrayNode)

#endif	/* __SLG_INDEXKDTREE_H */
