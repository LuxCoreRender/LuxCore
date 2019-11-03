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

#if defined(_OPENMP)
#include <omp.h>
#endif

#include <boost/format.hpp>
#include <boost/circular_buffer.hpp>

#include "luxrays/core/geometry/bbox.h"
#include "slg/samplers/sobol.h"
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
#include "slg/lights/strategies/dlscacheimpl/dlscoctree.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// DLSCOctree
//------------------------------------------------------------------------------

DLSCOctree::DLSCOctree(const vector<DLSCVisibilityParticle> &entries,
		const BBox &bbox, const float r, const float n, const u_int md) :
	IndexOctree(entries, bbox, r, n, md) {
}

DLSCOctree::~DLSCOctree() {
}

u_int DLSCOctree::GetNearestEntry(const Point &p, const Normal &n, const bool isVolume) const {
	u_int nearestEntryIndex = NULL_INDEX;
	float nearestDistance2 = entryRadius2;

	GetNearestEntryImpl(&root, worldBBox, p, n, isVolume,
			nearestEntryIndex, nearestDistance2);
	
	return nearestEntryIndex;
}

void DLSCOctree::GetNearestEntryImpl(const IndexOctreeNode *node, const BBox &nodeBBox,
		const Point &p, const Normal &n, const bool isVolume,
		u_int &nearestEntryIndex, float &nearestDistance2) const {
	// Check if I'm inside the node bounding box
	if (!nodeBBox.Inside(p))
		return;

	// Check every entry in this node
	for (auto const &entryIndex : node->entriesIndex) {
		const DLSCVisibilityParticle &entry = allEntries[entryIndex];

		const BSDF &bsdf = entry.bsdfList[0];
		const Normal landingNormal = bsdf.hitPoint.GetLandingShadeN();
		const float distance2 = DistanceSquared(p, bsdf.hitPoint.p);
		if ((distance2 < nearestDistance2) && (isVolume == bsdf.IsVolume()) &&
				(bsdf.IsVolume() || (Dot(n, landingNormal) >= entryNormalCosAngle))) {
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

			GetNearestEntryImpl(node->children[child], childBBox, p, n, isVolume,
					nearestEntryIndex, nearestDistance2);
		}
	}
}
