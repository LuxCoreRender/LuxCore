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

#include <embree3/rtcore.h>
#include <embree3/rtcore_builder.h>

#include "luxrays/core/bvh/bvhbuild.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/bsdf/bsdf.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PGICPhotonBvh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::PGICPhotonBvh)

PGICPhotonBvh::PGICPhotonBvh(const vector<Photon> *entries, const u_int count,
		const float radius, const float normalAngle) :
		IndexBvh(entries, radius), entryNormalCosAngle(cosf(Radians(normalAngle))),
		photonTracedCount(count) {
}

PGICPhotonBvh::~PGICPhotonBvh() {
}

SpectrumGroup PGICPhotonBvh::ConnectCacheEntry(const Photon &photon, const BSDF &bsdf) const {
	BSDFEvent event;
	float directPdfW;
	Spectrum bsdfEval = bsdf.Evaluate(-photon.d, &event, &directPdfW, nullptr);
	// bsdf.Evaluate() multiplies the result by AbsDot(bsdf.hitPoint.shadeN, -photon->d)
	// so I have to cancel that factor. It is already included in photon density
	// estimation.
	if (!bsdf.IsVolume())
		bsdfEval /= AbsDot(bsdf.hitPoint.shadeN, -photon.d);
	else
		bsdfEval /= directPdfW;

	SpectrumGroup result;
	result.Add(photon.lightID, photon.alpha * bsdfEval);

	return result;
}

SpectrumGroup PGICPhotonBvh::ConnectAllNearEntries(const BSDF &bsdf) const {
	const Point &p = bsdf.hitPoint.p;
	// Flip the normal if required
	const Normal n = (bsdf.hitPoint.intoObject ? 1.f: -1.f) * bsdf.hitPoint.geometryN;
	const bool isVolume = bsdf.IsVolume();

	SpectrumGroup result;

	u_int currentNode = 0; // Root Node
	const u_int stopNode = IndexBVHNodeData_GetSkipIndex(arrayNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		const luxrays::ocl::IndexBVHArrayNode &node = arrayNodes[currentNode];

		const u_int nodeData = node.nodeData;
		if (IndexBVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const Photon &entry = ((*allEntries)[node.entryLeaf.entryIndex]);

			const float distance2 = DistanceSquared(p, entry.p);
			if ((distance2 < entryRadius2) && (entry.isVolume == isVolume) &&
					(isVolume ||
						((Dot(n, -entry.d) > DEFAULT_COS_EPSILON_STATIC) &&
						(Dot(n, entry.landingSurfaceNormal) > entryNormalCosAngle)))) {
				// I have found a valid entry

				result += ConnectCacheEntry(entry, bsdf);
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

	if (isVolume)
		result /= photonTracedCount * (4.f / 3.f * M_PI * entryRadius2 * entryRadius);
	else
		result /= photonTracedCount * (M_PI * entryRadius2);

	return result;
}

//------------------------------------------------------------------------------
// PGICRadiancePhotonBvh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::PGICRadiancePhotonBvh)

PGICRadiancePhotonBvh::PGICRadiancePhotonBvh(const vector<RadiancePhoton> *entries,
		const float radius, const float normalAngle) :
		IndexBvh(entries, radius), entryNormalCosAngle(cosf(Radians(normalAngle))) {
}

PGICRadiancePhotonBvh::~PGICRadiancePhotonBvh() {
}

const RadiancePhoton *PGICRadiancePhotonBvh::GetNearestEntry(const Point &p, const Normal &n,
		const bool isVolume) const {
	const RadiancePhoton *nearestEntry = nullptr;
	float nearestDistance2 = entryRadius2;

	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(arrayNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		const luxrays::ocl::IndexBVHArrayNode &node = arrayNodes[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const RadiancePhoton *entry = &((*allEntries)[node.entryLeaf.entryIndex]);

			const float distance2 = DistanceSquared(p, entry->p);
			if ((distance2 < nearestDistance2) && (entry->isVolume == isVolume) &&
					(isVolume || (Dot(n, entry->n) > entryNormalCosAngle))) {
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
