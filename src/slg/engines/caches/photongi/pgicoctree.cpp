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
// PGCIOctree
//------------------------------------------------------------------------------

PGCIOctree::PGCIOctree(const vector<VisibilityParticle> &entries,
		const BBox &bbox, const float r, const float normAngle, const u_int md) :
	IndexOctree(entries, bbox, r, normAngle, md) {
}

PGCIOctree::~PGCIOctree() {
}

u_int PGCIOctree::GetNearestEntry(const Point &p, const Normal &n) const {
	u_int nearestEntryIndex = NULL_INDEX;
	float nearestDistance2 = numeric_limits<float>::infinity();

	GetNearestEntryImpl(&root, worldBBox, p, n, nearestEntryIndex, nearestDistance2);
	
	return nearestEntryIndex;
}

void PGCIOctree::GetNearestEntryImpl(const IndexOctreeNode *node, const BBox &nodeBBox,
		const Point &p, const Normal &n,
		u_int &nearestEntryIndex, float &nearestDistance2) const {
	// Check if I'm inside the node bounding box
	if (!nodeBBox.Inside(p))
		return;

	// Check every entry in this node
	for (auto const &entryIndex : node->entriesIndex) {
		const VisibilityParticle &entry = allEntries[entryIndex];

		const float distance2 = DistanceSquared(p, entry.p);
		if ((distance2 < nearestDistance2) &&
				(distance2 <= entryRadius2) &&
				(Dot(n, entry.n) >= entryNormalCosAngle)) {
			// I have found a valid nearer entry
			nearestEntryIndex = entryIndex;
			nearestDistance2 = distance2;
		}
	}

	// Check the children too
	const Point pMid = .5 * (nodeBBox.pMin + nodeBBox.pMax);
	for (u_int child = 0; child < 8; ++child) {
		if (node->children[child]) {
			const BBox childBBox = ChildNodeBBox(child, nodeBBox, pMid);

			GetNearestEntryImpl(node->children[child], childBBox,
					p, n, nearestEntryIndex, nearestDistance2);
		}
	}
}
