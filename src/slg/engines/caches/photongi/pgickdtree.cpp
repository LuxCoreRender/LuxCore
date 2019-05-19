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

#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PGCIKdTree
//------------------------------------------------------------------------------

PGICKdTree::PGICKdTree(const vector<PGICVisibilityParticle> *entries) :
		IndexKdTree(entries) {
}

PGICKdTree::~PGICKdTree() {
}

u_int PGICKdTree::GetNearestEntry(
		const Point &p, const Normal &n, const bool isVolume,
		const float radius2, const float normalCosAngle) const {
	const int stackSize = 128;

	u_int nodeIndexStack[stackSize];

	int stackCurrentIndex = 0;
	nodeIndexStack[stackCurrentIndex] = 0;

	u_int nearestEntryIndex = NULL_INDEX;
	float nearestMaxDistance2 = radius2;
	while (stackCurrentIndex >= 0) {
		// Pop the current node form the stack
		const u_int currentNodeIndex = nodeIndexStack[stackCurrentIndex--];

		const IndexKdTreeArrayNode &node = arrayNodes[currentNodeIndex];

		const u_int axis = KdTreeNodeData_GetAxis(node.nodeData);

		// Add check of the children if it is not a leaf
		if (axis != 3) {
			const float distance2 = Sqr(p[axis] - node.splitPos);

			if (p[axis] <= node.splitPos) {
				if (KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}

				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if ((distance2 < nearestMaxDistance2) && (rightChildIndex != KdTreeNodeData_NULL_INDEX)) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}
			} else {
				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if (rightChildIndex != KdTreeNodeData_NULL_INDEX) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}

				if ((distance2 < nearestMaxDistance2) && KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}
			}
		}

		// Check the current node
		const PGICVisibilityParticle &entry = (*allEntries)[node.index];
		const float distance2 = DistanceSquared(entry.p, p);
		if ((distance2 < nearestMaxDistance2) && (entry.isVolume == isVolume) &&
					(isVolume || (Dot(n, entry.n) > normalCosAngle))) {
			// I have found a valid entry

			nearestEntryIndex = node.index;
			nearestMaxDistance2 = distance2;
		}
	}

	return nearestEntryIndex;
}

void PGICKdTree::GetAllNearEntries(vector<u_int> &allNearEntryIndices,
		const Point &p, const Normal &n, const bool isVolume,
		const float radius2, const float normalCosAngle) const {
	const int stackSize = 128;

	u_int nodeIndexStack[stackSize];

	int stackCurrentIndex = 0;
	nodeIndexStack[stackCurrentIndex] = 0;

	while (stackCurrentIndex >= 0) {
		// Pop the current node form the stack
		const u_int currentNodeIndex = nodeIndexStack[stackCurrentIndex--];

		const IndexKdTreeArrayNode &node = arrayNodes[currentNodeIndex];

		const u_int axis = KdTreeNodeData_GetAxis(node.nodeData);

		// Add check of the children if it is not a leaf
		if (axis != 3) {
			const float distance2 = Sqr(p[axis] - node.splitPos);

			if (p[axis] <= node.splitPos) {
				if (KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}

				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if ((distance2 < radius2) && (rightChildIndex != KdTreeNodeData_NULL_INDEX)) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}
			} else {
				const u_int rightChildIndex = KdTreeNodeData_GetRightChild(node.nodeData);
				if (rightChildIndex != KdTreeNodeData_NULL_INDEX) {
					nodeIndexStack[++stackCurrentIndex] = rightChildIndex;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}

				if ((distance2 < radius2) && KdTreeNodeData_HasLeftChild(node.nodeData)) {
					nodeIndexStack[++stackCurrentIndex] = currentNodeIndex + 1;
					
					assert (stackCurrentIndex < stackSize);
					assert (nodeIndexStack[stackCurrentIndex] < allEntries->size());
				}
			}
		}

		// Check the current node
		const PGICVisibilityParticle &entry = (*allEntries)[node.index];
		const float distance2 = DistanceSquared(entry.p, p);
		if ((distance2 < radius2) && (entry.isVolume == isVolume) &&
					(isVolume || (Dot(n, entry.n) > normalCosAngle))) {
			// I have found a valid entry

			allNearEntryIndices.push_back(node.index);
		}
	}
}
