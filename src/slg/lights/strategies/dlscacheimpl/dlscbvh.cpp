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


#include <embree3/rtcore.h>
#include <embree3/rtcore_builder.h>

#include "luxrays/core/bvh/bvhbuild.h"
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
#include "slg/lights/strategies/dlscacheimpl/dlscbvh.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// DLSCBvh
//------------------------------------------------------------------------------

DLSCBvh::DLSCBvh(const vector<DLSCacheEntry> *entries, const float radius, const float normalAngle) :
			IndexBvh(entries, radius), normalCosAngle(cosf(Radians(normalAngle))) {
}

DLSCBvh::~DLSCBvh() {
}

const DLSCacheEntry *DLSCBvh::GetNearestEntry(const Point &p, const Normal &n, const bool isVolume) const {
	const DLSCacheEntry *nearestEntry = nullptr;
	float nearestDistance2 = entryRadius2;

	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(arrayNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		const luxrays::ocl::IndexBVHArrayNode &node = arrayNodes[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const DLSCacheEntry *entry = &((*allEntries)[node.entryLeaf.entryIndex]);

			const float distance2 = DistanceSquared(p, entry->p);
			if ((distance2 < nearestDistance2) && (isVolume == entry->isVolume) &&
					(isVolume || (Dot(n, entry->n) > normalCosAngle))) {
				// I have found a valid nearer entry
				nearestEntry = entry;
				nearestDistance2 = distance2;
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (p.x >= node.bvhNode.bboxMin[0] && p.x <= node.bvhNode.bboxMax[0] &&
					p.y >= node.bvhNode.bboxMin[1] && p.y <= node.bvhNode.bboxMax[1] &&
					p.z >= node.bvhNode.bboxMin[2] && p.z <= node.bvhNode.bboxMax[2])
				++currentNode;
			else {
				// I don't need to use IndexBVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}

	return nearestEntry;
}

void DLSCBvh::GetAllNearEntries(vector<u_int> &allNearEntryIndices,
		const Point &p, const Normal &n, const bool isVolume) const {
	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(arrayNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		const luxrays::ocl::IndexBVHArrayNode &node = arrayNodes[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const u_int entryIndex = node.entryLeaf.entryIndex;
			const DLSCacheEntry *entry = &((*allEntries)[entryIndex]);

			const float distance2 = DistanceSquared(p, entry->p);
			if ((distance2 < entryRadius2) && (isVolume == entry->isVolume) &&
					(isVolume || (Dot(n, entry->n) > normalCosAngle))) {
				// I have found a valid nearer entry
				allNearEntryIndices.push_back(entryIndex);
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (p.x >= node.bvhNode.bboxMin[0] && p.x <= node.bvhNode.bboxMax[0] &&
					p.y >= node.bvhNode.bboxMin[1] && p.y <= node.bvhNode.bboxMax[1] &&
					p.z >= node.bvhNode.bboxMin[2] && p.z <= node.bvhNode.bboxMax[2])
				++currentNode;
			else {
				// I don't need to use IndexBVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}
}
