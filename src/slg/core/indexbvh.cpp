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

#include <algorithm>

#include "luxrays/core/bvh/bvhbuild.h"
#include "slg/core/indexbvh.h"

// Required for explicit instantiations
#include "slg/engines/caches/photongi/photongicache.h"
// Required for explicit instantiations
#include "slg/lights/strategies/dlscacheimpl/dlscacheimpl.h"
// Required for explicit instantiations
#include "slg/lights/visibility/envlightvisibilitycache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BuildEmbreeBVH
//------------------------------------------------------------------------------

template<u_int CHILDREN_COUNT, class T> static luxrays::ocl::IndexBVHArrayNode *BuildEmbreeBVH(
		RTCBuildQuality quality, const vector<T> *allEntries,
		const float entryRadius, u_int *nNodes) {
	//const double t1 = WallClockTime();

	// Initialize RTCPrimRef vector
	vector<RTCBuildPrimitive> prims(allEntries->size());
	for (u_int i = 0; i < prims.size(); ++i) {
		RTCBuildPrimitive &prim = prims[i];
		const T &entry = (*allEntries)[i];

		prim.lower_x = entry.p.x - entryRadius;
		prim.lower_y = entry.p.y - entryRadius;
		prim.lower_z = entry.p.z - entryRadius;
		prim.geomID = 0;

		prim.upper_x = entry.p.x + entryRadius;
		prim.upper_y = entry.p.y + entryRadius;
		prim.upper_z = entry.p.z + entryRadius;
		prim.primID = i;
	}

	//const double t2 = WallClockTime();
	//cout << "BuildEmbreeBVH preprocessing time: " << int((t2 - t1) * 1000) << "ms\n";

	return luxrays::buildembreebvh::BuildEmbreeBVH<CHILDREN_COUNT>(quality, prims, nNodes);
}

//------------------------------------------------------------------------------
// IndexBvh
//------------------------------------------------------------------------------

template <class T>
IndexBvh<T>::IndexBvh() : arrayNodes(nullptr) {
}

template <class T>
IndexBvh<T>::IndexBvh(const vector<T> *entries, const float radius) :
		allEntries(entries), entryRadius(radius), entryRadius2(radius * radius) {
	arrayNodes = BuildEmbreeBVH<4, T>(RTC_BUILD_QUALITY_HIGH, allEntries, entryRadius, &nNodes);
}

template <class T>
IndexBvh<T>::~IndexBvh() {
	delete [] arrayNodes;
}

//------------------------------------------------------------------------------
// Explicit instantiations
//------------------------------------------------------------------------------

// C++ can be quite horrible...

namespace slg {
template class IndexBvh<Photon>;
template class IndexBvh<RadiancePhoton>;
template class IndexBvh<DLSCacheEntry>;
template class IndexBvh<ELVCacheEntry>;
}

BOOST_CLASS_EXPORT_IMPLEMENT(slg::IndexBvh<Photon>)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::IndexBvh<RadiancePhoton>)
// Serialization for DLSCacheEntry is still TODO
