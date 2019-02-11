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

#include <embree3/rtcore.h>
#include <embree3/rtcore_builder.h>

#include "luxrays/core/bvh/bvhbuild.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PGICPhotonBvh
//------------------------------------------------------------------------------

PGICPhotonBvh::PGICPhotonBvh(const vector<Photon> &entries, const u_int maxLookUpCount,
		const float radius, const float normalAngle) :
		IndexBvh(entries, radius), entryMaxLookUpCount(maxLookUpCount),
		entryNormalCosAngle(cosf(Radians(normalAngle))) {
}

PGICPhotonBvh::~PGICPhotonBvh() {
}

void PGICPhotonBvh::GetAllNearEntries(vector<NearPhoton> &entries,
		const Point &p, const Normal &n, float &maxDistance2) const {
	maxDistance2 = entryRadius2;

	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(arrayNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		const IndexBVHArrayNode &node = arrayNodes[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const Photon *entry = &allEntries[node.entryLeaf.index];

			const float distance2 = DistanceSquared(p, entry->p);
			if ((distance2 < maxDistance2) &&
					(Dot(n, -entry->d) > DEFAULT_COS_EPSILON_STATIC) &&
					(Dot(n, entry->landingSurfaceNormal) > entryNormalCosAngle)) {
				// I have found a valid entry

				NearPhoton nearPhoton(entry, distance2);

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

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (p.x >= node.bvhNode.bboxMin[0] && p.x <= node.bvhNode.bboxMax[0] &&
					p.y >= node.bvhNode.bboxMin[1] && p.y <= node.bvhNode.bboxMax[1] &&
					p.z >= node.bvhNode.bboxMin[2] && p.z <= node.bvhNode.bboxMax[2])
				++currentNode;
			else {
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}
}

//------------------------------------------------------------------------------
// PGICRadiancePhotonBvh
//------------------------------------------------------------------------------

PGICRadiancePhotonBvh::PGICRadiancePhotonBvh(const vector<RadiancePhoton> &entries,
		const float radius, const float normalAngle) :
		IndexBvh(entries, radius), entryNormalCosAngle(cosf(Radians(normalAngle))) {
}

PGICRadiancePhotonBvh::~PGICRadiancePhotonBvh() {
}

const RadiancePhoton *PGICRadiancePhotonBvh::GetNearestEntry(const Point &p, const Normal &n) const {
	const RadiancePhoton *nearestEntry = nullptr;
	float nearestDistance2 = entryRadius2;

	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(arrayNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		const IndexBVHArrayNode &node = arrayNodes[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const RadiancePhoton *entry = &allEntries[node.entryLeaf.index];

			const float distance2 = DistanceSquared(p, entry->p);
			if ((distance2 < nearestDistance2) &&
					(Dot(n, entry->n) > entryNormalCosAngle)) {
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
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}

	return nearestEntry;
}
