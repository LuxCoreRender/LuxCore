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

#include "luxrays/core/epsilon.h"
#include "slg/core/indexoctree.h"

// Required for explicit instantiations
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
// Required for explicit instantiations
#include "slg/engines/caches/photongi/photongicache.h"
// Required for explicit instantiations
#include "slg/lights/visibility/envlightvisibilitycache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// IndexOctree
//------------------------------------------------------------------------------

template <class T>
IndexOctree<T>::IndexOctree(const vector<T> &entries,
		const BBox &bbox, const float r, const float normAngle, const u_int md) :
	allEntries(entries), worldBBox(bbox), maxDepth(md), entryRadius(r), entryRadius2(r * r),
	entryNormalCosAngle(cosf(Radians(normAngle))) {
	worldBBox.Expand(MachineEpsilon::E(worldBBox));
}

template <class T>
IndexOctree<T>::~IndexOctree() {
}

template <class T>
void IndexOctree<T>::Add(const u_int entryIndex) {
	const T &entry = allEntries[entryIndex];

	const Vector entryRadiusVector(entryRadius, entryRadius, entryRadius);
	const BBox entryBBox(entry.p - entryRadiusVector, entry.p + entryRadiusVector);

	AddImpl(&root, worldBBox, entryIndex, entryBBox, DistanceSquared(entryBBox.pMin,  entryBBox.pMax));
}

template <class T>
BBox IndexOctree<T>::ChildNodeBBox(u_int child, const BBox &nodeBBox,
	const Point &pMid) const {
	BBox childBound;

	childBound.pMin.x = (child & 0x4) ? pMid.x : nodeBBox.pMin.x;
	childBound.pMax.x = (child & 0x4) ? nodeBBox.pMax.x : pMid.x;
	childBound.pMin.y = (child & 0x2) ? pMid.y : nodeBBox.pMin.y;
	childBound.pMax.y = (child & 0x2) ? nodeBBox.pMax.y : pMid.y;
	childBound.pMin.z = (child & 0x1) ? pMid.z : nodeBBox.pMin.z;
	childBound.pMax.z = (child & 0x1) ? nodeBBox.pMax.z : pMid.z;

	return childBound;
}

template <class T>
void IndexOctree<T>::AddImpl(IndexOctreeNode *node, const BBox &nodeBBox,
	const u_int entryIndex, const BBox &entryBBox,
	const float entryBBoxDiagonal2, const u_int depth) {
	// Check if I have to store the entry in this node
	if ((depth == maxDepth) ||
			DistanceSquared(nodeBBox.pMin, nodeBBox.pMax) < entryBBoxDiagonal2) {
		node->entriesIndex.push_back(entryIndex);
		return;
	}

	// Determine which children the item overlaps
	const Point pMid = .5 * (nodeBBox.pMin + nodeBBox.pMax);

	const bool x[2] = {
		entryBBox.pMin.x <= pMid.x,
		entryBBox.pMax.x > pMid.x
	};
	const bool y[2] = {
		entryBBox.pMin.y <= pMid.y,
		entryBBox.pMax.y > pMid.y
	};
	const bool z[2] = {
		entryBBox.pMin.z <= pMid.z,
		entryBBox.pMax.z > pMid.z
	};

	const bool overlap[8] = {
		bool(x[0] & y[0] & z[0]),
		bool(x[0] & y[0] & z[1]),
		bool(x[0] & y[1] & z[0]),
		bool(x[0] & y[1] & z[1]),
		bool(x[1] & y[0] & z[0]),
		bool(x[1] & y[0] & z[1]),
		bool(x[1] & y[1] & z[0]),
		bool(x[1] & y[1] & z[1])
	};

	for (u_int child = 0; child < 8; ++child) {
		if (!overlap[child])
			continue;

		// Allocated the child node if required
		if (!node->children[child])
			node->children[child] = new IndexOctreeNode();

		// Add the entry to each overlapping child
		const BBox childBBox = ChildNodeBBox(child, nodeBBox, pMid);
		AddImpl(node->children[child], childBBox,
				entryIndex, entryBBox, entryBBoxDiagonal2, depth + 1);
	}
}

//------------------------------------------------------------------------------
// Explicit instantiations
//------------------------------------------------------------------------------

// C++ can be quite horrible...

namespace slg {
template class IndexOctree<DLSCacheEntry>;
template class IndexOctree<PGICVisibilityParticle>;
template class IndexOctree<ELVCVisibilityParticle>;
}
