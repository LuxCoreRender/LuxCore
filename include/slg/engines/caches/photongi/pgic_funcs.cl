#line 2 "pgic_funcs.cl"

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

#if defined(PARAM_PGIC_ENABLED)

OPENCL_FORCE_INLINE bool PhotonGICache_IsDirectLightHitVisible(
		const bool causticCacheAlreadyUsed) {
#if !defined(PARAM_PGIC_CAUSTIC_ENABLED)
	return true;
#else
#if defined(PARAM_PGIC_DEBUG_NONE)
	return !causticCacheAlreadyUsed;
#else
	return false;
#endif
#endif
}

OPENCL_FORCE_INLINE bool PhotonGICache_IsPhotonGIEnabled(__global BSDF *bsdf,
		const float glossinessUsageThreshold
		MATERIALS_PARAM_DECL) {
	const uint matIndex = bsdf->materialIndex;

	if (matIndex == NULL_INDEX)
		return false;
	else {
		const MaterialType materialType = mats[matIndex].type;
		
		if (((materialType == GLOSSY2) && (BSDF_GetGlossiness(bsdf MATERIALS_PARAM) >= glossinessUsageThreshold)) ||
				(materialType == MATTE) ||
				(materialType == ROUGHMATTE))
			return BSDF_IsPhotonGIEnabled(bsdf MATERIALS_PARAM);
		else
			return false;
	}
}

OPENCL_FORCE_INLINE float PhotonGICache_GetIndirectUsageThreshold(
		const BSDFEvent lastBSDFEvent, const float lastGlossiness,
		const float pgicIndirectGlossinessUsageThreshold,
		const float pgicIndirectUsageThresholdScale,
		const float pgicIndirectLookUpRadius) {
	// Decide if the glossy surface is "nearly specular"

	if ((lastBSDFEvent & GLOSSY) && (lastGlossiness < pgicIndirectGlossinessUsageThreshold)) {
		// Disable the cache, the surface is "nearly specular"
		return INFINITY;
	} else
		return pgicIndirectUsageThresholdScale * pgicIndirectLookUpRadius;
}

OPENCL_FORCE_INLINE __global const RadiancePhoton* restrict RadiancePhotonsBVH_GetNearestEntry(
		__global const RadiancePhoton* restrict pgicRadiancePhotons,
		__global const IndexBVHArrayNode* restrict pgicRadiancePhotonsBVHNodes,
		const float pgicIndirectLookUpRadius2, const float pgicIndirectLookUpNormalCosAngle,
		const float3 p, const float3 n) {
	__global const RadiancePhoton* restrict nearestEntry = NULL;
#if defined(PARAM_PGIC_INDIRECT_ENABLED)
	float nearestDistance2 = pgicIndirectLookUpRadius2;

	uint currentNode = 0; // Root Node
	const uint stopNode = IndexBVHNodeData_GetSkipIndex(pgicRadiancePhotonsBVHNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		__global const IndexBVHArrayNode* restrict node = &pgicRadiancePhotonsBVHNodes[currentNode];

		const uint nodeData = node->nodeData;
		if (IndexBVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			__global const RadiancePhoton* restrict entry = &pgicRadiancePhotons[node->entryLeaf.entryIndex];

			const float distance2 = DistanceSquared(p, VLOAD3F(&entry->p.x));
			if ((distance2 < nearestDistance2) &&
					(dot(n, VLOAD3F(&entry->n.x)) > pgicIndirectLookUpNormalCosAngle)) {
				// I have found a valid nearer entry
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
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}
#endif

	return nearestEntry;
}

OPENCL_FORCE_INLINE float3 PhotonGICache_GetIndirectRadiance(__global BSDF *bsdf,
		__global const RadiancePhoton* restrict pgicRadiancePhotons,
		__global const IndexBVHArrayNode* restrict pgicRadiancePhotonsBVHNodes,
		const float pgicIndirectLookUpRadius2, const float pgicIndirectLookUpNormalCosAngle) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);
	const float3 n = (bsdf->hitPoint.intoObject ? 1.f: -1.f) * VLOAD3F(&bsdf->hitPoint.shadeN.x);
	__global const RadiancePhoton* restrict radiancePhoton = RadiancePhotonsBVH_GetNearestEntry(
			pgicRadiancePhotons, pgicRadiancePhotonsBVHNodes,
			pgicIndirectLookUpRadius2, pgicIndirectLookUpNormalCosAngle,
			p, n);

	if (radiancePhoton)
		return VLOAD3F(&radiancePhoton->outgoingRadiance.c[0]);
	else
		return BLACK;
}

#endif
