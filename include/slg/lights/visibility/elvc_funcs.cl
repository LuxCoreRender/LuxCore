#line 2 "elvc_funcs.cl"

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

OPENCL_FORCE_INLINE bool EnvLightVisibilityCache_IsCacheEnabled(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return !BSDF_IsDelta(bsdf MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE __global const ELVCacheEntry* restrict EnvLightVisibilityCache_GetNearestEntry(
		__global const ELVCacheEntry* restrict elvcAllEntries,
		__global const float* restrict elvcDistributions,
		__global const IndexBVHArrayNode* restrict elvcBVHNodes,
		const float elvcRadius2, const float elvcNormalCosAngle,
		const float3 p, const float3 n, const bool isVolume
		) {
	__global const ELVCacheEntry* restrict nearestEntry = NULL;
	float nearestDistance2 = elvcRadius2;

	uint currentNode = 0; // Root Node
	const uint stopNode = IndexBVHNodeData_GetSkipIndex(elvcBVHNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		__global const IndexBVHArrayNode* restrict node = &elvcBVHNodes[currentNode];

		const uint nodeData = node->nodeData;
		if (IndexBVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			__global const ELVCacheEntry* restrict entry = &elvcAllEntries[node->entryLeaf.entryIndex];

			const float distance2 = DistanceSquared(p, VLOAD3F(&entry->p.x));
			if ((distance2 <= nearestDistance2) &&
					(isVolume == entry->isVolume) && 
					(isVolume ||
					(dot(n, VLOAD3F(&entry->n.x)) >= elvcNormalCosAngle)
					)
				) {
				// I have found a valid entry

				nearestEntry = entry;
				nearestDistance2 = distance2;
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (p.x >= node->bvhNode.bboxMin[0] && p.x <= node->bvhNode.bboxMax[0] &&
					p.y >= node->bvhNode.bboxMin[1] && p.y <= node->bvhNode.bboxMax[1] &&
					p.z >= node->bvhNode.bboxMin[2] && p.z <= node->bvhNode.bboxMax[2])
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

OPENCL_FORCE_INLINE __global const float* restrict EnvLightVisibilityCache_GetVisibilityMap(__global const BSDF *bsdf
		LIGHTS_PARAM_DECL) {
	__global const ELVCacheEntry* restrict cacheEntry = EnvLightVisibilityCache_GetNearestEntry(
			elvcAllEntries, elvcDistributions, elvcBVHNodes, elvcRadius2, elvcNormalCosAngle,
			VLOAD3F(&bsdf->hitPoint.p.x), BSDF_GetLandingShadeN(bsdf), bsdf->isVolume
			);

	return (cacheEntry && (cacheEntry->distributionOffset != NULL_INDEX)) ? &elvcDistributions[cacheEntry->distributionOffset] : NULL;
}
