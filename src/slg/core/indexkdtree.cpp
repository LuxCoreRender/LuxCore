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

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// IndexKdTree
//------------------------------------------------------------------------------

template <class T>
IndexKdTree<T>::IndexKdTree(const vector<T> &entries) : allEntries(entries),
		arrayNodes(nullptr) {
	assert (allEntries.size() > 0);

	arrayNodes = new IndexKdTreeArrayNode[allEntries.size()];

	vector<u_int> buildNodes(allEntries.size());
	for (u_int i = 0; i < allEntries.size(); ++i)
		buildNodes[i] = i;

	// Build the KdTree
	nextFreeNode = 1;
	Build(0, 0, allEntries.size(), &buildNodes[0]);
}

template <class T>
IndexKdTree<T>::~IndexKdTree() {
	delete arrayNodes;
}

template <class T>
struct CompareNode {
	CompareNode(const vector<T> &entries, u_int a) : allEntries(entries),
		axis(a) {
	}

	const std::vector<T> &allEntries;
	u_int axis;

	bool operator()(const u_int i1, const u_int i2) const {
		return (allEntries[i1].p[axis] == allEntries[i2].p[axis]) ?
			(i1 < i2) :	(allEntries[i1].p[axis] < allEntries[i2].p[axis]);
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
			bb = Union(bb, allEntries[buildNodes[i]].p);

		// Compute the split axis and position
		const u_int splitAxis = bb.MaximumExtent();
		const u_int splitPos = (start + end) / 2;

		// Sort the nodes around the split plane
		nth_element(&buildNodes[start], &buildNodes[splitPos],
				&buildNodes[end], CompareNode<T>(allEntries, splitAxis));

		// Set up the KdTree node
		KdTreeNodeData_SetAxis(arrayNodes[nodeIndex].nodeData, splitAxis);
		arrayNodes[nodeIndex].splitPos = allEntries[buildNodes[splitPos]].p[splitAxis];
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
// GetAllNearEntries() example
//------------------------------------------------------------------------------

// Iterative implementation

/*
void PGICPhotonKdTree::GetAllNearEntries(vector<NearPhoton> &entries,
		const Point &p, const Normal &n, float &maxDistance2) const {
	const int stackSize = 64;

	u_int nodeIndexStack[stackSize];

	int stackCurrentIndex = 0;
	nodeIndexStack[stackCurrentIndex] = 0;

	while (stackCurrentIndex >= 0) {
		// Pop the current node form the stack
		const u_int currentNodeIndex = nodeIndexStack[stackCurrentIndex--];

		const IndexKdTreeArrayNode &node = arrayNodes[currentNodeIndex];
		const Photon &entry = allEntries[node.index];

		const u_int axis = KdTreeNodeData_GetAxis(node.nodeData);

		// Add check of the children if it is not a leaf
		if (axis != 3) {
			const float distance2 = Sqr(p[axis] - node.splitPos);

			if (p[axis] <= node.splitPos) {
				if (KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}

				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if ((distance2 < maxDistance2) && (rightChildIndex != KdTreeNodeData_NULL_INDEX)) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}
			} else {
				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if (rightChildIndex != KdTreeNodeData_NULL_INDEX) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}

				if ((distance2 < maxDistance2) && KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries.size());
				}
			}
		}

		// Check the current node
		const float distance2 = DistanceSquared(entry.p, p);
		if ((distance2 < maxDistance2) &&
					(Dot(n, -entry.d) > DEFAULT_COS_EPSILON_STATIC) &&
					(Dot(n, entry.landingSurfaceNormal) > entryNormalCosAngle)) {
			// I have found a valid entry

			NearPhoton nearPhoton(&entry, distance2);

			if (entries.size() < entryMaxLookUpCount) {
				// Just add the entry
				entries.push_back(nearPhoton);

				// Check if the array is now full and sort the entries for
				// the next addition
				if (entries.size() == entryMaxLookUpCount)
					make_heap(&entries[0], &entries[entryMaxLookUpCount]);
			} else {
				// Check if the new entry is nearer than the farthest array entry
				if (distance2 < entries[0].distance2) {
					// Remove the farthest array entry
					pop_heap(&entries[0], &entries[entryMaxLookUpCount]);
					// Add the new entry
					entries[entryMaxLookUpCount - 1] = nearPhoton;
					push_heap(&entries[0], &entries[entryMaxLookUpCount]);

					// Update max. squared distance
					maxDistance2 = entries[0].distance2;
				}
			}
		}
	}
}
*/

// Recursive implementation

/*
void PGICPhotonKdTree::GetAllNearEntries(vector<NearPhoton> &entries,
		const Point &p, const Normal &n, float &maxDistance2) const {
	GetAllNearEntriesImpl(0, entries, p, n, maxDistance2); 
}

void PGICPhotonKdTree::GetAllNearEntriesImpl(const u_int currentNodeIndex, vector<NearPhoton> &entries,
		const Point &p, const Normal &n, float &maxDistance2) const {
	const IndexKdTreeArrayNode &node = arrayNodes[currentNodeIndex];
	const Photon &entry = allEntries[node.index];

	const u_int axis = KdTreeNodeData_GetAxis(node.nodeData);

	// Add check of the children if it is not a leaf
	if (axis != 3) {
		const float distance2 = Sqr(p[axis] - node.splitPos);

		if (p[axis] <= node.splitPos) {
			if (KdTreeNodeData_HasLeftChild(node.nodeData))
				GetAllNearEntriesImpl(currentNodeIndex + 1, entries, p, n, maxDistance2);

			const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
			if ((distance2 < maxDistance2) && (rightChildIndex != KdTreeNodeData_NULL_INDEX))
				GetAllNearEntriesImpl(rightChildIndex, entries, p, n, maxDistance2);
		} else {
			const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
			if (rightChildIndex != KdTreeNodeData_NULL_INDEX)
				GetAllNearEntriesImpl(rightChildIndex, entries, p, n, maxDistance2);

			if ((distance2 < maxDistance2) && KdTreeNodeData_HasLeftChild(node.nodeData))
				GetAllNearEntriesImpl(currentNodeIndex + 1, entries, p, n, maxDistance2);
		}
	}

	// Check the current node
	const float distance2 = DistanceSquared(entry.p, p);
	if ((distance2 < maxDistance2) &&
				(Dot(n, -entry.d) > DEFAULT_COS_EPSILON_STATIC) &&
				(Dot(n, entry.landingSurfaceNormal) > entryNormalCosAngle)) {
		// I have found a valid entry

		NearPhoton nearPhoton(&entry, distance2);

		if (entries.size() < entryMaxLookUpCount) {
			// Just add the entry
			entries.push_back(nearPhoton);

			// Check if the array is now full and sort the entries for
			// the next addition
			if (entries.size() == entryMaxLookUpCount)
				make_heap(&entries[0], &entries[entryMaxLookUpCount]);
		} else {
			// Check if the new entry is nearer than the farthest array entry
			if (distance2 < entries[0].distance2) {
				// Remove the farthest array entry
				pop_heap(&entries[0], &entries[entryMaxLookUpCount]);
				// Add the new entry
				entries[entryMaxLookUpCount - 1] = nearPhoton;
				push_heap(&entries[0], &entries[entryMaxLookUpCount]);

				// Update max. squared distance
				maxDistance2 = entries[0].distance2;
			}
		}
	}
}
*/
