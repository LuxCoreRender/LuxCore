/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include <boost/serialization/version.hpp>
#include <vector>

namespace boost { namespace serialization {
class access;
} }

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

struct IndexKdTreeArrayNode {
	float splitPos;
	size_t index;

	// Most significant 30 and 31 bits are used to encode the splitting axis and
	// if it is a leaf
	// 29 bit => if it has a left child
	// [0, 28] bits => the index of right child
	unsigned int nodeData;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const unsigned int version) {
		ar & splitPos;
		ar & index;
		ar & nodeData;
	}
};

template <class T>
class IndexKdTree {
public:
	IndexKdTree(const std::vector<T>& entries);
	virtual ~IndexKdTree() = default;

	size_t GetMemoryUsage() const;

	const std::vector<T> & GetAllEntries() const;


protected:

	void Build(
		const size_t nodeIndex,
		const size_t start,
		const size_t end,
		std::vector<size_t>& buildNodes
	);


	const std::vector<T> & allEntries;
	std::vector<IndexKdTreeArrayNode> arrayNodes;

	size_t nextFreeNode;

	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive &ar, const unsigned int file_version);

};

}  // Namespace slg




BOOST_CLASS_VERSION(slg::IndexKdTreeArrayNode, 1)


#endif	/* __SLG_INDEXKDTREE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
