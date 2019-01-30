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

#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PGICPhotonKdTree
//------------------------------------------------------------------------------

PGICPhotonKdTree::PGICPhotonKdTree(const vector<Photon> &entries, const u_int maxLookUpCount,
		const float normalAngle) :
		IndexKdTree(entries), entryMaxLookUpCount(maxLookUpCount),
		entryNormalCosAngle(cosf(Radians(normalAngle))) {
}

PGICPhotonKdTree::~PGICPhotonKdTree() {
}

// Iterative implementation
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