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

#include <algorithm>

#include "luxrays/core/geometry/bbox.h"
#include "slg/core/indexkdtree.h"

// Required for explicit instantiations
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// IndexKdTree
//------------------------------------------------------------------------------

template <class T>
IndexKdTree<T>::IndexKdTree() : arrayNodes(nullptr) {
}

template <class T>
IndexKdTree<T>::IndexKdTree(const vector<T> *entries) : allEntries(entries),
		arrayNodes(nullptr) {
	assert (allEntries->size() > 0);

	arrayNodes = new IndexKdTreeArrayNode[allEntries->size()];

	vector<u_int> buildNodes(allEntries->size());
	for (u_int i = 0; i < allEntries->size(); ++i)
		buildNodes[i] = i;

	// Build the KdTree
	nextFreeNode = 1;
	Build(0, 0, allEntries->size(), &buildNodes[0]);
}

template <class T>
IndexKdTree<T>::~IndexKdTree() {
	delete arrayNodes;
}

template <class T>
struct CompareNode {
	CompareNode(const vector<T> *entries, u_int a) : allEntries(entries),
		axis(a) {
	}

	const std::vector<T> *allEntries;
	u_int axis;

	bool operator()(const u_int i1, const u_int i2) const {
		return ((*allEntries)[i1].p[axis] == (*allEntries)[i2].p[axis]) ?
			(i1 < i2) :	((*allEntries)[i1].p[axis] < (*allEntries)[i2].p[axis]);
	}
};

template <class T> void
IndexKdTree<T>::Build(const u_int nodeIndex, const u_int start, const u_int end,
		u_int *buildNodes) {
	// Check if we are done
	if (start + 1 == end) {
		arrayNodes[nodeIndex].index = buildNodes[start];
		KdTreeNodeData_SetLeaf(arrayNodes[nodeIndex].nodeData);
		
		assert (KdTreeNodeData_IsLeaf(arrayNodes[nodeIndex].nodeData));
	} else {
		// Compute the bounding box of all nodes
		BBox bb;
		for (u_int i = start; i < end; ++i)
			bb = Union(bb, (*allEntries)[buildNodes[i]].p);

		// Compute the split axis and position
		const u_int splitAxis = bb.MaximumExtent();
		const u_int splitPos = (start + end) / 2;

		// Sort the nodes around the split plane
		nth_element(&buildNodes[start], &buildNodes[splitPos],
				&buildNodes[end], CompareNode<T>(allEntries, splitAxis));

		// Set up the KdTree node
		KdTreeNodeData_SetAxis(arrayNodes[nodeIndex].nodeData, splitAxis);
		arrayNodes[nodeIndex].splitPos = (*allEntries)[buildNodes[splitPos]].p[splitAxis];
		arrayNodes[nodeIndex].index = buildNodes[splitPos];

		if (start < splitPos) {
			KdTreeNodeData_SetHasLeftChild(arrayNodes[nodeIndex].nodeData, 1);
			
			assert (KdTreeNodeData_HasLeftChild(arrayNodes[nodeIndex].nodeData));

			const u_int leftChildIndex = nextFreeNode++;
			Build(leftChildIndex, start, splitPos, buildNodes);
		} else {
			KdTreeNodeData_SetHasLeftChild(arrayNodes[nodeIndex].nodeData, 0);

			assert (!KdTreeNodeData_HasLeftChild(arrayNodes[nodeIndex].nodeData));
		}

		if (splitPos + 1 < end) {
			const u_int rightChildIndex = nextFreeNode++;

			KdTreeNodeData_SetRightChild(arrayNodes[nodeIndex].nodeData, rightChildIndex);
			assert (KdTreeNodeData_GetRightChild(arrayNodes[nodeIndex].nodeData) == rightChildIndex);

			Build(rightChildIndex, splitPos + 1, end, buildNodes);
		} else {
			KdTreeNodeData_SetRightChild(arrayNodes[nodeIndex].nodeData, KdTreeNodeData_NULL_INDEX);
			
			assert (KdTreeNodeData_GetRightChild(arrayNodes[nodeIndex].nodeData) == KdTreeNodeData_NULL_INDEX);
		}
	}
}

//------------------------------------------------------------------------------
// Explicit instantiations
//------------------------------------------------------------------------------

// C++ can be quite horrible...

namespace slg {
template class IndexKdTree<VisibilityParticle>;
}

BOOST_CLASS_EXPORT_IMPLEMENT(slg::IndexKdTree<VisibilityParticle>)
