#line 2 "dlsc_funcs.cl"

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

OPENCL_FORCE_INLINE bool DLSCacheEntry_IsDirectLightSamplingDisabled(__global const DLSCacheEntry* restrict cacheEntry) {
	return (cacheEntry->lightsDistributionOffset == NULL_INDEX);
}

OPENCL_FORCE_INLINE __global const DLSCacheEntry* restrict DLSCache_GetEntry(
		__global const DLSCacheEntry* restrict dlscAllEntries,
		__global const uint* restrict dlscDistributionIndexToLightIndex,
		__global const float* restrict dlscDistributions,
		__global const IndexBVHArrayNode* restrict dlscBVHNodes,
		const float dlscRadius2, const float dlscNormalCosAngle,
		const float3 p, const float3 n
#if defined(PARAM_HAS_VOLUMES)
		, const bool isVolume
#endif
		) {
	uint currentNode = 0; // Root Node
	const uint stopNode = IndexBVHNodeData_GetSkipIndex(dlscBVHNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		__global const IndexBVHArrayNode* restrict node = &dlscBVHNodes[currentNode];

		const uint nodeData = node->nodeData;
		if (IndexBVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			__global const DLSCacheEntry* restrict entry = &dlscAllEntries[node->entryLeaf.entryIndex];

			if ((DistanceSquared(p, VLOAD3F(&entry->p.x)) <= dlscRadius2) &&
#if defined(PARAM_HAS_VOLUMES)
					(isVolume == entry->isVolume) && 
					(isVolume ||
#endif
					(dot(n, VLOAD3F(&entry->n.x)) >= dlscNormalCosAngle)
#if defined(PARAM_HAS_VOLUMES)
					)
#endif
					) {
				// I have found a valid entry
				return entry;
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

	return NULL;
}
