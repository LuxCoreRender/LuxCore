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
		const bool causticCacheAlreadyUsed, const BSDFEvent lastBSDFEvent,
		__global const PathDepthInfo *depthInfo) {
	// This is a specific check to cut fireflies created by some glossy or
	// specular bounce
	if (!(lastBSDFEvent & DIFFUSE) && (depthInfo->diffuseDepth > 0))
		return false;
#if !defined(PARAM_PGIC_CAUSTIC_ENABLED)
	return true;
#endif
#if defined(PARAM_PGIC_DEBUG_NONE)
	if (!causticCacheAlreadyUsed || !(lastBSDFEvent & SPECULAR))
		return true;
#endif

	return false;
}

OPENCL_FORCE_INLINE bool PhotonGICache_IsPhotonGIEnabled(__global BSDF *bsdf,
		const float glossinessUsageThreshold
		MATERIALS_PARAM_DECL) {
	const uint matIndex = bsdf->materialIndex;

	if (matIndex == NULL_INDEX)
		return false;
	else {
		const BSDFEvent eventTypes = BSDF_GetEventTypes(bsdf
				MATERIALS_PARAM);

		if ((eventTypes & TRANSMIT) || (eventTypes & SPECULAR) ||
				((eventTypes & GLOSSY) && (BSDF_GetGlossiness(bsdf MATERIALS_PARAM) < glossinessUsageThreshold)))
			return false;
		else
			return BSDF_IsPhotonGIEnabled(bsdf MATERIALS_PARAM);
	}
}

//------------------------------------------------------------------------------
// Indirect cache related functions
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float PhotonGICache_GetIndirectUsageThreshold(
		const BSDFEvent lastBSDFEvent, const float lastGlossiness,
		const float u0,
		const float pgicIndirectGlossinessUsageThreshold,
		const float pgicIndirectUsageThresholdScale,
		const float pgicIndirectLookUpRadius) {
	// Decide if the glossy surface is "nearly specular"

	if ((lastBSDFEvent & GLOSSY) && (lastGlossiness < pgicIndirectGlossinessUsageThreshold)) {
		// Disable the cache, the surface is "nearly specular"
		return INFINITY;
	} else {
		// Use a larger blend zone for glossy surface
		const float scale = (lastBSDFEvent & GLOSSY) ? 2.f : 1.f;

		// Enable the cache for diffuse or glossy "nearly diffuse" but only after
		// the threshold (before I brute force and cache between 0x and 1x the threshold)
		return scale * u0 * pgicIndirectUsageThresholdScale * pgicIndirectLookUpRadius;
	}
}

OPENCL_FORCE_INLINE __global const RadiancePhoton* restrict RadiancePhotonsBVH_GetNearestEntry(
		__global const RadiancePhoton* restrict pgicRadiancePhotons,
		__global const IndexBVHArrayNode* restrict pgicRadiancePhotonsBVHNodes,
		const float pgicIndirectLookUpRadius2, const float pgicIndirectLookUpNormalCosAngle,
		const float3 p, const float3 n
#if defined(PARAM_HAS_VOLUMES)
		, const bool isVolume
#endif
		) {
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
#if defined(PARAM_HAS_VOLUMES)
					(entry->isVolume == isVolume) &&
						(isVolume ||
#endif
						(dot(n, VLOAD3F(&entry->n.x)) > pgicIndirectLookUpNormalCosAngle)
#if defined(PARAM_HAS_VOLUMES)
						)
#endif
					) {
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
				// I don't need to use IndexBVHNodeData_GetSkipIndex() here because
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
	const float3 n = (bsdf->hitPoint.intoObject ? 1.f: -1.f) * VLOAD3F(&bsdf->hitPoint.geometryN.x);

	__global const RadiancePhoton* restrict radiancePhoton = RadiancePhotonsBVH_GetNearestEntry(
			pgicRadiancePhotons, pgicRadiancePhotonsBVHNodes,
			pgicIndirectLookUpRadius2, pgicIndirectLookUpNormalCosAngle,
			p, n
#if defined(PARAM_HAS_VOLUMES)
			, bsdf->isVolume
#endif
			);

	if (radiancePhoton)
		return VLOAD3F(&radiancePhoton->outgoingRadiance.c[0]);
	else
		return BLACK;
}

//------------------------------------------------------------------------------
// Caustic cache related functions
//------------------------------------------------------------------------------

// From https://www.randygaul.net/2016/12/28/binary-heaps-in-c/

OPENCL_FORCE_INLINE void CausticPhotonsBVH_Cascade(__global NearPhoton *items,
		const uint N, uint i) {
	uint max = i;
	NearPhoton maxVal = items[i];
	uint i2 = 2 * i;

	while (i2 < N) {
		uint left = i2;
		uint right = i2 + 1;

		if (items[left].distance2 > maxVal.distance2)
			max = left;
		if ((right < N) && (items[right].distance2 > items[max].distance2))
			max = right;
		if (max == i)
			break;

		items[i] = items[max];
		items[max] = maxVal;
		i = max;
		i2 = 2 * i;
	}
}

OPENCL_FORCE_INLINE void CausticPhotonsBVH_MakeHeap(__global NearPhoton *items, uint count) {
	for (uint i = count / 2; i > 0; --i)
		CausticPhotonsBVH_Cascade(items, count, i);
}

OPENCL_FORCE_INLINE NearPhoton CausticPhotonsBVH_PopHeap(__global NearPhoton *items, uint *count) {
	NearPhoton root = items[0];
	items[0] = items[*count - 1];
	CausticPhotonsBVH_Cascade(items, *count - 1, 0);

	*count -= 1;
	
	return root;
}

OPENCL_FORCE_INLINE void CausticPhotonsBVH_PushHeap(__global NearPhoton *items,
		uint *count, const uint newPhotonIndex, const float newDistance2) {
	uint i = *count;
	items[*count].photonIndex = newPhotonIndex;
	items[*count].distance2 = newDistance2;

	while (i) {
		uint j = i / 2;
		NearPhoton child = items[i];
		NearPhoton parent = items[j];

		if (child.distance2 > parent.distance2) {
			items[i] = parent;
			items[j] = child;
		}

		i = j;
	}
	
	*count += 1;
}

OPENCL_FORCE_INLINE void CausticPhotonsBVH_GetAllNearEntries(
		__global NearPhoton *entries,
		uint *entriesSize,
		__global const Photon* restrict pgicCausticPhotons,
		__global const IndexBVHArrayNode* restrict pgicCausticPhotonsBVHNodes,
		const float pgicCausticLookUpRadius2, const float pgicCausticLookUpNormalCosAngle,
		const uint pgicCausticLookUpMaxCount,
		const float3 p, const float3 n,
#if defined(PARAM_HAS_VOLUMES)
		const bool isVolume,
#endif
		float *maxDistance2) {
	*entriesSize = 0;
	
#if defined(PARAM_PGIC_CAUSTIC_ENABLED)
	*maxDistance2 = pgicCausticLookUpRadius2;

	uint currentNode = 0; // Root Node
	const uint stopNode = IndexBVHNodeData_GetSkipIndex(pgicCausticPhotonsBVHNodes[0].nodeData); // Non-existent

	while (currentNode < stopNode) {
		__global const IndexBVHArrayNode* restrict node = &pgicCausticPhotonsBVHNodes[currentNode];

		const uint nodeData = node->nodeData;
		if (IndexBVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the entry
			const uint entryIndex = node->entryLeaf.entryIndex;
			__global const Photon* entry = &pgicCausticPhotons[entryIndex];

			const float distance2 = DistanceSquared(p, VLOAD3F(&entry->p.x));
			if ((distance2 < *maxDistance2) &&
#if defined(PARAM_HAS_VOLUMES)
					(entry->isVolume == isVolume) &&
						(isVolume ||
#endif
						((dot(n, -VLOAD3F(&entry->d.x)) > DEFAULT_COS_EPSILON_STATIC) &&
						(dot(n, VLOAD3F(&entry->landingSurfaceNormal.x)) > pgicCausticLookUpNormalCosAngle))
#if defined(PARAM_HAS_VOLUMES)
					)
#endif
					) {
				// I have found a valid entry

				if (*entriesSize < pgicCausticLookUpMaxCount) {
					// Just add the entry
					__global NearPhoton *nearPhoton = &entries[*entriesSize];
					nearPhoton->photonIndex = entryIndex;
					nearPhoton->distance2 = distance2;
					*entriesSize += 1;

					// Check if the array is now full and sort the entries for
					// the next addition
					if (*entriesSize == pgicCausticLookUpMaxCount)
						CausticPhotonsBVH_MakeHeap(&entries[0], *entriesSize);
				} else {
					// Check if the new entry is nearer than the farthest array entry
					if (distance2 < entries[0].distance2) {
						// Remove the farthest array entry
						CausticPhotonsBVH_PopHeap(&entries[0], entriesSize);
						// Add the new entry
						CausticPhotonsBVH_PushHeap(&entries[0], entriesSize, entryIndex, distance2);

						// Update max. squared distance
						*maxDistance2 = entries[0].distance2;
					}
				}
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
#endif
}

// Simpson filter from PBRT v2. Filter the photons according their
// distance, giving more weight to the nearest.

OPENCL_FORCE_INLINE float SimpsonKernel(const float3 p1, const float3 p2,
		const float maxDist2) {
	const float dist2 = DistanceSquared(p1, p2);

	// The distance between p1 and p2 is supposed to be < maxDist2
    const float s = (1.f - dist2 / maxDist2);

    return 3.f * M_1_PI_F * s * s;
}

OPENCL_FORCE_NOT_INLINE float3 PhotonGICache_ProcessCacheEntries(
		__global NearPhoton *entries, const uint entriesSize,
		__global const Photon* restrict photons, const uint photonTracedCount,
		const float maxDistance2, __global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	float3 result = BLACK;

	if (entriesSize > 0) {
		// Generic path

		BSDFEvent event;
		for (uint i = 0; i < entriesSize; ++i) {
			__global NearPhoton *entry = &entries[i];
			__global const Photon *photon = &photons[entry->photonIndex];

			// bsdf.Evaluate() multiplies the result by AbsDot(bsdf.hitPoint.shadeN, -photon->d)
			// so I have to cancel that factor. It is already included in photon density
			// estimation.
			const float3 bsdfEval = BSDF_Evaluate(bsdf, -VLOAD3F(&photon->d.x), &event, NULL MATERIALS_PARAM) /
					fabs(dot(VLOAD3F(&bsdf->hitPoint.shadeN.x), -VLOAD3F(&photon->d.x)));

			// Using a Simpson filter here
			result += SimpsonKernel(VLOAD3F(&bsdf->hitPoint.p.x), VLOAD3F(&photon->p.x), maxDistance2) *
					bsdfEval *
					VLOAD3F(photon->alpha.c);
		}
		result /= photonTracedCount * maxDistance2;
	}
	
	return result;
}

OPENCL_FORCE_NOT_INLINE float3 PhotonGICache_GetCausticRadiance(__global BSDF *bsdf,
		__global const Photon* restrict pgicCausticPhotons,
		__global const IndexBVHArrayNode* restrict pgicCausticPhotonsBVHNodes,
		__global NearPhoton *pgicCausticNearPhotons,
		const uint pgicCausticPhotonTracedCount,
		const float pgicCausticLookUpRadius2, const float pgicCausticLookUpNormalCosAngle,
		const uint pgicCausticLookUpMaxCount
		MATERIALS_PARAM_DECL) {
	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);
	const float3 n = (bsdf->hitPoint.intoObject ? 1.f: -1.f) * VLOAD3F(&bsdf->hitPoint.geometryN.x);

	uint nearPhotonsCount;
	float maxDistance2;
	CausticPhotonsBVH_GetAllNearEntries(pgicCausticNearPhotons, &nearPhotonsCount,
			pgicCausticPhotons, pgicCausticPhotonsBVHNodes,
			pgicCausticLookUpRadius2, pgicCausticLookUpNormalCosAngle,
			pgicCausticLookUpMaxCount,
			p, n,
#if defined(PARAM_HAS_VOLUMES)
			bsdf->isVolume,
#endif
			&maxDistance2);

	return PhotonGICache_ProcessCacheEntries(pgicCausticNearPhotons, nearPhotonsCount,
			pgicCausticPhotons,	pgicCausticPhotonTracedCount,
			maxDistance2, bsdf
			MATERIALS_PARAM);
}

#endif
